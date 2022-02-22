#include <main.h>
//variables=====================================================================
unsigned short adc_value = 0;
signed short temperature;

float setting_T;
unsigned short delta;
unsigned short sample_time=1; //minutes
unsigned char second=0;
unsigned char minute=0;
unsigned char second_set=0;

char bt1_temp = 1;
char bt2_temp = 1;
char bt3_temp = 1;
char bt4_temp = 1;

short bt1_count;
short bt2_count;
short bt2_longpress=0;
short bt3_count;
short bt4_count;

char led_flicker = 0;
char sleep;
char show_mode = 0; // 0 is temp, 1 is upper, 2 is lower, 3 is addres
//F: turn off - 4: input password
unsigned short password = 1234;
unsigned short input_password;
char setting_mode = 0;
char Fn;

//int led1_count;
//int led2_count;       
short led3_count;
short led4_count;
unsigned int timer4_buf = 0;

extern char modbus_slave_addr;
extern int buffer_count;
extern int char_time;
extern char start_char_time;
extern char start_receive;

//float A = 1.009249522e-03, B = 2.378405444e-04, C = 2.019202697e-07; //NTC 10k
float A = 0.9656447848e-03, B = 2.106930774e-04, C = 0.858331228e-07; //NTC 50k
//float A = 0.965e-03, B = 2.107e-04, C = 0.858e-07; //NTC 50k
float T,logRt,Tc;

