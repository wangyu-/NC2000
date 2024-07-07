#include "cpu.h"
#include "comm.h"
#include "disassembler.h"
#include "mem.h"
#include "state.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include "nand.h"
#include "nor.h"



extern nc1020_states_t nc1020_states;

static uint32_t& cycles = nc1020_states.cycles;
static bool& should_irq = nc1020_states.should_irq;
static bool& timer0_toggle = nc1020_states.timer0_toggle;

static uint32_t& timer0_cycles = nc1020_states.timer0_cycles;
static uint32_t& timer1_cycles = nc1020_states.timer1_cycles;

static bool& should_wake_up = nc1020_states.should_wake_up;

static uint16_t& reg_pc = nc1020_states.cpu.reg_pc;
static uint8_t& reg_a = nc1020_states.cpu.reg_a;
static uint8_t& reg_ps = nc1020_states.cpu.reg_ps;
static uint8_t& reg_x = nc1020_states.cpu.reg_x;
static uint8_t& reg_y = nc1020_states.cpu.reg_y;
static uint8_t& reg_sp = nc1020_states.cpu.reg_sp;

static int udp_fd;
static struct sockaddr_in myaddr;
static struct sockaddr_in remaddr;

string udp_msg;
std::mutex g_mutex;

void read_loop(std::string msg)
{
	char buf[1000];
	socklen_t addrlen = sizeof(remaddr);  
	
	while(1){
		ssize_t recvlen = recvfrom(udp_fd, buf, sizeof(buf)-1, 0, (struct sockaddr *)&remaddr, &addrlen);
		if(recvlen>0) {
			buf[recvlen]=0;
		} 
		g_mutex.lock();
		udp_msg=buf;
		g_mutex.unlock();
	}
    //std::cout << "task1 says: " << msg;
}


void init_udp_server(){
	if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket");
		return ;
	}
	printf("create socket done\n");
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(listen_port);

	if (bind(udp_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		close(udp_fd);
		return ;
	}
	
	printf("bind socket done\n");

	printf("udp listening at %d!!!\n",listen_port);
	std::thread task1(read_loop, "hi");
	task1.detach();
	return ;
}
void reset_cpu_states(){
	nc1020_states.should_irq = false;
	nc1020_states.cycles = 0;
	nc1020_states.cpu.reg_a = 0;
	nc1020_states.cpu.reg_ps = 0x24;
	nc1020_states.cpu.reg_x = 0;
	nc1020_states.cpu.reg_y = 0;
	nc1020_states.cpu.reg_sp = 0xFF;
	nc1020_states.cpu.reg_pc = PeekW(RESET_VEC);
	nc1020_states.timer0_cycles = CYCLES_TIMER0;
	nc1020_states.timer1_cycles = CYCLES_TIMER1;
}
void AdjustTime(){
	uint8_t* clock_buff = nc1020_states.clock_buff;
    if (++ clock_buff[0] >= 60) {
        clock_buff[0] = 0;
        if (++ clock_buff[1] >= 60) {
            clock_buff[1] = 0;
            if (++ clock_buff[2] >= 24) {
                clock_buff[2] &= 0xC0;
                ++ clock_buff[3];
            }
        }
    }
}

bool IsCountDown(){
	uint8_t* clock_buff = nc1020_states.clock_buff;
	uint8_t& clock_flags = nc1020_states.clock_flags;
    if (!(clock_buff[10] & 0x02) ||
        !(clock_flags & 0x02)) {
        return false;
    }
    return (
        ((clock_buff[7] & 0x80) && !(((clock_buff[7] ^ clock_buff[2])) & 0x1F)) ||
        ((clock_buff[6] & 0x80) && !(((clock_buff[6] ^ clock_buff[1])) & 0x3F)) ||
        ((clock_buff[5] & 0x80) && !(((clock_buff[5] ^ clock_buff[0])) & 0x3F))
        );
}

