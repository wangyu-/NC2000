#include <cstring>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "comm.h"
#include "dsp/dsp.h"
#include "iv_uart.h"
#include "nor.h"
#include "settings.h"
using namespace std;
extern WqxRom nc2k_rom;
void print_help(){
    printf("help:\n");
    printf("  nc2000/2600/1020 emulator\n");
    printf("  check https://github.com/wangyu-/NC2000 for usage\n");
}
static string rom_path;
int listen_port=9000;
void process_args(int argc, char *argv[])
{
	int i, j, k;
	int opt;
	bool timer01_speed_fix_set=0;
	static struct option long_options[] = {
		{"port", required_argument, 0, 1},
		{"cpu", required_argument, 0, 1},
		{"loop", required_argument, 0, 1},
		{"io", required_argument, 0, 1},
		{"oc", required_argument, 0, 1},
		{"rtc-speed", required_argument, 0, 1},
		{"nor-read", required_argument, 0, 1},
		{"nor-write", required_argument, 0, 1},
		{"nc1020", no_argument, 0, 1},
		{"pc1000", no_argument, 0, 1},
		{"nc2000", no_argument, 0, 1},
		{"nc3000", no_argument, 0, 1},
		{"no-keepon", no_argument, 0, 1},
		{"no-sync", no_argument, 0, 1},
		{"no-sync-on-resume", no_argument, 0, 1},
		{"debug-beeper", no_argument, 0, 1},
		{"debug-dsp", no_argument, 0, 1},
		{"debug-timer", no_argument, 0, 1},
		{"power-save", required_argument, 0, 1},
		{"rom", required_argument, 0, 1},
		{"pixel-size", required_argument, 0, 1},
		{"gap-size", required_argument, 0, 1},
		{"lcd-scale", required_argument, 0, 1},
		{"slice", required_argument, 0, 1},
		{"cpu-batch", required_argument, 0, 1},
		{"lcd-inner-refresh", required_argument, 0, 1},
		{"lcd-outer-refresh", required_argument, 0, 1},
		{"stripe", required_argument, 0, 1},
		{"timer01-speed", required_argument, 0, 1},
		{"load-state", no_argument, 0, 1},
		{"load-state-reset", no_argument, 0, 1},
		{"no-lcd-latency-effect", no_argument, 0, 1},
		{"state", required_argument, 0, 1},
		//{"auto-save-state", no_argument, 0, 1},
		{"auto-save-all", no_argument, 0, 1},
		{"auto-save-flash", no_argument, 0, 1},
		{"cks", no_argument, 0, 1},
		{"debug-cks", no_argument, 0, 1},
		{"pro-keyboard", no_argument, 0, 1},
		{"no-nand-forced-erase", no_argument, 0, 1},
		{"log-level", required_argument, 0, 1},
		{"lcd-effect", required_argument, 0, 1},
		{"log-on-key-press", required_argument, 0, 1},
		{"debug-next-n", required_argument, 0, 1},
		{"log-all-dsp-io", no_argument, 0, 1},
		{"battery-level", required_argument, 0, 1},
		{"nc1020tw", no_argument, 0, 1},
		{"patch-nc1020tw-nor", no_argument, 0, 1},
		{"oops", no_argument, 0, 1},
		{"quit-after-debug-next-n", no_argument, 0, 1},
		{"rgb-scale", required_argument, 0, 1},
		{"fast-forward-limit", required_argument, 0, 1},
		{"assert", no_argument, 0, 1},
		{"uart-log-level", required_argument, 0, 1},
		{"uart-passthrough", required_argument, 0, 1},
		{"uart-advance", no_argument, 0, 1},
		{NULL, 0, 0, 0}
	};
	string uart_dev_name;
	int option_index = 0;
    if (argc == 1)
	{
        //printf("no argument provided\n");
	}
	for (i = 0; i < argc; i++)
	{
		if(strcmp(argv[i],"-h")==0||strcmp(argv[i],"--help")==0||strcmp(argv[i],"?")==0||strcmp(argv[i],"/?")==0)
		{
			print_help();
			exit(0);
		}
	}

	while ((opt = getopt_long(argc, argv, "l:r:tuh:",long_options,&option_index)) != -1)
	{
		switch (opt)
		{
		case 'h':
			break;
		case 1:
			if(strcmp(long_options[option_index].name,"rom")==0)
			{
                rom_path = optarg;
			}
			else if(strcmp(long_options[option_index].name,"port")==0)
			{
                listen_port = atoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"nc1020")==0)
			{
				nc1020mode = true;
			}
			else if(strcmp(long_options[option_index].name,"pc1000")==0)
			{
				pc1000mode = true;
			}
			else if(strcmp(long_options[option_index].name,"nc2000")==0)
			{
				nc2000mode = true;
			}
			else if(strcmp(long_options[option_index].name,"nc3000")==0)
			{
				nc3000mode = true;
			}
			else if(strcmp(long_options[option_index].name,"debug-beeper")==0)
			{
				enable_debug_beeper = true;
			}
			else if(strcmp(long_options[option_index].name,"debug-dsp")==0)
			{
				enable_debug_dsp = true;
			}
			else if(strcmp(long_options[option_index].name,"debug-timer")==0)
			{
				enable_debug_timer = true;
			}
			else if(strcmp(long_options[option_index].name,"cpu")==0)
			{
				cpu_version = (CpuVersion)stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"loop")==0)
			{
				cpu_loop_version = (CpuLoopVersion)stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"nor-read")==0)
			{
				nor_read_format = (NorFormat)stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"nor-write")==0)
			{
				nor_write_format = (NorFormat)stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"power-save")==0)
			{
				power_save_interval = stoi(optarg);
				if (power_save_interval==0){
					power_save_interval = int_inf;
				}
			}
			else if(strcmp(long_options[option_index].name,"io")==0)
			{
				io_version = (IoVersion)stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"oc")==0)
			{
				oc_factor = stod(optarg);
			}
			else if(strcmp(long_options[option_index].name,"rtc-speed")==0)
			{
				rtc_speed = stod(optarg);
			}
			else if(strcmp(long_options[option_index].name,"pixel-size")==0)
			{
				pixel_size = stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"gap-size")==0)
			{
				gap_size = stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"lcd-scale")==0)
			{
				lcd_scale = stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"slice")==0)
			{
				SLICE_INTERVAL = stoi(optarg);
				if(SLICE_INTERVAL<1) SLICE_INTERVAL=1;
			}
			else if(strcmp(long_options[option_index].name,"lcd-inner-refresh")==0)
			{
				LCD_INNER_REFRESH_INTERVAL = stoi(optarg);
				if(LCD_INNER_REFRESH_INTERVAL<1) LCD_INNER_REFRESH_INTERVAL=1;
			}
			else if(strcmp(long_options[option_index].name,"lcd-outer-refresh")==0)
			{
				LCD_OUTER_REFRESH_INTERVAL = stoi(optarg);
				if(LCD_OUTER_REFRESH_INTERVAL<1) LCD_OUTER_REFRESH_INTERVAL=1;
			}
			else if(strcmp(long_options[option_index].name,"cpu-batch")==0)
			{
				cpu_batch = stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"stripe")==0)
			{
				lcdstripe_suffix = optarg;
			}
			else if(strcmp(long_options[option_index].name,"no-keepon")==0)
			{
				enable_keepon = false;
			}
			else if(strcmp(long_options[option_index].name,"no-sync")==0)
			{
				enable_auto_time_sync = false;
			}
			else if(strcmp(long_options[option_index].name,"timer01-speed")==0)
			{
				timer01_speed_fix_set=true;
				timer01_speed_fix = stod(optarg);
			}
			else if (strcmp(long_options[option_index].name,"load-state")==0)
			{
				enable_load_state = true;
			}
			else if (strcmp(long_options[option_index].name,"load-state-reset")==0)
			{
				enable_load_state = true;
				reset_after_load_state = true;
			}
			else if (strcmp(long_options[option_index].name,"state")==0){
				nc2k_rom.statesPath = optarg;
				nc2k_rom.statesPath += ".state";
			}
			/*else if (strcmp(long_options[option_index].name,"auto-save-state")==0){
				save_state_on_exit = true;
			}*/
			else if (strcmp(long_options[option_index].name,"auto-save-all")==0){
				save_state_on_exit = true;
				save_flash_on_exit = true;
			}
			else if (strcmp(long_options[option_index].name,"auto-save-flash")==0){
				save_flash_on_exit = true;
			}
			else if (strcmp(long_options[option_index].name,"no-sync-on-resume")==0){
				sync_on_resume = false;
			}
			else if (strcmp(long_options[option_index].name,"cks")==0){
				enable_emulate_cks = true;
			}
			else if (strcmp(long_options[option_index].name,"debug-cks")==0){
				enable_debug_cks = true;
			}
			else if (strcmp(long_options[option_index].name,"no-lcd-latency-effect")==0){
				enable_lcd_latency_effect = false;
			}
			else if (strcmp(long_options[option_index].name,"pro-keyboard")==0){
				pro_key = true;
			}
			else if (strcmp(long_options[option_index].name,"no-nand-forced-erase")==0){
				forced_erase_before_write = false;
			}
			else if (strcmp(long_options[option_index].name,"log-level")==0){
				debug_level = stoi(optarg);
			}
			else if (strcmp(long_options[option_index].name,"lcd-effect")==0){
				string effect = optarg;
				if(effect.find(",")!=string::npos){
					sscanf(effect.c_str(),"%d/%d,%d/%d",
						&lcd_effect_charge_a, &lcd_effect_charge_b,
						&lcd_effect_discharge_a, &lcd_effect_discharge_b);
				}else{
					sscanf(effect.c_str(),"%d/%d", &lcd_effect_charge_a, &lcd_effect_charge_b);
					lcd_effect_discharge_a = lcd_effect_charge_a;
					lcd_effect_discharge_b = lcd_effect_charge_b;
				}
			}
			else if (strcmp(long_options[option_index].name,"log-on-key-press")==0){
				log_on_key_press = stoi(optarg);
			}
			else if (strcmp(long_options[option_index].name,"debug-next-n")==0){
				enable_dyn_debug_next_n = stoi(optarg);
			}
			else if (strcmp(long_options[option_index].name,"log-all-dsp-io")==0){
				log_all_dsp_io = true;
			}
			else if (strcmp(long_options[option_index].name,"battery-level")==0){
				battery_level = stoi(optarg);
			}
			else if (strcmp(long_options[option_index].name,"oops")==0){
				enable_oops = true;
			}
			else if (strcmp(long_options[option_index].name,"nc1020tw")==0){
				nc1020mode = true;
				nc1020tw_mode = true;
			}
			else if (strcmp(long_options[option_index].name,"quit-after-debug-next-n")==0){
				enable_quit_after_debug_next_n = true;
			}
			else if (strcmp(long_options[option_index].name,"assert")==0){
				enable_assert = true;
			}
			else if (strcmp(long_options[option_index].name,"rgb-scale")==0){
				sscanf(optarg,"%lf,%lf,%lf",&r_scale,&g_scale,&b_scale);
			}
			else if (strcmp(long_options[option_index].name,"patch-nc1020tw-nor")==0){
				patch_nc1020tw_nor = true;
			}
			else if (strcmp(long_options[option_index].name,"fast-forward-limit")==0){
				fast_forward_limit = stod(optarg);
			}
			else if (strcmp(long_options[option_index].name,"uart-log-level")==0){
				uart_log_level = stoi(optarg);
			}
			else if(strcmp(long_options[option_index].name,"uart-passthrough")==0){
				uart_dev_name = optarg;
			}
			else if(strcmp(long_options[option_index].name,"uart-advance")==0){
				uart_advance = true;
			}
			else
			{
				printf("unknown option\n");
				print_help();
				exit(-1);
			}
			break;
		default:
			printf("unknown option <%x>\n", opt);
			print_help();
			exit(-1);
		}
	}
	if (!timer01_speed_fix_set){
		timer01_speed_fix = 1.0/oc_factor;
	}

	set_dsp_log_level(debug_level);

	int mode_cnt=0;
	mode_cnt+= nc1020mode;
	mode_cnt+= pc1000mode;
	mode_cnt+= nc2000mode;
	mode_cnt+= nc3000mode;
	if(mode_cnt==0){
		printf("no mode specified, default to nc2000\n");
		nc2000mode = true;
	}
	if(mode_cnt>1){
		printf("only one of --nc1020, --pc1000, --nc2000, --nc3000 can be specified\n");
		exit(-1);
	}

	if(nc2000mode){
		if(rom_path.empty()){
			rom_path = "roms/nc2000";
		}
  	    nc2k_rom.nandFlashPath = rom_path + ".nand";
		nc2k_rom.nand0Path = rom_path + ".nand0";
        nc2k_rom.norFlashPath = rom_path + ".nor";
    }
	if(nc1020mode){
		string default_rom_path;//without suffix
		if(nc1020tw_mode) default_rom_path = "roms/nc1020tw";
		else default_rom_path = "roms/nc1020";
		string default_with_suffix= default_rom_path + ".rom";

		if(rom_path.empty()){
			rom_path = default_rom_path; //without suffix
		}
		nc2k_rom.romPath = rom_path + ".rom";
		nc2k_rom.norFlashPath = rom_path + ".nor";
		nor_info_block[8]=0xfc;
		nor_info_block[9]=0x03;

		if(rom_path != default_with_suffix){
			if (!fileExists(nc2k_rom.romPath.c_str())) {
				if(fileExists(default_with_suffix)){
					//if given rom not exist, try default rom instead (since rom can not be changed, the file can be re-used)
					printf("WARN: file %s does not exist, but default %s exists, use default instead\n", nc2k_rom.romPath.c_str(), default_with_suffix.c_str());
					nc2k_rom.romPath = default_with_suffix;
				}
			}
		}

	}
	if(pc1000mode){
		if(rom_path.empty()){
			rom_path = "roms/pc1000";
		}
		nc2k_rom.romPath = rom_path + ".rom";
		nc2k_rom.norFlashPath = rom_path + ".nor";
	}

	if(nc3000mode){
		if(rom_path.empty()){
			rom_path = "roms/nc3000";
		}
		nc2k_rom.nand0Path = rom_path + ".nand0";
		nc2k_rom.nandFlashPath = rom_path + ".nand";
		nc2k_rom.norFlashPath = rom_path + ".nor";
	}

	if(nc2k_rom.statesPath.empty()){
		nc2k_rom.statesPath=rom_path + ".state";
	}

	if(lcdstripe_suffix.empty()){
		if(pixel_size+gap_size==5){
			lcdstripe_suffix = "w938";
		}else{
			lcdstripe_suffix = "w1313";
		}
	}

	if(!uart_dev_name.empty()){
		open_serial_port((char*)uart_dev_name.c_str());
	}

}
