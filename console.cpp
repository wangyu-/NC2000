#include "comm.h"

#include "nand.h"
#include "nor.h"
#include <time.h>

#include <mutex>
#include "mem.h"
#include "state.h"
#include "cpu.h"
extern "C" {
#include "ansi/w65c02.h"
}
extern nc1020_states_t nc1020_states;

deque<string> udp_msgs;
std::mutex g_mutex;

void push_message(char* msg){
		g_mutex.lock();
		udp_msgs.push_back(msg);
		g_mutex.unlock();
}

string get_message(){
    string res;
    g_mutex.lock();
    if(!udp_msgs.empty()){
        res= udp_msgs.front();
        udp_msgs.pop_front();
        udp_msgs.clear();
    }
	g_mutex.unlock();
    return res;
}


deque<char> queue;
int32_t dummy_io_cnt=-1;
bool dummy_io(uint16_t addr, uint8_t &value){
	if(addr!=0x3fff) return false;
	if(dummy_io_cnt== -1) return false;
	if(queue.empty()) {
		value=0;
        dummy_io_cnt=-1;
		//printf("<dummy read %02x>\n",value);
		return true;
	}
	if(dummy_io_cnt++%2==0) {
		value=1;
	}else{
		value=queue.front();
		queue.pop_front();
	}
	//printf("<dummy read %02x>\n",value);
	return true;
}

void copy_to_addr(uint16_t addr, uint8_t * buf,uint16_t size){
	for(uint32_t i=0;i<size;i++){
		Peek16(addr+i)=buf[i];
	}
}

void handle_cmd(string & str){
	while(!str.empty() &&(str.back()=='\n'||str.back()=='\r'||str.back()==' ')){
		str.pop_back();
	} 
	printf("received %s from udp\n",str.c_str());
	auto cmds=split_s(str," ");
	for(int i=0;i<cmds.size();i++){
		printf("<%s>",cmds[i].c_str());
	}
	printf("\n");
	fflush(stdout);
	if(cmds.size()==0) return;
	if(cmds[0]=="save_flash"){
		write_nand_file();
		SaveNor();
		printf("flash saved to file!!\n");
		return;
	}
	if(cmds[0]=="dump"){
		uint32_t start=stoi(cmds[1],0,16);
		uint32_t size=stoi(cmds[2],0,10);
		for(uint32_t i=start;i<start+size;i++){
			printf("%02x ",Peek16(i));
		}
		printf("\n");
		return;
	}

	if(cmds[0]=="ec"){
		uint32_t start=stoi(cmds[1],0,16);
		for(uint32_t i=2;i<cmds.size();i++){
			Peek16(start++)=stoi(cmds[i],0,16);;
		}
		printf("ec done\n");
		return;
	}

	if(cmds[0]=="file_manager"){
		mPC = 0x3000;
		uint8_t buf[]={0x00,0x27,0x05,0x18,0x90,0xfa};
		copy_to_addr(0x3000, buf, sizeof buf);
		return;
	}

	if(cmds[0]=="create_dir"){
			printf("<pc=%x>\n",mPC);
			mPC = 0x3000;
			copy_to_addr(0x08d6, (uint8_t*)cmds[1].c_str(), cmds[1].size()+1);
			Peek16(0x0912)=0x02;
			uint8_t buf[]={0x00,0x0b,0x05,0x00,0x27,0x05,0x18,0x90,0xfa};
			copy_to_addr(0x3000, buf, sizeof buf);
			return;
	}

	if(cmds[0]=="wqxhex"){
			vector<char> wqxhex;
			read_file("wqxhex.bin", wqxhex);
			memcpy(nc1020_states.ext_ram, &wqxhex[0], wqxhex.size());
			ram_io[0x00]|=0x80;
			ram_io[0x0a]|=0x80;
			super_switch();
			mPC=0x4018;
			return;
	}
	if(cmds[0]=="speed"){
			if(cmds.size()==1) speed_multiplier=1;
			else{
				sscanf(cmds[1].c_str(),"%lf",&speed_multiplier);
			}
			printf("change speed to %f\n",speed_multiplier);
			return;
	}
	if(cmds[0]=="log"){
			enable_dyn_debug^=0x1;
			return;
	}
	if(cmds[0]=="put"){
			vector<char> file;
			read_file(cmds[1], file);
			string target=cmds[1];
			if(cmds.size()>2) target=cmds[2];
			queue.clear();
			for(int i=0;i<file.size();i++){
				queue.push_back(file[i]);
			}
			dummy_io_cnt=0;

			copy_to_addr(0x08d6, (uint8_t*)target.c_str(), target.size()+1);

			/*
			for(;;){
				auto value=Load(0x3fff);
				printf("<%02x>",value);
				if(value==0) break;
				auto value2=Load(0x3fff);
				printf("<%02x>",value2);
			}
			printf("\n");*/
			uint8_t buf[]={0xA9,0x70,0x8D,0x12,0x09,0xA9,0xEF,0x8D,0x13,0x09,0x8D,0x14,0x09,0x00,0x14,0x05,
0xA9,0x00,0x8D,0xF6,0x03,0xAD,0xFF,0x3F,0xC9,0x00,0xF0,0x21,0xAD,0xFF,0x3F,0x8D,
0x00,0x32,0xA9,0x00,0x85,0xDD,0xA9,0x32,0x85,0xDE,0xA9,0x01,0x8D,0x0F,0x09,0xA9,
0x00,0x8D,0x10,0x09,0x8D,0x11,0x09,0x00,0x17,0x05,0x4C,0x10,0x30,0x00,0x16,0x05,
0x00,0x27,0x05,0x4C,0x40,0x30,};
			copy_to_addr(0x3000,buf,sizeof(buf));
			mPC=0x3000;
			
			return;
	}

	if(cmds[0]=="sync_time") {
		time_t t = time(NULL);
  		struct tm tm = *localtime(&t);
		Peek16(1018)=tm.tm_year+1900-1881;
		Peek16(1019)=tm.tm_mon;
	    Peek16(1020)=tm.tm_mday-1;
		Peek16(1015)=tm.tm_hour;
		Peek16(1016)=tm.tm_min;
		Peek16(1017)=tm.tm_sec*2;
		return ;
	}
	printf("unknow command <%s>\n",cmds[0].c_str());
	fflush(stdout);
}


