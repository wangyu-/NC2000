#include "nc2000.h"
#include "comm.h"
#include "disassembler.h"
#include "state.h"
#include "cpu.h"
#include "mem.h"
#include "io.h"
#include "rom.h"
#include "nor.h"
#include "nand.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <deque>

extern WqxRom nc1020_rom;

nc1020_states_t nc1020_states;

//static uint32_t& version = nc1020_states.version;

static bool& slept = nc1020_states.slept;
static bool& should_wake_up = nc1020_states.should_wake_up;

static uint8_t* keypad_matrix = nc1020_states.keypad_matrix;
static uint32_t& lcd_addr = nc1020_states.lcd_addr;

static bool& wake_up_pending = nc1020_states.pending_wake_up;
static uint8_t& wake_up_key = nc1020_states.wake_up_flags;

void Initialize() {
	init_udp_server();

	if(nc2000){
    	nc1020_rom.norFlashPath= "./nor.bin";
  	}

	if(enable_inject){
		std::ifstream inject_bin("inject.bin");
		std::stringstream buffer;
		buffer << inject_bin.rdbuf();
		printf("<inject_size=%lu>\n",buffer.str().size());
		inject_code=buffer.str();
	}

	/*
	// bug: this doens't read full nand on windows
	std::ifstream nand_bin("2600nand.bin");
	std::stringstream nand_buffer;
	nand_buffer << nand_bin.rdbuf();
	printf("<size=%lu>\n",nand_buffer.str().size());*/

	if(nc2000){
		read_nand_file();
	}
	
	init_io();

//#ifdef DEBUG
//	FILE* file = fopen((nc1020_dir + "/wqxsimlogs.bin").c_str(), "rb");
//	fseek(file, 0L, SEEK_END);
//	uint32_t file_size = ftell(file);
//	uint32_t insts_count = (file_size - 8) / 8;
//	debug_logs.insts_count = insts_count;
//	debug_logs.logs = (log_rec_t*)malloc(insts_count * 8);
//	fseek(file, 0L, SEEK_SET);
//	fread(&debug_logs.insts_start, 4, 1, file);
//	fseek(file, 4L, SEEK_SET);
//	fread(&debug_logs.peek_addr, 4, 1, file);
//	fseek(file, 8L, SEEK_SET);
//	fread(debug_logs.logs, 8, insts_count, file);
//	fclose(file);
//#endif
}

void ResetStates(){
	//version = VERSION;
	memset(&nc1020_states,0,sizeof(nc1020_states_t));
	init_mem();
	reset_cpu_states();
}

/*
void Reset() {
	init_nor();
	ResetStates();
}*/

void LoadStates(){
	ResetStates();
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "rb");
	if (file == NULL) {
		return;
	}
	fread(&nc1020_states, 1, sizeof(nc1020_states), file);
	fclose(file);
	/*
	if (version != VERSION) {
		return;
	}*/
	super_switch();
}

void SaveStates(){
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "wb");
	fwrite(&nc1020_states, 1, sizeof(nc1020_states), file);
	fflush(file);
	fclose(file);
}

void LoadNC1020(){
	init_nor();
	init_rom();
	LoadStates();
}

void SaveNC1020(){
	SaveNor();
	SaveStates();
}

