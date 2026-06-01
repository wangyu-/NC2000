#include "comm.h"
#include "dsp/dsp.h"
#include "state.h"
#include <SDL2/SDL.h>

extern nc2k_states_t nc2k_states;

static SDL_AudioDeviceID g_audio_device = 0;
static FILE *audio_dump_fp; //for dump data
/*
=============
beeper
=============
*/
struct BeeperSignal{
    long long cycle;
    int value;
};

static BeeperSignal last_beeper{};
static deque<signed short> sound_stream_beeper;
/*buffer to SDL_QueueAudio */
//static vector<signed short> beeper_buffer; 

/*filter out DC signal*/
double filter_beeper(double in){
	static double cuttmp=-8000;
	static double cutoff=2.0*3.141592654*40/BEEPER_AUDIO_HZ;
	double val=in-cuttmp;
	cuttmp+=cutoff*val;
	return val;
}
/*
double filter_dsp(double in){
	static double cuttmp=0;
	static double cutoff=2.0*3.141592654*40/DSP_AUDIO_HZ;
	double val=in-cuttmp;
	cuttmp+=cutoff*val;
	return val;
}*/
/*
==============
dsp
==============
*/

Dsp dsp; //make it non-static for dsp_test

static deque<signed short> sound_stream_dsp_wqx;
static deque<signed short> sound_stream_dsp_host;
/*
static long long last_audio_queue_check_time=0;
static long long last_audio_queue_increase_time=0;
static int target_audio_queue_size_shrink_thres=2000;
static int target_audio_queue_size=10000;
static int target_audio_queue_size_min=5000;
static int target_audio_queue_size_max=20000;
static int min_audio_queue_size_observed=int_inf;*/


void manipulate_beeper(int a){
    long long current_cycle=nc2k_states.cycles;
	//note: (BEEPER_AUDIO_HZ+20) is to make it a bit larger, so that queue will not drain because of clock mismatch
    int CYCLES_SECOND_adjusted= CYCLES_SECOND;
    /*if(fast_forward && fast_forward_limit!=0){
        CYCLES_SECOND_adjusted= CYCLES_SECOND*fast_forward_limit;
    }else if(speed_multiplier!=1.0){
        CYCLES_SECOND_adjusted= CYCLES_SECOND*speed_multiplier;
    }*/
    CYCLES_SECOND_adjusted= int(CYCLES_SECOND*speed_multiplier);
    long long samples_start=last_beeper.cycle*(BEEPER_AUDIO_HZ+20)/(CYCLES_SECOND_adjusted);
    long long samples_end=current_cycle*(BEEPER_AUDIO_HZ+20)/(CYCLES_SECOND_adjusted);
    //printf("%lld, %d  %lld %lld\n",current_cycle -last_beeper.cycle, nc1020_states.cycles, samples_start,samples_end);
    last_beeper.cycle=current_cycle;

    if (g_audio_device) SDL_LockAudioDevice(g_audio_device);
    for(int i=0;i<(samples_end-samples_start);i++){
        if(sound_stream_beeper.size() > 4096) {//avoid beeper queue too large. 4096 samples =~ 90ms
            break;
        }
        sound_stream_beeper.push_back(8000*last_beeper.value);
    }
    if (g_audio_device) SDL_UnlockAudioDevice(g_audio_device);

    last_beeper.value=a;
}

void beeper_on_io_write(int a){
    if (a!=last_beeper.value){
        long long current_cycle=nc2k_states.cycles;
        //printf("%lld %lld, %d!!!!!!!!!!!\n",current_cycle, last_beeper.cycle, a);
    }
    manipulate_beeper(a);
}

void reset_dsp(){
    dsp.reset();
}
void write_data_to_dsp(uint8_t high,uint8_t low){
    dsp.write(high,low);
}

void post_cpu_run_sound_handling(){
    manipulate_beeper(last_beeper.value);
}

/*
void init_audio_dump_file(){
     audio_dump_fp=fopen("./audio1.dump","wb");
	 assert(audio_dump_fp!=0);
}
void close_audio_dump_file(){
     fclose(audio_dump_fp);
}
void write_audio_dump_file(unsigned char *p, int size){
    fwrite(p,size,1,audio_dump_fp);
}*/

const int dsp_busy_len_wqx = 5000; /* unit: samples */
const int dsp_drop_len_wqx = 10000;
const int dsp_drop_len_host = 10000;


// Linear resampler state for DSP -> output
static double g_dsp_phase = 0.0;  // in [0,1)
static float  g_dsp_s0 = 0.0f;    // last DSP sample
static float  g_dsp_s1 = 0.0f;    // next DSP sample (lookahead)

static const double g_dsp_ratio = (double)DSP_AUDIO_HZ / (double)BEEPER_AUDIO_HZ;

static inline Sint16 clamp_s16(int x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (Sint16)x;
}

