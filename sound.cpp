#include "comm.h"
#include "dsp/dsp.h"
#include "state.h"
#include <SDL2/SDL.h>

extern nc1020_states_t nc1020_states;

static SDL_AudioDeviceID beeper_deviceId;
static SDL_AudioDeviceID dsp_deviceId;
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

static BeeperSignal last_beeper{0};
static deque<signed short> sound_stream_beeper;
/*buffer to SDL_QueueAudio */
static vector<signed short> beeper_buffer; 

/*filter out DC signal*/
static double cuttmp=-8000;
static double cutoff=2.0*3.141592654*40/DSP_AUDIO_HZ;

/*
==============
dsp
==============
*/

Dsp dsp; //make it non-static for dsp_test

static deque<signed short> sound_stream_dsp;
static long long last_audio_queue_check_time=0;
static long long last_audio_queue_increase_time=0;
static int target_audio_queue_size=10000;
static int min_audio_queue_size_observed=int_inf;

void manipulate_beeper(int a){
    long long current_cycle=nc1020_states.cycles;
    long long samples_start=last_beeper.cycle*BEEPER_AUDIO_HZ/CYCLES_SECOND;
    long long samples_end=current_cycle*BEEPER_AUDIO_HZ/CYCLES_SECOND;
    //printf("%lld, %d  %lld %lld\n",current_cycle -last_beeper.cycle, nc1020_states.cycles, samples_start,samples_end);
    last_beeper.cycle=current_cycle;

    for(int i=0;i<(samples_end-samples_start);i++){
        sound_stream_beeper.push_back(8000*last_beeper.value);
    }
    last_beeper.value=a;
}

void beeper_on_io_write(int a){
    if (a!=last_beeper.value){
        long long current_cycle=nc1020_states.cycles;
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

	long long current_time=SDL_GetTicks64();
	if(current_time-last_audio_queue_check_time>1000*10){
		//if(pop_cnt==0){
		if(min_audio_queue_size_observed >DSP_AUDIO_HZ/100){
			target_audio_queue_size -= min_audio_queue_size_observed-DSP_AUDIO_HZ/100;
            if(enable_debug_beeper){
			    printf("shrink!!!!!!!!!!!!!!!target_queue=%d\n",target_audio_queue_size);
            }
		}
		//pop_cnt=0;
		min_audio_queue_size_observed=DSP_AUDIO_HZ*999;
		last_audio_queue_check_time=current_time;

		//printf("dsp_audio_queue=%d\n",SDL_GetQueuedAudioSize(dsp_deviceId));
	}

	int queue_size=SDL_GetQueuedAudioSize(beeper_deviceId);

	//if(rand()%100==0) printf("q=%d\n",queue_size);
	if(queue_size<min_audio_queue_size_observed) min_audio_queue_size_observed=queue_size;
	if(queue_size==0 && current_time-last_audio_queue_increase_time>1000){
		//pop_cnt++;;
		target_audio_queue_size*=1.1;
		last_audio_queue_increase_time=current_time;
        if(enable_debug_beeper){
		    printf("oops audio queue drained! target_queue=%d\n",target_audio_queue_size);
        }
	}

	while(!sound_stream_beeper.empty() &&  SDL_GetQueuedAudioSize(beeper_deviceId) >target_audio_queue_size){
		sound_stream_beeper.pop_front();
	}

	if(enable_beeper)
	{
		while(!sound_stream_beeper.empty()){
			int value=sound_stream_beeper[0];
			sound_stream_beeper.pop_front();
			double val=value-cuttmp;
			cuttmp+=cutoff*val;
			value=val;
			/*if(!sound_stream_dsp.empty()){
				value+=sound_stream_dsp.front();
				sound_stream_dsp.pop_front();
			}*/
			beeper_buffer.push_back(value);
		}
		if(!beeper_buffer.empty()){
			SDL_QueueAudio(beeper_deviceId, &beeper_buffer[0] , 2*beeper_buffer.size());
			beeper_buffer.clear();
		}
	}
}


void init_audio_dump_file(){
     audio_dump_fp=fopen("./audio1.dump","wb");
	 assert(audio_dump_fp!=0);
}
void close_audio_dump_file(){
     fclose(audio_dump_fp);
}
void write_audio_dump_file(unsigned char *p, int size){
    fwrite(p,size,1,audio_dump_fp);
}

/*
void callback(void* userdata, Uint8* stream, int len) {
	short * snd = reinterpret_cast<short*>(stream);
	printf("calling!!\n");
	if (sound_stream.size()<len){
		printf("oops!!\n");
		for(int i=0;i<len;i++){
			snd[i]=0;
		}
	}else{
		for(int i=0;i<len;i++){
				int value=sound_stream[0];
				sound_stream.pop_front();
				double val=value-cuttmp;
				cuttmp+=cutoff*val;
				value=val;
				if(!sound_stream_dsp.empty()){
					value+=sound_stream_dsp.front();
					sound_stream_dsp.pop_front();
					if(value>32767) value=32767;
					if(value<-32768) value=-32768;
				}
				snd[i]=value;
		}

	}
}*/

void dsp_call_back(unsigned char *p,int len){
    SDL_QueueAudio(dsp_deviceId, p,len );
}

void init_audio(){
    dsp.callback=dsp_call_back;

    //SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec desired_spec = {
        .freq = BEEPER_AUDIO_HZ,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .samples = 4096,
        .callback = NULL,
        .userdata = NULL,
    };
	SDL_AudioSpec desired_spec2 = {
        .freq = DSP_AUDIO_HZ,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .samples = 4096,
        .callback = NULL,
        .userdata = NULL,
    };

    //SDL_AudioSpec obtained_spec;
    beeper_deviceId = SDL_OpenAudioDevice(NULL, 0, &desired_spec, NULL, 0);
    if(beeper_deviceId<=0){
        printf("SDL_OpenAudioDevice Failed!\n");
    }
	dsp_deviceId = SDL_OpenAudioDevice(NULL, 0, &desired_spec2, NULL, 0);
    if(dsp_deviceId<=0){
        printf("SDL_OpenAudioDevice Failed!\n");
    }
	SDL_PauseAudioDevice(beeper_deviceId, 0);
    SDL_PauseAudioDevice(dsp_deviceId, 0);

	extern void dsp_test();
	extern void dsp_test2();
	extern void dsp_test3();
	if(enable_dsp_test){
		//dsp_test();
		//dsp_test2();
		//dsp_test3();
	}
}

bool sound_busy(){
	//try to mimic emux's behavior
	//at the moment only for experiment

	//the default value 8000/2 (equivalently 44100/2) is too small
	//have to use large value here
	if(SDL_GetQueuedAudioSize( dsp_deviceId )>20000) {
		//printf("busy!!!\n");
		return true;
	}
	return false;
}
