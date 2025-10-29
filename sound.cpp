#include "comm.h"
#include "dsp/dsp.h"
#include "state.h"
#include <SDL2/SDL.h>

extern nc2k_states_t nc2k_states;

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
static int target_audio_queue_size_shrink_thres=2000;
static int target_audio_queue_size=10000;
static int target_audio_queue_size_min=5000;
static int target_audio_queue_size_max=20000;
static int min_audio_queue_size_observed=int_inf;


// 线性插值采样率转换
// 输入:
//   input        - 8000Hz的原始音频数据（16位PCM）
//   input_len    - 输入数据的样本数（不是字节数）
//   output       - 输出缓冲区（需提前分配足够空间）
//   output_max_len - 输出缓冲区最大可容纳的样本数
// 输出:
//   实际转换后的样本数
int resample_8000_to_44100(const int16_t* input, int input_len, int16_t* output, int output_max_len) {
    // 转换比率：44100 / 8000 = 5.5
    const double ratio = 44100.0 / 8000.0;
    // 计算理论输出样本数（不超过output_max_len）
    const int output_len = (int)(input_len * ratio);
    const int actual_output_len = output_len > output_max_len ? output_max_len : output_len;

    for (int i = 0; i < actual_output_len; i++) {
        // 计算当前输出样本对应输入的位置（浮点数）
        const double input_pos = i / ratio;
        // 整数部分（输入样本索引）
        const int src_idx = (int)input_pos;
        // 小数部分（插值权重）
        const double frac = input_pos - src_idx;

        // 处理边界：如果索引超出输入范围，用最后一个样本填充
        if (src_idx >= input_len - 1) {
            output[i] = input[input_len - 1];
        } else {
            // 线性插值：output = (1 - frac)*input[src_idx] + frac*input[src_idx+1]
            output[i] = (int16_t)(
                (1.0 - frac) * input[src_idx] + 
                frac * input[src_idx + 1]
            );
        }
    }

    return actual_output_len;
}

void manipulate_beeper(int a){
    long long current_cycle=nc2k_states.cycles;
	//note: (BEEPER_AUDIO_HZ+20) is to make it a bit larger, so that queue will not drain because of clock mismatch
    long long samples_start=last_beeper.cycle*(BEEPER_AUDIO_HZ+20)/CYCLES_SECOND;
    long long samples_end=current_cycle*(BEEPER_AUDIO_HZ+20)/CYCLES_SECOND;
    //printf("%lld, %d  %lld %lld\n",current_cycle -last_beeper.cycle, nc1020_states.cycles, samples_start,samples_end);
    last_beeper.cycle=current_cycle;

    for(int i=0;i<(samples_end-samples_start);i++){
        sound_stream_beeper.push_back(8000*last_beeper.value);
    }
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

	long long current_time=SDL_GetTicks64();
	if(current_time-last_audio_queue_check_time>1000*30){
		//if(pop_cnt==0){
		if(min_audio_queue_size_observed >target_audio_queue_size_shrink_thres){
			target_audio_queue_size -= min_audio_queue_size_observed-target_audio_queue_size_shrink_thres;
			if(target_audio_queue_size<target_audio_queue_size_min) target_audio_queue_size=target_audio_queue_size_min;
            if(enable_debug_beeper){
			    printf("shrink!!!!!!!!!!!!!!!target_queue=%d\n",target_audio_queue_size);
            }
		}
		//pop_cnt=0;
		min_audio_queue_size_observed=int_inf;
		last_audio_queue_check_time=current_time;

		//printf("dsp_audio_queue=%d\n",SDL_GetQueuedAudioSize(dsp_deviceId));
	}

	int queue_size=SDL_GetQueuedAudioSize(beeper_deviceId);

	//if(rand()%100==0) printf("q=%d\n",queue_size);
	if(queue_size<min_audio_queue_size_observed) min_audio_queue_size_observed=queue_size;
	if(queue_size==0 && current_time-last_audio_queue_increase_time>1000){
		//pop_cnt++;;
		target_audio_queue_size*=1.1;
		if(target_audio_queue_size>target_audio_queue_size_max) target_audio_queue_size=target_audio_queue_size_max;
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
const int dsp_busy_len=10000;
const int dsp_drop_len=20000;
void dsp_call_back(unsigned char *p,int len){
	static int cnt=0;
	cnt++;
	if(enable_debug_dsp){
		if(cnt%1000==0){
			printf("audio_queue_len=%d\n",SDL_GetQueuedAudioSize(dsp_deviceId));
		}
		if(SDL_GetQueuedAudioSize(dsp_deviceId)==0) {
			printf("audio queue drain!!! \n");
		}
	}
	 //if this is too small, then dic's repeat prounce will be cut off
	 //if this is too large, then sound will not be stopped immediately
	 //(if wqx program doesn't respect dsp busy, then this is last resort to stop queueing)
    if(SDL_GetQueuedAudioSize( dsp_deviceId )>dsp_drop_len) {
		if(enable_debug_dsp){
			printf("audio queue dropping!!!!!!");
		}
        return ;
    }
    
	
    // 原DSP数据是8000Hz的16位PCM（AUDIO_S16LSB），转换为int16_t数组
    int input_samples = len / sizeof(int16_t);
    if (input_samples <= 0) return;

    // 复制输入数据到转换缓冲区（确保不超过最大长度）
    int process_samples = input_samples > MAX_INPUT_SAMPLES ? MAX_INPUT_SAMPLES : input_samples;
    for (int i = 0; i < process_samples; i++) {
        resample_input_buf[i] = ((int16_t*)p)[i];
    }

    // 执行采样率转换（8000→44100）
    int output_samples = resample_8000_to_44100(
        resample_input_buf,
        process_samples,
        resample_output_buf,
        MAX_OUTPUT_SAMPLES
    );

    // 将转换后的数据入队播放（注意字节数 = 样本数 * 2）
    if (output_samples > 0) {
        SDL_QueueAudio(
            dsp_deviceId,
            resample_output_buf,
            output_samples * sizeof(int16_t)
        );
    }
}

bool sound_busy(){
	// this value is tricky:
	// if too small sdl will pop because queue too small
	// if too large, then too many queued and sound cannot be stopped immediately. 
	// (some wqx program respect dsp busy, some doesn't)
	if(SDL_GetQueuedAudioSize( dsp_deviceId )>dsp_busy_len) {
		//printf("busy!!!\n");
		return true;
	}
	return false;
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

	// dsp_deviceId = SDL_OpenAudioDevice(NULL, 0, &desired_spec2, NULL, 0);
    // if(dsp_deviceId<=0){
    //     printf("dsp SDL_OpenAudioDevice Failed!\n");
    // }

    beeper_deviceId = SDL_OpenAudioDevice(NULL, 0, &desired_spec, NULL, 0);
    if(beeper_deviceId<=0){
        printf("beeper SDL_OpenAudioDevice Failed!\n");
    }

	dsp_deviceId = beeper_deviceId;
	
	SDL_PauseAudioDevice(beeper_deviceId, 0);
    SDL_PauseAudioDevice(dsp_deviceId, 0);

	/*
	extern void dsp_test();
	extern void dsp_test2();
	extern void dsp_test3();
	if(enable_dsp_test){
		dsp_test();
		dsp_test2();
		dsp_test3();
	}*/
}


