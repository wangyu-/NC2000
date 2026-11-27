#include "comm.h"
#include "sound.h"
#include "state.h"

#include <algorithm>
#include <cmath>

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
        sound_stream_beeper.push_back(4000*last_beeper.value);
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

const int dsp_drop_len_host = 10000;

// Adaptive DSP playback rate control (±10% max):
// 1) Prebuffer 600 samples (75 ms) and start gently stretched at -3% (0.97x) so normal
//    8 kHz speech climbs into [1600, 3200] (1.00x) without audible startup pitch dip,
//    while slower RC-clocked PCM streams (down to 7200 Hz) smoothly settle down to 0.90x.
// 2) Hold nominal 1.00x across [1600, 3200], then compress up to +10% (1.10x) by 6000
//    (still 4000 samples / 500 ms below dsp_drop_len_host = 10000) to absorb backlog.
static constexpr int    kDspPrebufferSamples = 600;   // 75 ms @ 8 kHz
static constexpr int    kDspMinWatermark     = 400;   // 50 ms: full -10% stretch below this
static constexpr int    kDspLowWatermark     = 800;   // 100 ms: -3% stretch
static constexpr int    kDspTargetWatermark  = 1600;  // 200 ms: nominal 1.00x
static constexpr int    kDspHighWatermark    = 3200;  // 400 ms: begin compressing backlog
static constexpr int    kDspMaxWatermark     = 6000;  // 750 ms: full +10% compress (4000 below drop)
static constexpr double kDspInitScale        = 0.97;
static constexpr double kDspMinScale         = 0.90;
static constexpr double kDspMaxScale         = 1.10;

// Linear resampler state for DSP -> output
static double g_dsp_phase = 0.0;  // in [0,1)
static float  g_dsp_s0 = 0.0f;    // last DSP sample
static float  g_dsp_s1 = 0.0f;    // next DSP sample (lookahead)
static bool   g_dsp_buffering = true;
static int    g_dsp_idle_cbs = 0;
static int    g_dsp_pushed_since_cb = 0;
static double g_dsp_rate_scale = kDspInitScale;

static const double g_dsp_ratio = (double)DSP_AUDIO_HZ / (double)BEEPER_AUDIO_HZ;

static inline Sint16 clamp_s16(int x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (Sint16)x;
}

void sound_push_dsp_samples(const int16_t* samples, int count) {
    if (count <= 0) return;

    if (g_audio_device) SDL_LockAudioDevice(g_audio_device);
    if (enable_debug_dsp && sound_stream_dsp_host.empty()) {
        std::printf("[audio] host dsp queue drained\n");
    }
    int pushed = 0;
    for (int i = 0; i < count; ++i) {
        if ((int)sound_stream_dsp_host.size() >= dsp_drop_len_host) break;
        sound_stream_dsp_host.push_back(samples[i]);
        ++pushed;
    }
    g_dsp_pushed_since_cb += pushed;
    if (g_audio_device) SDL_UnlockAudioDevice(g_audio_device);
}