void SetKey(uint8_t key_id, bool down_or_up){
	uint8_t row = key_id % 8;
	uint8_t col = key_id / 8;
	uint8_t bits = 1 << col;
	if (key_id == 0x0F) {
		bits = 0xFE;
	}
	if (down_or_up) {
		keypad_matrix[row] |= bits;
	} else {
		keypad_matrix[row] &= ~bits;
	}

	if (down_or_up) {

		if (slept) {
			if (key_id >= 0x08 && key_id <= 0x0F && key_id != 0x0E) {
				switch (key_id) {
				case 0x08: wake_up_key = 0x00; break;
				case 0x09: wake_up_key = 0x0A; break;
				case 0x0A: wake_up_key = 0x08; break;
				case 0x0B: wake_up_key = 0x06; break;
				case 0x0C: wake_up_key = 0x04; break;
				case 0x0D: wake_up_key = 0x02; break;
				case 0x0E: wake_up_key = 0x0C; break;
				case 0x0F: wake_up_key = 0x00; break;
				}
				should_wake_up = true;
				wake_up_pending = true;
				slept = false;
			}
		} else {
			if (key_id == 0x0F) {
				slept = true;
			}
		}
	}
}
bool is_grey_mode(){
	return nc1020_states.grey_mode;
}
bool CopyLcdBuffer(uint8_t* buffer){
	if (lcd_addr == 0) return false;
	memcpy(buffer, ram_buff + lcd_addr, 1600);

	if(nc2000){
		if(!is_grey_mode()){
			memcpy(buffer, ram_buff + 0x19c0, 1600 );
		}else{
			memcpy(buffer, ram_buff + 0x19c0 -1600, 1600 *2);
		}

	}
	return true;
}
extern deque<signed short> sound_stream;
extern deque<signed short> sound_stream_dsp;
extern SDL_AudioDeviceID deviceId;
bool start=false;


FILE *fp;
void init_file(){
     fp=fopen("./1.output","wb");
	 assert(fp!=0);
}
void close_file(){
     fclose(fp);
}
void write_file(unsigned char *p, int size){
    fwrite(p,size,1,fp);
}

void manipulate_beeper2();
//int draining=false;
vector<signed short> buf;
double cuttmp=-8000;
double cutoff=2.0*3.141592654*40/AUDIO_HZ;
bool charging=true;
bool draining=false;

double queue_drain_threshold=AUDIO_HZ/10.0;
//int min_since_last_drain=0;
long long last_drain=0;
void RunTimeSlice(uint32_t time_slice, bool speed_up) {
	uint32_t end_cycles = time_slice * CYCLES_MS;

	end_cycles= end_cycles * speed_multiplier;

	auto old=sound_stream.size();
	//printf("<%u,%u, %lld>",cycles,end_cycles,SDL_GetTicks64());
	while (nc1020_states.cycles < end_cycles) {
		cpu_run();
	}
	manipulate_beeper2();

	//printf("<size=%lu, delta=%lu>\n",sound_stream.size(),sound_stream.size()-old);
	//if(sound_stream.size()>200||start){
		//start=true;
	if(SDL_GetQueuedAudioSize(deviceId)==0){
		printf("oops audio queue drained!\n");
		draining=false;
		//last_drain=SDL_GetTicks64();
		queue_drain_threshold*=1.1;
		//min_since_last_drain=AUDIO_HZ*100;
		//charging=true;
	}

	if(SDL_GetQueuedAudioSize(deviceId)>queue_drain_threshold){
		draining=true;
		printf("draining!!!!!!!!!!\n");
	}
	printf("queue size=%d\n",SDL_GetQueuedAudioSize(deviceId));
	/*if(charging && sound_stream.size()>AUDIO_HZ/20){
		charging=false;
	}*/

	if(true||!charging){
		while(!sound_stream.empty()){
			//vector<signed short> vec(sound_stream.begin(),sound_stream.end());
			//SDL_QueueAudio(deviceId, &vec[0], vec.size()*2);
			//write_file((unsigned char *)&sound_stream[0], 2);
			//sound_stream.clear();
			//if(SDL_GetQueuedAudioSize(deviceId)<1000){

			/*if(SDL_GetQueuedAudioSize(deviceId)<AUDIO_HZ/10){
				draining=false;
			}*/
			if(!draining){
				int value=sound_stream[0];
				double val=value-cuttmp;
				cuttmp+=cutoff*val;
				value=val;
				if(!sound_stream_dsp.empty()){
					value+=sound_stream_dsp.front();
					sound_stream_dsp.pop_front();
					if(value>32767) value=32767;
					if(value<-32768) value=-32768;
				}
				buf.push_back(value);
			}
			sound_stream.pop_front();
		}
		if(!buf.empty()){
			SDL_QueueAudio(deviceId, &buf[0] , 2*buf.size());
			buf.clear();
		}
	}

	nc1020_states.previous_cycles+=end_cycles;
	nc1020_states.cycles -= end_cycles;
	nc1020_states.timer0_cycles -= end_cycles;
	nc1020_states.timer1_cycles -= end_cycles;


}