void dsp_call_back(unsigned char* p, int len) {
	if (enable_debug_dsp) {
        static int cnt = 0;
        cnt++;
        if (cnt % 1000 == 0) {
            std::printf("dsp fifo_len_wqx=%d, fifo_len_host=%d\n", (int)sound_stream_dsp_wqx.size(), (int)sound_stream_dsp_host.size());
        }
        if(sound_stream_dsp_host.size() ==0) {
            printf("audio queue drain!!! fifo_len_wqx=%d \n", (int)sound_stream_dsp_wqx.size());
        }
    }

    const Sint16* in = (const Sint16*)p;
    const int samples = len / 2;

    for (int i = 0; i < samples; ++i) {
        //if this is too small, then dic's repeat prounce will be cut off
        //if this is too large, then sound will not be stopped immediately
        //(if wqx program doesn't respect dsp busy, then this is last resort to stop queueing)
        if (sound_stream_dsp_wqx.size() >= dsp_drop_len_wqx) {
            if (enable_debug_dsp) {
                std::printf("audio queue dropping (dsp fifo = %d)\n", (int)sound_stream_dsp_wqx.size());
            }
            break;
        }
        sound_stream_dsp_wqx.push_back(in[i]);
    }
}

//this function simulates the consumption speed of wqx dsp
//so that wqx will not feel the glitch of host's sound card processing speed
void dsp_move(int len /*unit sample*/){
    if (g_audio_device) SDL_LockAudioDevice(g_audio_device);
    for(int i=0;i<len;i++){
        if(sound_stream_dsp_wqx.empty()) break;
        if(sound_stream_dsp_host.size() < dsp_drop_len_host) {
            sound_stream_dsp_host.push_back(sound_stream_dsp_wqx.front());
        }else {
            //cannot break here, need to pop sound_stream_dsp_wqx
        }
        sound_stream_dsp_wqx.pop_front();
    }
    if (g_audio_device) SDL_UnlockAudioDevice(g_audio_device);
}

// this value is tricky:
// if too small sdl will pop because queue too small
// if too large, then too many queued and sound cannot be stopped immediately.
// (some wqx program respect dsp busy, some doesn't)
bool sound_busy() {
	const int fifo_len = (int)sound_stream_dsp_wqx.size();
    return fifo_len > dsp_busy_len_wqx;
}

// Single audio mixing callback: pulls beeper (44100 Hz) and DSP (8 kHz), resamples DSP, mixes, clamps.
static void audio_mix_cb(void* userdata, Uint8* stream, int len_bytes) {
    // We open the device as AUDIO_S16LSB mono
    Sint16* out = (Sint16*)stream;
    const int frames = len_bytes / (int)sizeof(Sint16);

    // Clear output
    SDL_memset(stream, 0, len_bytes);
    static int last_beeper_sample=0;

    for (int i = 0; i < frames; ++i) {
        // 1) Beeper @ output rate (44100 Hz)
        int beeper_sample = 0;
		if(enable_beeper){
			if (!sound_stream_beeper.empty()) {
				beeper_sample = sound_stream_beeper.front();
				sound_stream_beeper.pop_front();
				last_beeper_sample=beeper_sample;
			}else{
				beeper_sample=last_beeper_sample;
			}
			beeper_sample=filter_beeper(beeper_sample);
		}

        // 2) DSP resample 8000 -> 44100 using linear interpolation with phase in [0,1)
        // Advance fractional position
        double dsp_ratio_adjusted=g_dsp_ratio;
        /*if(fast_forward && fast_forward_limit!=0){
            dsp_ratio_adjusted= dsp_ratio_adjusted*fast_forward_limit;
        }else if(speed_multiplier!=1.0){
            dsp_ratio_adjusted= dsp_ratio_adjusted*speed_multiplier;
        }*/
        g_dsp_phase += dsp_ratio_adjusted;
        // When phase crosses 1.0, we step to next DSP sample(s)
        while (g_dsp_phase >= 1.0) {
            g_dsp_phase -= 1.0;
            // shift look-back/forward window
            g_dsp_s0 = g_dsp_s1;
            if (!sound_stream_dsp_host.empty()) {
                g_dsp_s1 = (float)sound_stream_dsp_host.front();
                sound_stream_dsp_host.pop_front();
            } else {
                // If no new DSP sample
                g_dsp_s1 = 0;
            }
        }
        // Interpolate between s0 and s1
        float dsp_f = g_dsp_s0 + (g_dsp_s1 - g_dsp_s0) * (float)g_dsp_phase;

        //dsp_f=filter2(dsp_f);

        // 3) Mix and clamp (you can add per-stream gains here if needed)
        int mixed = beeper_sample + (int)dsp_f;
        out[i] = clamp_s16(mixed);
    }
}

// Initialize one device with a callback, no SDL_mixer required.
void init_audio() {
    dsp.callback = dsp_call_back;

    SDL_AudioSpec desired_spec = {};
    desired_spec.freq = (int)BEEPER_AUDIO_HZ;   // Pick beeper rate to avoid resampling it
    desired_spec.format = AUDIO_S16LSB;
    desired_spec.channels = 1;                  // mono
    desired_spec.samples = 1024;
    desired_spec.callback = audio_mix_cb;
    desired_spec.userdata = nullptr;

    // Reset DSP resampler state
    g_dsp_phase = 0.0;
    g_dsp_s0 = 0.0f;
    g_dsp_s1 = 0.0f;

    if(g_audio_device){
        //only possible for emscripten version, because variables are kept across runs
        printf("re-used audio device from last run\n");
    } else {
        g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, nullptr, 0);
    }
    if (!g_audio_device) {
        std::printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);
}

// Call on shutdown
void shutdown_audio() {
    if (g_audio_device) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
}

