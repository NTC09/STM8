/*---Specifications:
A/D Channels Available: 9
Clock Frequency Max: 16 MHz
Core: STM8
Data Bus Width: 8 bit
Height: 1.4 mm
Interface: I2C, SPI, UART
Length: 10 mm
Mounting Style: SMD/SMT
Number of I/O Lines: 34
Number of Timers: 8
Number of bits in ADC: 10 bit
On-Chip ADC: Yes
Operating Temperature Range: - 40 C to + 85 C
Package / Case: LQFP
Packaging Type: Tube
Processor Series: STM8S10x
Program Memory Size: 16 Kb
Program Memory Type: Flash
RAM Size: 2 Kb
Supply Voltage Range: 2.95 V to 5.5 V
Width: 10 mm
RoHS: Yes
*/
#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <math.h>
#include <iostm8s105k4.h>
#include <intrinsics.h>
#include <modbus.h>
#include <stdbool.h>

//Define========================================================================
#define led_1  PD_ODR_ODR7
#define seg_a  PD_ODR_ODR4
#define seg_f  PD_ODR_ODR3
#define led_2  PD_ODR_ODR2
#define led_3  PD_ODR_ODR0
#define seg_b  PC_ODR_ODR7

#define seg_e  PC_ODR_ODR6
#define seg_d  PC_ODR_ODR5
#define seg_dp PC_ODR_ODR4
#define seg_c  PC_ODR_ODR3
#define seg_g  PC_ODR_ODR2
#define led_4  PC_ODR_ODR1

#define relay_cool PF_ODR_ODR4
#define relay_heat PB_ODR_ODR5

#define bt1 PB_IDR_IDR0
#define bt2 PB_IDR_IDR1
#define bt3 PB_IDR_IDR2
#define bt4 PB_IDR_IDR3
//EEP ROM 0x00 4000 -> 0x00 43FF ~ 1 Kbyte
#define NO_NTC      0x01
#define key_C       0x39
#define key_L       0x38
#define key_U       0x3E
#define key_A       0x77
#define key__       0x40
#define key_T       0x45
#define key_d       0x5E
#define key_F       0x71
#define key_E       0x79
#define key_r       0x50
//Funtions define===============================================================
void stm8_init(void);
void gpio_init(void);
void interrupt_config(void);
void uart2_init(void);
void uart2_send_char(char data);
void uart2_send_msg(char *data,char size);
void uart2_send_str(char *data);
void adc_init(void);
void adc_read(void);
void seg7(unsigned char hex);
void led_show(short value);
void tem_show(float value);
void error_show(char code);
void setting_show(short value);
void delay(long int time);
void button_handler(void);
void power_button(void);
void eeprom_rw_enable(void);
void eeprom_rw_disable(void);
void write_eeprom(unsigned char data,unsigned short eep_address);
unsigned char read_eeprom(unsigned short eep_address);
void reload_data_eeprom(void);
void write_data_eeprom(void);
void timer2_init(void);
short timer2_get_couter(void);
void timer2_set_couter(short value);
void timer4_init(void);
void timer3_init(void);
signed char get_data(short addr);
#endif