void inject(){
	for(int i=0;i<8;i++){
		for(int j=0;j<=0xFE;j+=2){
			ram_b[0x100*i+j]=j;
			ram_b[0x100*i+j+1]=i;
		}
	}

	for(int i=0;i<16;i++){
		printf("<%x>",ram_b[i]);
	}
	printf("\n");

	memcpy(nc1020_states.ext_ram, inject_code.c_str(), inject_code.size());
	ram_io[0x00]=0x80;
	ram_io[0x0a]=0x80;
	//Peek16(0xe3)=0x40;
	//ram_io[0x0d]&=0x5d;
	super_switch();
	//Peek16(0x280)=0xFF;
	//Peek16(0x281)=0xFF;
	//Peek16(0x282)=0xFF;
	//Peek16(0x283)=0xFF;
	//Peek16(0xe3)=0x40;
	//Peek16(0xe4)=0xb2;
	//nc1020_states.ext_ram[0x17]=0x58;
	reg_pc=0x4018;
}

vector<string> split_s(const string &str, const string &sp) {
    vector<string> res;
    size_t i = 0, pos;
    for (;; i = pos + sp.length()) {
        pos = str.find(sp, i);
        if (pos == string::npos) {
			string s=str.substr(i, pos);
            if(!s.empty()) res.push_back(s);
            break;
        } else {
			string s=str.substr(i, pos - i);
            if(!s.empty()) res.push_back(s);
        }
    }
    return res;
}

void copy_to_addr(uint16_t addr, uint8_t * buf,uint16_t size){
	for(uint32_t i=0;i<size;i++){
		Peek16(addr+i)=buf[i];
	}
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
uint32_t speed_multiplier=1;
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
		reg_pc = 0x3000;
		uint8_t buf[]={0x00,0x27,0x05,0x18,0x90,0xfa};
		copy_to_addr(0x3000, buf, sizeof buf);
		return;
	}

	if(cmds[0]=="create_dir"){
			printf("<pc=%x>\n",reg_pc);
			reg_pc = 0x3000;
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
			reg_pc=0x4018;
			return;
	}
	if(cmds[0]=="speed"){
			if(cmds.size()==1) speed_multiplier=1;
			else{
				sscanf(cmds[1].c_str(),"%u",&speed_multiplier);
			}
			printf("change speed to %u\n",speed_multiplier);
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
			reg_pc=0x3000;
			
			return;
	}
	printf("unknow command <%s>\n",cmds[0].c_str());
}


