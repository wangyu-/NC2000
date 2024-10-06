#include "comm.h"

struct cpu_states_t {
	uint16_t reg_pc;
	uint8_t reg_a;
	uint8_t reg_ps;
	uint8_t reg_x;
	uint8_t reg_y;
	uint8_t reg_sp;
};

struct nc1020_states_t{
	uint32_t version;
	////////////cpu_states_t cpu;
	uint8_t ram[0x8000];
	uint8_t ext_ram[0x8000];
	//uint8_t ext_ram2[0x8000];

	uint8_t bak_40[0x40];

	uint8_t clock_buff[80];
	uint8_t clock_flags;

	uint8_t jg_wav_data[0x20];
	uint8_t jg_wav_flags;
	uint8_t jg_wav_idx;
	bool jg_wav_playing;

	uint8_t fp_step;
	uint8_t fp_type;
	uint8_t fp_bank_idx;
	uint8_t fp_bak1;
	uint8_t fp_bak2;
	uint8_t fp_buff[0x100];

	bool slept;
	bool should_wake_up;
	bool pending_wake_up;
	uint8_t wake_up_flags;

	bool timer0_toggle;
	uint64_t cycles;
	uint64_t unknown_timer_cycles;
	uint64_t timer0_cycles;
	uint64_t timer1_cycles;
	uint64_t timebase_cycles;
	uint64_t nmi_cycles;
	/////////bool should_irq;

	uint32_t lcd_addr;
	uint8_t keypad_matrix[8];

	bool grey_mode;
	//long long previous_cycles;
};