// Single audio mixing callback: pulls beeper (44100 Hz) and DSP (8 kHz), resamples DSP, mixes, clamps.
static void audio_mix_cb(void* userdata, Uint8* stream, int len_bytes) {
    // We open the device as AUDIO_S16LSB mono
    Sint16* out = (Sint16*)stream;
    const int frames = len_bytes / (int)sizeof(Sint16);

    // Clear output
    SDL_memset(stream, 0, len_bytes);
    static int last_beeper_sample=0;

    // Update adaptive DSP resample scale once per callback based on host queue level.
    const int q = (int)sound_stream_dsp_host.size();
    const int pushed = g_dsp_pushed_since_cb;
    g_dsp_pushed_since_cb = 0;
    if (pushed > 0) {
        g_dsp_idle_cbs = 0;
    } else if (g_dsp_idle_cbs < 100) {
        ++g_dsp_idle_cbs;
    }
    const bool producer_active = (pushed > 0 || g_dsp_idle_cbs < 2);

    if (g_dsp_buffering) {
        if (q >= kDspPrebufferSamples || (q > 0 && !producer_active)) {
            g_dsp_buffering = false;
            g_dsp_rate_scale = kDspInitScale;
        }
    } else {
        double target_scale;
        if (!producer_active && q <= kDspHighWatermark) {
            // Draining tail after speech/PCM ended: play out at nominal 1.0x speed.
            target_scale = 1.0;
        } else if (q <= kDspMinWatermark) {
            target_scale = kDspMinScale;
        } else if (q <= kDspLowWatermark) {
            double t = (double)(q - kDspMinWatermark) / (kDspLowWatermark - kDspMinWatermark);
            target_scale = kDspMinScale + (kDspInitScale - kDspMinScale) * t;
        } else if (q <= kDspTargetWatermark) {
            double t = (double)(q - kDspLowWatermark) / (kDspTargetWatermark - kDspLowWatermark);
            target_scale = kDspInitScale + (1.0 - kDspInitScale) * t;
        } else if (q <= kDspHighWatermark) {
            target_scale = 1.0;
        } else {
            double t = std::min(1.0, (double)(q - kDspHighWatermark) / (kDspMaxWatermark - kDspHighWatermark));
            target_scale = 1.0 + (kDspMaxScale - 1.0) * t;
        }
        g_dsp_rate_scale += (target_scale - g_dsp_rate_scale) * 0.25;
    }

    const double dsp_ratio_adjusted = g_dsp_ratio * g_dsp_rate_scale;
    int cb_underrun = 0;

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
        float dsp_f = 0.0f;
        if (!g_dsp_buffering) {
            g_dsp_phase += dsp_ratio_adjusted;
            while (g_dsp_phase >= 1.0) {
                g_dsp_phase -= 1.0;
                g_dsp_s0 = g_dsp_s1;
                if (!sound_stream_dsp_host.empty()) {
                    g_dsp_s1 = (float)sound_stream_dsp_host.front();
                    sound_stream_dsp_host.pop_front();
                } else {
                    if (producer_active) ++cb_underrun;
                    // Exponential decay to zero on drain to avoid hard-step clicks
                    g_dsp_s1 *= 0.92f;
                    if (std::fabs(g_dsp_s0) < 1.0f && std::fabs(g_dsp_s1) < 1.0f) {
                        g_dsp_s0 = 0.0f;
                        g_dsp_s1 = 0.0f;
                        g_dsp_phase = 0.0;
                        g_dsp_buffering = true;
                        g_dsp_rate_scale = kDspInitScale;
                        break;
                    }
                }
            }
            if (!g_dsp_buffering) {
                dsp_f = g_dsp_s0 + (g_dsp_s1 - g_dsp_s0) * (float)g_dsp_phase;
            }
        }

        // 3) Mix and clamp (you can add per-stream gains here if needed)
        int mixed = (int)(beeper_sample * beeper_volume + dsp_f * speaker_volume);
        out[i] = clamp_s16(mixed);
    }

    if (enable_debug_dsp) {
        static int dbg_cbs = 0;
        static int dbg_pushed = 0;
        static int dbg_underruns = 0;
        dbg_pushed += pushed;
        dbg_underruns += cb_underrun;
        if (++dbg_cbs >= 20) {
            if (dbg_pushed > 0 || q > 0 || dbg_underruns > 0) {
                double elapsed_s = (dbg_cbs * frames) / (double)BEEPER_AUDIO_HZ;
                std::printf("[audio] q=%d in=%.0fHz scale=%.3f (out=%.0fHz) underrun=%d\n",
                            q, dbg_pushed / elapsed_s, g_dsp_rate_scale,
                            DSP_AUDIO_HZ * g_dsp_rate_scale, dbg_underruns);
            }
            dbg_cbs = 0;
            dbg_pushed = 0;
            dbg_underruns = 0;
        }
    }
}

// Initialize one device with a callback, no SDL_mixer required.
void init_audio() {
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
    g_dsp_buffering = true;
    g_dsp_idle_cbs = 0;
    g_dsp_pushed_since_cb = 0;
    g_dsp_rate_scale = kDspInitScale;

    if(g_audio_device){
        //if previous audio device is not correctly shutdhown.
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