void cpu_run(){

		string tmp;
		g_mutex.lock();
		if(!udp_msg.empty()){
			tmp= udp_msg;
			udp_msg.clear();
		}
		g_mutex.unlock();
		if(!tmp.empty()){
			handle_cmd(tmp);
		}
		tick++;
		//if(tick%100000000==0) printf("tick=%lld\n",tick);
//#ifdef DEBUG
//		if (executed_insts == 2792170) {
//			printf("debug start!\n");
//		}
//		if (executed_insts >= debug_logs.insts_start &&
//			executed_insts < debug_logs.insts_start + debug_logs.insts_count) {
//			log_rec_t& log = debug_logs.logs[executed_insts - debug_logs.insts_start];
//			string debug_info;
//			if (log.reg_pc != reg_pc) {
//				debug_info += " pc ";
//			}
//			if (log.reg_a != reg_a) {
//				debug_info += " a ";
//			}
//			if (log.reg_ps != reg_ps) {
//				debug_info += " ps ";
//			}
//			if (log.reg_x != reg_x) {
//				debug_info += " x ";
//			}
//			if (log.reg_y != reg_y) {
//				debug_info += " y ";
//			}
//			if (log.reg_sp != reg_sp) {
//				debug_info += " sp ";
//			}
//			if (debug_logs.peek_addr != -1) {
//				if (log.peeked != Peek((uint16_t)debug_logs.peek_addr)) {
//					debug_info += " mem ";
//				}
//			} else {
//				if (log.peeked != Peek(reg_pc)) {
//					debug_info += " op ";
//				}
//			}
//			if (debug_info.length()) {
//				printf("%d: %s\n", executed_insts, debug_info.c_str());
//				exit(-1);
//			}
//		}
//		if (executed_insts >= debug_logs.insts_start + debug_logs.insts_count) {
//			printf("ok\n");
//		}
//#endif
		//if(tick>8525000) wanna_inject=true;
		if(enable_inject&& tick>60000000) wanna_inject=true;
		if(wanna_inject&& !injected){
			inject();
			printf("injected!!!");
			wanna_inject=false;
			injected=true;
		}

		//if(tick>=6012850) enable_debug_pc=true;
		//if(injected && reg_pc==0x2000) enable_debug_pc=true;
		if(enable_debug_pc||enable_dyn_debug){
			unsigned char buf[10];
			buf[0]=Peek16(reg_pc);
			buf[1]=Peek16(reg_pc+1);
			buf[2]=Peek16(reg_pc+2);
			buf[3]=0;
			printf("tick=%lld ",tick /*, reg_pc*/);
			printf("%02x %02x %02x %02x; ",Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			printf("bs=%02x roa_bbs=%02x ramb=%02x zp=%02x reg=%02x,%02x,%02x,%02x,%03o  pc=%s",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],reg_a,reg_x,reg_y,reg_sp,reg_ps,disassemble_next(buf,reg_pc).c_str());
			printf("\n");

			//getchar();		
		}
		if(injected && tick%1==0){
			//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			//getchar();
		}

		
		switch (Peek16(reg_pc++)) {
		case 0x00: {
			reg_pc++;
			stack[reg_sp--] = reg_pc >> 8;
			stack[reg_sp--] = reg_pc & 0xFF;
			reg_ps |= 0x10;
			stack[reg_sp--] = reg_ps;
			reg_ps |= 0x04;
			reg_pc = PeekW(IRQ_VEC);
			cycles += 7;
		}
			break;
		case 0x01: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x02: {
		}
			break;
		case 0x03: {
		}
			break;
		case 0x04: {
		}
			break;
		case 0x05: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x06: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 5;
		}
			break;
		case 0x07: {
		}
			break;
		case 0x08: {
			stack[reg_sp--] = reg_ps;
			cycles += 3;
		}
			break;
		case 0x09: {
			uint16_t addr = reg_pc++;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x0A: {
			reg_ps &= 0x7C;
			reg_ps |= reg_a >> 7;
			reg_a <<= 1;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x0B: {
		}
			break;
		case 0x0C: {
		}
			break;
		case 0x0D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x0E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x0F: {
		}
			break;
		case 0x10: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x80)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x11: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x12: {
		}
			break;
		case 0x13: {
		}
			break;
		case 0x14: {
		}
			break;
		case 0x15: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x16: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x17: {
		}
			break;
		case 0x18: {
			reg_ps &= 0xFE;
			cycles += 2;
		}
			break;
		case 0x19: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x1A: {
		}
			break;
		case 0x1B: {
		}
			break;
		case 0x1C: {
		}
			break;
		case 0x1D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x1E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x1F: {
		}
			break;
		case 0x20: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_pc--;
			stack[reg_sp--] = reg_pc >> 8;
			stack[reg_sp--] = reg_pc & 0xFF;
			reg_pc = addr;
			cycles += 6;
		}
			break;
		case 0x21: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x22: {
		}
			break;
		case 0x23: {
		}
			break;
		case 0x24: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x3D;
			reg_ps |= (!(reg_a & tmp1) << 1) | (tmp1 & 0xC0);
			cycles += 3;
		}
			break;
		case 0x25: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x26: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 5;
		}
			break;
		case 0x27: {
		}
			break;
		case 0x28: {
			reg_ps = stack[++reg_sp];
			cycles += 4;
		}
			break;
		case 0x29: {
			uint16_t addr = reg_pc++;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x2A: {
			uint8_t tmp1 = reg_a;
			reg_a = (reg_a << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1) | (tmp1 >> 7);
			cycles += 2;
		}
			break;
		case 0x2B: {
		}
			break;
		case 0x2C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x3D;
			reg_ps |= (!(reg_a & tmp1) << 1) | (tmp1 & 0xC0);
			cycles += 4;
		}
			break;
		case 0x2D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x2E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x2F: {
		}
			break;
		case 0x30: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x80)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x31: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x32: {
		}
			break;
		case 0x33: {
		}
			break;
		case 0x34: {
		}
			break;
		case 0x35: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x36: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x37: {
		}
			break;
		case 0x38: {
			reg_ps |= 0x01;
			cycles += 2;
		}
			break;
		case 0x39: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x3A: {
		}
			break;
		case 0x3B: {
		}
			break;
		case 0x3C: {
		}
			break;
		case 0x3D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x3E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x3F: {
		}
			break;
		case 0x40: {
			reg_ps = stack[++reg_sp];
			reg_pc = stack[++reg_sp];
			reg_pc |= stack[++reg_sp] << 8;
			cycles += 6;
		}
			break;
		case 0x41: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x42: {
		}
			break;
		case 0x43: {
		}
			break;
		case 0x44: {
		}
			break;
		case 0x45: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x46: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 5;
		}
			break;
		case 0x47: {
		}
			break;
		case 0x48: {
			stack[reg_sp--] = reg_a;
			cycles += 3;
		}
			break;
		case 0x49: {
			uint16_t addr = reg_pc++;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x4A: {
			reg_ps &= 0x7C;
			reg_ps |= reg_a & 0x01;
			reg_a >>= 1;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x4B: {
		}
			break;
		case 0x4C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_pc = addr;
			cycles += 3;
		}
			break;
		case 0x4D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x4E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x4F: {
		}
			break;
		case 0x50: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x40)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x51: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x52: {
		}
			break;
		case 0x53: {
		}
			break;
		case 0x54: {
		}
			break;
		case 0x55: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x56: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x57: {
		}
			break;
		case 0x58: {
			reg_ps &= 0xFB;
			cycles += 2;
		}
			break;
		case 0x59: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x5A: {
		}
			break;
		case 0x5B: {
		}
			break;
		case 0x5C: {
		}
			break;
		case 0x5D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x5E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x5F: {
		}
			break;
		case 0x60: {
			reg_pc = stack[++reg_sp];
			reg_pc |= (stack[++reg_sp] << 8);
			reg_pc++;
			cycles += 6;
		}
			break;
		case 0x61: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 6;
		}
			break;
		case 0x62: {
		}
			break;
		case 0x63: {
		}
			break;
		case 0x64: {
		}
			break;
		case 0x65: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 3;
		}
			break;
		case 0x66: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 5;
		}
			break;
		case 0x67: {
		}
			break;
		case 0x68: {
			reg_a = stack[++reg_sp];
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x69: {
			uint16_t addr = reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 2;
		}
			break;
		case 0x6A: {
			uint8_t tmp1 = reg_a;
			reg_a = (reg_a >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1) | (tmp1 & 0x01);
			cycles += 2;
		}
			break;
		case 0x6B: {
		}
			break;
		case 0x6C: {
			uint16_t addr = PeekW(PeekW(reg_pc));
			reg_pc += 2;
			reg_pc = addr;
			cycles += 6;
		}
			break;
		case 0x6D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x6E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x6F: {
		}
			break;
		case 0x70: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x40)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x71: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 5;
		}
			break;
		case 0x72: {
		}
			break;
		case 0x73: {
		}
			break;
		case 0x74: {
		}
			break;
		case 0x75: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x76: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x77: {
		}
			break;
		case 0x78: {
			reg_ps |= 0x04;
			cycles += 2;
		}
			break;
		case 0x79: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x7A: {
		}
			break;
		case 0x7B: {
		}
			break;
		case 0x7C: {
		}
			break;
		case 0x7D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x7E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x7F: {
		}
			break;
		case 0x80: {
		}
			break;
		case 0x81: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			Store(addr, reg_a);
			cycles += 6;
		}
			break;
		case 0x82: {
		}
			break;
		case 0x83: {
		}
			break;
		case 0x84: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_y);
			cycles += 3;
		}
			break;
		case 0x85: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_a);
			cycles += 3;
		}
			break;
		case 0x86: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_x);
			cycles += 3;
		}
			break;
		case 0x87: {
		}
			break;
		case 0x88: {
			reg_y--;
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0x89: {
		}
			break;
		case 0x8A: {
			reg_a = reg_x;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x8B: {
		}
			break;
		case 0x8C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_y);
			cycles += 4;
		}
			break;
		case 0x8D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 4;
		}
			break;
		case 0x8E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_x);
			cycles += 4;
		}
			break;
		case 0x8F: {
		}
			break;
		case 0x90: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x01)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x91: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			addr += reg_y;
			reg_pc++;
			Store(addr, reg_a);
			cycles += 6;
		}
			break;
		case 0x92: {
		}
			break;
		case 0x93: {
		}
			break;
		case 0x94: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			Store(addr, reg_y);
			cycles += 4;
		}
			break;
		case 0x95: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			Store(addr, reg_a);
			cycles += 4;
		}
			break;
		case 0x96: {
			uint16_t addr = (Peek16(reg_pc++) + reg_y) & 0xFF;
			Store(addr, reg_x);
			cycles += 4;
		}
			break;
		case 0x97: {
		}
			break;
		case 0x98: {
			reg_a = reg_y;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x99: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_y;
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 5;
		}
			break;
		case 0x9A: {
			reg_sp = reg_x;
			cycles += 2;
		}
			break;
		case 0x9B: {
		}
			break;
		case 0x9C: {
		}
			break;
		case 0x9D: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 5;
		}
			break;
		case 0x9E: {
		}
			break;
		case 0x9F: {
		}
			break;
		case 0xA0: {
			uint16_t addr = reg_pc++;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0xA1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0xA2: {
			uint16_t addr = reg_pc++;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xA3: {
		}
			break;
		case 0xA4: {
			uint16_t addr = Peek16(reg_pc++);
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 3;
		}
			break;
		case 0xA5: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0xA6: {
			uint16_t addr = Peek16(reg_pc++);
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 3;
		}
			break;
		case 0xA7: {
		}
			break;
		case 0xA8: {
			reg_y = reg_a;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xA9: {
			uint16_t addr = reg_pc++;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xAA: {
			reg_x = reg_a;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xAB: {
		}
			break;
		case 0xAC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xAD: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xAE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xAF: {
		}
			break;
		case 0xB0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x01)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xB1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			//printf("<%x>\n",addr);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0xB2: {
		}
			break;
		case 0xB3: {
		}
			break;
		case 0xB4: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xB5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xB6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_y) & 0xFF;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xB7: {
		}
			break;
		case 0xB8: {
			reg_ps &= 0xBF;
			cycles += 2;
		}
			break;
		case 0xB9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xBA: {
			reg_x = reg_sp;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xBB: {
		}
			break;
		case 0xBC: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xBD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xBE: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xBF: {
		}
			break;
		case 0xC0: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xC1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 6;
		}
			break;
		case 0xC2: {
		}
			break;
		case 0xC3: {
		}
			break;
		case 0xC4: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xC5: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xC6: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 5;
		}
			break;
		case 0xC7: {
		}
			break;
		case 0xC8: {
			reg_y++;
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0xC9: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xCA: {
			reg_x--;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xCB: {
		}
			break;
		case 0xCC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xCD: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xCE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xCF: {
		}
			break;
		case 0xD0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x02)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xD1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 5;
		}
			break;
		case 0xD2: {
		}
			break;
		case 0xD3: {
		}
			break;
		case 0xD4: {
		}
			break;
		case 0xD5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xD6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xD7: {
		}
			break;
		case 0xD8: {
			reg_ps &= 0xF7;
			cycles += 2;
		}
			break;
		case 0xD9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xDA: {
		}
			break;
		case 0xDB: {
		}
			break;
		case 0xDC: {
		}
			break;
		case 0xDD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xDE: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xDF: {
		}
			break;
		case 0xE0: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xE1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 6;
		}
			break;
		case 0xE2: {
		}
			break;
		case 0xE3: {
		}
			break;
		case 0xE4: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xE5: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 3;
		}
			break;
		case 0xE6: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 5;
		}
			break;
		case 0xE7: {
		}
			break;
		case 0xE8: {
			reg_x++;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xE9: {
			uint16_t addr = reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 2;
		}
			break;
		case 0xEA: {
			cycles += 2;
		}
			break;
		case 0xEB: {
		}
			break;
		case 0xEC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xED: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xEE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xEF: {
		}
			break;
		case 0xF0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x02)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xF1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 5;
		}
			break;
		case 0xF2: {
		}
			break;
		case 0xF3: {
		}
			break;
		case 0xF4: {
		}
			break;
		case 0xF5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xF6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xF7: {
		}
			break;
		case 0xF8: {
			reg_ps |= 0x08;
			cycles += 2;
		}
			break;
		case 0xF9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xFA: {
		}
			break;
		case 0xFB: {
		}
			break;
		case 0xFC: {
		}
			break;
		case 0xFD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xFE: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xFF: {
		}
			break;
		}