//unsigned char seg7_hex[]={0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90,0xbf};
unsigned char seg7_hex[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f}; //CC
//Interupt======================================================================
#pragma vector = UART2_R_RXNE_vector //0x17
__interrupt void uart2_int_handler(void)
{
  //buffer=UART2_DR;
  //uart2_send_str("Hello/n");
  //Receive_handler_ascii(buffer);
  receive_handler_rtu(UART2_DR);
  UART2_SR_RXNE = 0;
}
#pragma vector = TIM4_OVR_UIF_vector //0x19
__interrupt void timer4_int_handler(void)
{
  if(start_char_time==1)
    char_time++;
  else
    char_time=0;
  if(char_time>40)
  {
    start_char_time = 0;
    start_receive = 0;
    if(buffer_count>0)
    {
      modbus_rtu_handler();
      buffer_count = 0;
    }
  }
  TIM4_SR = 0; // Clear flag
}
#pragma vector = TIM3_OVR_UIF_vector //0x11
__interrupt void timer3_int_handler(void)
{
  adc_read();
  second++;
  if(setting_mode==1)
  {
    second_set++;
    if(second_set>=sample_time*60) //after x min idle
    {
      write_data_eeprom();
      setting_mode=0;
      show_mode=0;
      second_set=0;
    }
  }
  if(second == 60)
  {
    second=0;
    minute++;
  }
  if(minute == sample_time)
  {
    minute=0;
    if(Tc > setting_T + delta)
    {
      relay_cool=1;
      relay_heat=0;
    }
    else if (Tc <  setting_T - delta)
    {
      relay_cool=0;
      relay_heat=1;
    }
    else
      relay_cool=relay_heat=0;
  }
  //write_eeprom(temperature,0x03);
  //write_eeprom(relay_heat,0x04);
  //write_eeprom(relay_cool,0x05);
  TIM3_SR1_UIF = 0;
}
void interrupt_config(void)
{
  ITC_SPR6 |= 0xC0;           // uart2 level 3
  ITC_SPR5 |= 0x01<<2;        //timer3 level 1        
  ITC_SPR7 |= 0x02<<2;        //timer4 level 2      
  /*ITC_SPR2 |= 0b11<<2       // exti0 level = 3 (highest)
  ITC_SPR2 |= 0b01<<4         // exti1 level = 1 
  ITC_SPR2 |= 0b01<<6         // exti2 level = 1
  ITC_SPR3 |= 0b01            // exti3 level = 1*/
  asm("rim") ; //enable global interrupt
}
//MAIN==========================================================================
void main(void)
{
  stm8_init();
  gpio_init();
  uart2_init();
  timer2_init();
  timer3_init();
  timer4_init();
  interrupt_config();
  reload_data_eeprom();
  adc_init();
  adc_read();
  
  while(1)
  {
    power_button();
    if(sleep==0)
    {
      button_handler();
      if(bt2_longpress==1)
      {
        setting_show(Fn);
        continue;
      }
      if(setting_mode == 0)
      {
        if (show_mode==0)
          if(adc_value==0)
            error_show(NO_NTC);
          else
            tem_show(Tc);
        else if (show_mode==1) 
          setting_show(modbus_slave_addr);
        else
          setting_show(delta);
      }
      else
      {
        if (show_mode==1)
          setting_show(modbus_slave_addr);
        else if (show_mode==2)
          setting_show(delta);
        else if (show_mode==3)
          tem_show(setting_T);
        else if (show_mode==4)
          setting_show(sample_time);
      }
    }
  }
}
//Functions=====================================================================
signed char get_data(short addr)
{
  switch (addr)
  {
  case 0:
    return modbus_slave_addr;
    break;
  case 1:
    return 0;
    break;
  case 2:
    return 0;
    break;
  case 3:
    return (signed short)(Tc);
    break;
  case 4:
    return relay_heat;
    break;
  case 5:
    return relay_cool;
    break;
  case 6:
    return sleep;
    break;
  default:
    return 0x00;
    break;
  }
}
void reload_data_eeprom(void)
{
  eeprom_rw_enable();
  modbus_slave_addr =         read_eeprom(0x00);
  delta  =                    read_eeprom(0x01);
  unsigned int byte =         read_eeprom(0x02)*0x1000000;
  byte |=                     read_eeprom(0x03)*0x10000;
  byte |=                     read_eeprom(0x04)<<8;
  byte |=                     read_eeprom(0x05);
  setting_T =  *((float*)&byte);
  relay_heat =                read_eeprom(0x06)&0x01;
  relay_cool =                read_eeprom(0x07)&0x01;
  sample_time =               read_eeprom(0x08);
}
void write_data_eeprom(void)
{
  write_eeprom(modbus_slave_addr,0x00);
  write_eeprom(delta,0x01);
  unsigned int byte = *((unsigned int*)&setting_T);
  write_eeprom(byte&0xFF,0x05);
  byte>>=8;
  write_eeprom(byte&0xFF,0x04);
  byte>>=8;
  write_eeprom(byte&0xFF,0x03);
  byte>>=8;
  write_eeprom(byte&0xFF,0x02);
  write_eeprom(relay_heat,0x06);
  write_eeprom(relay_cool,0x07);
  write_eeprom(sample_time,0x08);
}
void delay(long int time)
{
  while (time--);
}
void seg7(unsigned char hex)
{
  seg_a  = hex&0x01;
  seg_b  = (hex>>1)&0x01;
  seg_c  = (hex>>2)&0x01;
  seg_d  = (hex>>3)&0x01;
  seg_e  = (hex>>4)&0x01;
  seg_f  = (hex>>5)&0x01;
  seg_g  = (hex>>6)&0x01;
  seg_dp = (hex>>7)&0x01;
}
//LED===========================================================================
void tem_show(float value) 
{
  //first digit
  seg7(0x00);
  led_1 = 0;
  led_2 = 1;
  led_3 = 1;
  led_4 = 1;
  if(value<0)
  {
    seg7(key__);
    value = - value;
  }
  delay(5);
  //Second digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 0;
  led_3 = 1;
  led_4 = 1;
  seg7(seg7_hex[(short)value/10]);
  delay(5);
  //Third digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 0;
  led_4 = 1;
  seg7(0x80|seg7_hex[(short)value%10]);
  delay(5);
  //Forth digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 1;
  led_4 = 0;
  seg7(seg7_hex[(short)(value*10)%10]);
  delay(5);
}
void error_show(char code) 
{
  //first digit
  seg7(0x00);
  led_1 = 0;
  led_2 = 1;
  led_3 = 1;
  led_4 = 1;
  if(code == NO_NTC)
  {
    seg7(key_E);
  }
  delay(5);
  //Second digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 0;
  led_3 = 1;
  led_4 = 1;
  if(code == NO_NTC)
  {
    seg7(key_r);
  }
  delay(5);
  //Third digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 0;
  led_4 = 1;
  if(code == NO_NTC)
  {
    seg7(key_r);
  }
  delay(5);
  //Forth digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 1;
  led_4 = 0;
  seg7(seg7_hex[code]);
  delay(5);
}
void setting_show(short value)
{
  //first digit
  seg7(0x00);
  led_1 = 0;
  led_2 = 1;
  led_3 = 1;
  led_4 = 1;
  if(bt2_longpress == 1)
    seg7(key_F);
  else
    if (show_mode==1)
      seg7(key_A);
    else if (show_mode==2)
      seg7(key_d);
    else if (show_mode==4)
      seg7(seg7_hex[5]); //S key
  delay(10);
  //Second digit
  seg7(0x00); // blank
  //Third digit
  led_1 = 1;
  led_2 = 1;
  led_3 = 0;
  led_4 = 1;
  seg7(seg7_hex[(value)/10]);
  delay(10);
  //Forth digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 1;
  led_4 = 0;
  seg7(seg7_hex[(value)%10]);
  delay(5);
}
void led_show(short value)
{
  //first digit
  seg7(0x00);
  led_1 = 0;
  led_2 = 1;
  led_3 = 1;
  led_4 = 1;
  seg7(seg7_hex[value/1000]); 
  delay(10);
  //Second digit
  seg7(0x00); 
  led_1 = 1;
  led_2 = 0;
  led_3 = 1;
  led_4 = 1;
  seg7(seg7_hex[(value/100)%10]);
  delay(10);
  //Third digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 0;
  led_4 = 1;
  seg7(seg7_hex[(value/10)%10]);
  delay(10);
  //Forth digit
  seg7(0x00);
  led_1 = 1;
  led_2 = 1;
  led_3 = 1;
  led_4 = 0;
  seg7(seg7_hex[(value)%10]);
  delay(5);
}
void power_button(void)
{
//Button 1 -------------------------------------------------------------------
  if (bt1==0) //power off button
  {
    if(bt1_temp == 1)
    {
      timer2_set_couter(0);
      bt1_temp = 0;
    }
    else
    {
      if(timer2_get_couter() > 100) //Long press
      {
        if(sleep==0)
          sleep=1;
        else
          sleep=0;
        bt1_temp=1;
        led_1 = led_2 = led_3 = led_4 = 1;
        write_eeprom(sleep,0x06);
      }
    }
    return;
  }
  else
  {
    if(bt1_temp==0) //Release
    {
      bt1_temp = 1;
      if(timer2_get_couter()<100) //Short press
      {
        if(setting_mode==1)
        {
          write_data_eeprom();
          setting_mode=0;
        }
        show_mode=0;   
      }
    }
  }
}
void button_handler(void)
{
  //Button 2 -------------------------------------------------------------------
  if (bt2==0)
  {
    if(bt2_temp==1) // setting button
    {
      bt2_temp = 0;
      timer2_set_couter(0);
    }
    else
    {
      if(timer2_get_couter() > 100) //longpress
      {
        if(bt2_longpress==0 && setting_mode == 0)
        {
          Fn=1;
          bt2_longpress = 1;
          setting_mode = 1;
        }
      }
    }
  }   
  else
  {
    if(bt2_temp == 0) //release
    {
      if(bt2_longpress==1)
      {
        show_mode=Fn;
        bt2_longpress=0;
      }
      bt2_temp = 1;
    }
  }	
  //Button 3 -------------------------------------------------------------------
  //up
  if (bt3==0)
  {
    if(bt3_temp==1) //Press
    {
      bt3_temp=0;
      if(bt2_longpress==1) // still holding
      {
        if(Fn<4)
          Fn++;
        else
          Fn=1;
      }
      else
      {
        if(setting_mode==0)
        {
          if(show_mode<2)
            show_mode++;
          else
            show_mode=0;
        }
        else
        {
          if(show_mode==1)
          {
            if(modbus_slave_addr<99)
              modbus_slave_addr++;
          }
          else if(show_mode==2)
          {
            if(delta<99)
              delta++;
          }
          else if(show_mode==3)
          {
            if(setting_T<99.9)
              if(bt2==0)
                setting_T+=1.0;
              else
              setting_T+=0.1;
          }
          else if(show_mode==4)
          {
            if(sample_time<99)
              sample_time++;
          }
        }
      }
    }
  }   
  else
  {
    if(bt3_temp==0) //release
      bt3_temp=1;
  }
  //Button 4 -------------------------------------------------------------------
  //down
  if (bt4==0)
  {
    if(bt4_temp==1) //Press
    {
      bt4_temp=0;
      if(bt2_longpress==1) // still holding
      {
        if(Fn>1)
          Fn--;
        else
          Fn=4;
      }
      else
      {
        if(setting_mode==0)
        {
          if(show_mode>0)
            show_mode--;
          else
            show_mode=2;
        }
        else
        {
          if(show_mode==1)
          {
            if(modbus_slave_addr>1)
              modbus_slave_addr--;
          }
          else if(show_mode==2)
          {
            if(delta>0)
              delta--;
          }
          else if(show_mode==3)
          {
            if(setting_T>-99.9)
              if(bt2==0)
                setting_T-=1.0;
              else
              setting_T-=0.1;
          }
          else if(show_mode==4)
          {
            if(sample_time>1)
              sample_time--;
          }
        }
      }
    }
  }   
  else
  {
    if(bt4_temp==0) //release
      bt4_temp=1;
  }
}
//Initialization================================================================
void stm8_init(void)
{ 
  //CLK_ICKR_LSIEN = 0;         // Low speed internal RC off
  CLK_ICKR_HSIEN = 0;         // High speed internal RC off
  CLK_ECKR_HSEEN = 1;         // High speed external clock on
  
  CLK_SWR = 0xB4;             // HSE selected as master clock source B4
  //CLK_SWCR_SWIEN = 0;       // Clock switch interrupt disabled
  //CLK_CKDIVR_HSIDIV = 0;    // fHSI= fHSI RC output
  CLK_CKDIVR_CPUDIV = 0;      // fCPU=fMASTER
  CLK_PCKENR1 |= 1<<6;        // Timer 3
  CLK_PCKENR1 |= 1<<4;        //Timer 4
  CLK_PCKENR1 |= 1<<3;        // UART2
  CLK_PCKENR2 |= 1<<3;        // ADC
}
/*--GPIO Pin map================================================================
PA1 - RCC OSCIN
PA2 - RCC OSCOUT
PB4 - ADC IN4
PD1 - SYS SWIM
PD5 - UART2 TX
PD6 - UART2 RX
*/
void gpio_init(void)
{
  //PD7,4,3,2,0: output push-pull,high speed 10MHz
  PD_DDR |= 0x9D;
  PD_CR1 |= 0x9D;
  PD_CR2 |= 0x9D;
  //PC7-1: output push-pull,high speed 10MHz
  PC_DDR |= 0xFE;
  PC_CR1 |= 0xFE;
  PC_CR2 |= 0xFE;
  //PB0-PB3 - Input,no interrupt, pull up
  PB_DDR &= 0xF0; 
  PB_CR1 |= 0x0F;
  PB_CR2 &= 0xF0;
  //PB_CR2 |= 0x0F; // Use interrupt
  //PB5: output push-pull,high speed 10MHz
  PB_DDR_DDR5 = 1;
  PB_CR1_C15 = 1;
  PB_CR2_C25 = 1;
  //PF4: output push-pull,high speed 10MHz
  PF_DDR_DDR4 = 1;
  PF_CR1_C14 = 1;
  PF_CR2_C24 = 1;
}
//Timer=========================================================================
void timer4_init(void)
{
  TIM4_CR1_CEN = 1; //Counter enable
  TIM4_CR1_URS = 1;
  TIM4_CR1_OPM = 0;
  TIM4_CR1_ARPE= 1; 
  TIM4_IER_UIE = 1; //Update interrupt enabled
  TIM4_PSCR = 0x07; //8MHz / 2^7 ~ 62500
  TIM4_ARR = 0x06;
}
void timer3_init(void)
{
  TIM3_CR1_CEN = 1; //Counter enable
  TIM3_CR1_URS = 1;
  TIM3_CR1_OPM = 0;
  TIM3_CR1_ARPE= 1; 
  TIM3_IER_UIE = 1; //Update interrupt enabled
  TIM3_PSCR = 0x07; //8MHz / 2^(7+1) ~ 31250Hz
  TIM3_ARRH = 0x3D; 
  TIM3_ARRL = 0x09; //15625
}
void timer2_init(void)
{
  TIM2_CR1_CEN = 1; //Counter enable
  TIM3_CR1_OPM = 0;
  TIM3_CR1_ARPE= 1; 
  TIM2_PSCR = 0x0F; //8MHz / 2^15 ~ 244Hz
  TIM2_ARRH = 0xFF; 
  TIM2_ARRL = 0xFF;
}
short timer2_get_couter(void)
{
  short counter = TIM2_CNTRH<<8;
  counter |= TIM2_CNTRL;
  return counter;
}
void timer2_set_couter(short value)
{
  TIM2_CNTRL = value&0xFF;
  TIM2_CNTRH = value>>8;
}
//UART==========================================================================
void uart2_init()
{
  UART2_CR1_M = 0; //1 Start bit, 8 Data bits, n Stop bit 
  UART2_CR3_STOP = 0; //1 Stop bit
  UART2_CR1_PCEN = 0; //Parity check 
  //UART2_CR1_PS = 0; //Even parity
  UART2_CR2_TEN = 0; 
  UART2_CR2_REN = 0;
  UART2_CR2_RIEN = 1; //  Receiver interrupt enable
  UART2_BRR1 = 0x0D; //Baud 9600
  UART2_BRR2 = 0x00;
  UART2_CR2_TEN = 1; //Transmitter enable
  UART2_CR2_REN = 1; //Receiver enable
}
void uart2_send_char(char data)
{
  while(UART2_SR_TXE == 0);
  UART2_DR = data; 
}
void uart2_send_msg(char *data, char size)
{
  for(int i=0;i<size;i++)
    uart2_send_char(*data++);
}
void uart2_send_str(char *data)
{
  while(*data)
    uart2_send_char(*data++);
} 
//ADC===========================================================================
void adc_init(void)
{
  //ADC_CSR_AWDIE = 0; //AWD interrupt disabled
  ADC_CR1_SPSEL = 0x7; //fADC = fMASTER/18
  ADC_CR1_CONT = 1;  //0=Single conversion mode 
  ADC_CR2_ALIGN = 1;//Right alignment
  //ADC_CSR_CH = 0x5; // Use PB5 channel 5
  //ADC_CR2_EXTSEL = 0x2;
  //ADC_CR2_EXTTRIG = 0;
  ADC_CSR_CH = 4;
  //ADC_CR2_ALIGN = 1;
  ADC_CR1_CONT = 0;
}
void adc_read(void)
{
  //ADC_CSR_CH = 4;
  //ADC_CR1_SPSEL = 2;
  //ADC_CR2_ALIGN = 1;
  //ADC_CR1_CONT = 0;
  ADC_CR1_ADON = 1;
  ADC_CR1_ADON = 1;
  while(ADC_CSR_EOC == 0);
  ADC_CSR_EOC = 0;
  adc_value = ADC_DRL;
  adc_value |= ADC_DRH<<8;
  //ADC_CR1_ADON = 0;                    
  logRt = log(10000.0 * ((1023.0/adc_value - 1.0 )));
  T = (1.0 / (A + B*logRt + C*logRt*logRt*logRt));
  Tc = T - 273.15; // Convert Kelvin to Celsius
  //temperature = (signed short)(Tc);
}
//EEPROM========================================================================
void eeprom_rw_enable(void)
{
  if((FLASH_IAPSR & 8) == 0)
  {
    FLASH_DUKR = 0xAE;
    FLASH_DUKR = 0x56;
  }
}
void eeprom_rw_disable(void)
{
  FLASH_IAPSR &= 0xf7;
}
void write_eeprom(unsigned char data, unsigned short eep_address)
{
  unsigned char *address;
  address = 0x4000 + (unsigned char*)eep_address;
  *address = data;
}
unsigned char read_eeprom(unsigned short eep_address)
{
  unsigned char data;
  char *address;
  address  = 0x4000 + (char*)eep_address;
  data = *address;
  return data;
}