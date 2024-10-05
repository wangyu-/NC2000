#ifndef _CC800_IO_NAME_H
#define _CC800_IO_NAME_H

#define io00_bank_switch    0x00
#define io01_int_status     0x01 // read
#define io01_int_enable     0x01 // write
#define io02_timer0_val     0x02 //rw
#define io03_timer1_val     0x03
#define io04_stop_timer0    0x04 //r
#define io04_general_ctrl   0x04 //w
#define io05_start_timer0   0x05 //r
#define io05_clock_ctrl     0x05 //w
#define io06_stop_timer1    0x06 //r
#define io06_lcd_config     0x06 //w
#define io07_start_timer1   0x07 //r
#define io07_port_config    0x07 //w
#define io08_port0_data     0x08 //rw
#define io09_port1_data     0x09 //rw
#define io0A_bios_bsw       0x0A
#define io0A_roa            0x0A
#define io0B_port3_data     0x0B
#define io0B_lcd_ctrl       0x0B
#define io0C_general_status 0x0C // r
#define io0C_timer01_ctrl   0x0C
#define io0C_lcd_config     0x0C
#define io0D_volumeid       0x0D
#define io0D_lcd_segment    0x0D
#define io0E_dac_data       0x0E
#define io0F_zp_bsw         0x0F //rw
#define io0F_port0_dir      0x0F //rw
#define io15_port1_dir      0x15 //w
#define io16_port2_dir      0x16
#define io17_port2_data     0x17
#define io18_port4_data     0x18
#define io19_ckv_select     0x19
#define io1A_volume_set     0x1A
#define io1B_pwm_data       0x1B
#define io1C_batt_detect    0x1C
#define io1E_batt_detect    0x1E
#define io20_JG             0x20
#define io23_unknow         0x23

#define io_ROA_bit          0x80 // RAM/ROM (io_bios_bsw)

#define map0000 0
#define map2000 1
#define map4000 2
#define map6000 3
#define map8000 4
#define mapA000 5
#define mapC000 6
#define mapE000 7

#endif