//#ifdef DEBUG
//		if (should_irq && !(reg_ps & 0x04)) {
//			should_irq = false;
//			stack[reg_sp --] = reg_pc >> 8;
//			stack[reg_sp --] = reg_pc & 0xFF;
//			reg_ps &= 0xEF;
//			stack[reg_sp --] = reg_ps;
//			reg_pc = PeekW(IRQ_VEC);
//			reg_ps |= 0x04;
//			cycles += 7;
//		}
//		executed_insts ++;
//		if (executed_insts % 6000 == 0) {
//			timer1_cycles += CYCLES_TIMER1;
//			clock_buff[4] ++;
//			if (should_wake_up) {
//				should_wake_up = false;
//				ram_io[0x01] |= 0x01;
//				ram_io[0x02] |= 0x01;
//				reg_pc = PeekW(RESET_VEC);
//			} else {
//				ram_io[0x01] |= 0x08;
//				should_irq = true;
//			}
//		}
//#else
		if (cycles >= timer0_cycles) {
			timer0_cycles += CYCLES_TIMER0;
			timer0_toggle = !timer0_toggle;
			if (!timer0_toggle) {
				AdjustTime();
			}
			if (!IsCountDown() || timer0_toggle) {
				ram_io[0x3D] = 0;
			} else {
				ram_io[0x3D] = 0x20;
				nc1020_states.clock_flags &= 0xFD;
			}
			should_irq = true;
		}
		if (should_irq && !(reg_ps & 0x04)) {
			if(enable_debug_switch||enable_dyn_debug){
				printf("doing irq!\n");
			}
			should_irq = false;
			stack[reg_sp --] = reg_pc >> 8;
			stack[reg_sp --] = reg_pc & 0xFF;
			reg_ps &= 0xEF;
			stack[reg_sp --] = reg_ps;
			reg_pc = PeekW(IRQ_VEC);
			reg_ps |= 0x04;
			cycles += 7;
		}
		if (cycles >= timer1_cycles) {
			timer1_cycles += CYCLES_TIMER1;

			nc1020_states.clock_buff[4] ++;
			if (should_wake_up) {
				should_wake_up = false;
				ram_io[0x01] |= 0x01;
				ram_io[0x02] |= 0x01;
				reg_pc = PeekW(RESET_VEC);
			} else {
				ram_io[0x01] |= 0x08;
				should_irq = true;
			}
		}
		if(should_irq && (enable_debug_pc ||enable_dyn_debug)&&false)
			printf("should irq!\n");
}
