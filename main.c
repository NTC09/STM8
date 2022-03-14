#include <main.h>
//variables=====================================================================
unsigned int adc_value = 0;
//unsigned int adc_value_old = 0;

float setting_T;
float buffer_T;
unsigned int delta;
unsigned int sample_time=1; //minutes
unsigned int second=0;
unsigned int minute=0;
unsigned int second_set=0;

char bt1_temp = 1;
char bt2_temp = 1;
char bt3_temp = 1;
char bt4_temp = 1;

//int bt1_count;
//int bt2_count;
int bt2_longpress=0;
//int bt3_count;
//int bt4_count;

int sleep;
int show_mode = 0;
/*unsigned int password = 1234;
unsigned int input_password;*/
int setting_mode = 0;
int Fn;

//unsigned int timer4_buf = 0;
//unsigned char adc_update=0;
//unsigned char rtu_update=0;

extern char modbus_slave_addr;
extern int buffer_count;
extern int char_time;
extern char start_char_time;
extern char start_receive;

//const float A = 1.009249522e-3, B = 2.378405444e-4, C = 2.019202697e-7; //NTC 10k
//float A=1.128470071e-3, B=2.342569703e-4, C= 0.8698130160e-7;
//float A=1.129329295e-3, B=2.349793430e-4, C=0.7736609222e-7;
//float A = 0.9656447848e-3, B = 2.106930774e-4, C = 0.858331228e-7; //NTC 50k
//float A = 0.965e-03, B = 2.107e-04, C = 0.858e-07; //NTC 50k

/*const float A = 0.001110864337533712; 1.02192985237609E-03 
const float B = 0.0002402386598987505; 2.41453242427025E-04 
const float C = -4.99706118262111e-007; -2.47620754758454E-07 
const float D = 9.325597005727104e-008;*/ //1.65394923419592E-07

float T,logRt,Tc,R;
float L1, L2, L3, A, B, C;
float Y1, Y2, Y3;
float y2, y3;
//unsigned char seg7_hex[]=
//{0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90,0xbf}; //CA
char seg7_hex[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f}; //CC
char led7_count = 0;
unsigned char led7_value[4];
//Interupt======================================================================
#pragma vector = TIM1_OVR_UIF_vector //0x0D
__interrupt void timer1_int_handler(void) //1ms
{
  seg7(0x00);
  if(sleep == 0)
  {
    if(led7_count == 0)//Firstdigit
    {
      led_1 = 0;
      led_4 = 1;
      seg7(led7_value[0]);
      led7_count = 1;
    }
    else if(led7_count == 1)//Second digit
    {
      led_1 = 1;
      led_2 = 0;
      seg7(led7_value[1]);
      led7_count = 2;
    }
    else if(led7_count == 2)//Third digit
    {
      led_2 = 1;
      led_3 = 0;
      seg7(led7_value[2]);
      led7_count = 3;
    }
    else if(led7_count == 3)//Forth digit
    {
      led_3 = 1;
      led_4 = 0;
      seg7(led7_value[3]);
      led7_count = 0;
    }
  }
  TIM1_SR1=0;
}
#pragma vector = UART2_R_RXNE_vector //0x17
__interrupt void uart2_int_handler(void)
{
  receive_handler_rtu(UART2_DR);
  UART2_SR_RXNE = 0;
}
#pragma vector = TIM4_OVR_UIF_vector //0x19
__interrupt void timer4_int_handler(void) //1ms
{
  TIM4_SR = 0; // Clear flag
  //Modbus timeout
  if(start_char_time==1)
    char_time++;
  else
    char_time=0;
  if(char_time>10)
  {
    start_char_time = 0;
    char_time = 0;
    start_receive = 0;
    if(buffer_count>0)
    {
      modbus_rtu_handler();
      buffer_count = 0;
    }
  }
}
#pragma vector = TIM3_OVR_UIF_vector //0x11
__interrupt void timer3_int_handler(void)
{
  TIM3_SR1_UIF = 0;
  second++;
  if(setting_mode==0||bt2_longpress==0)
    adc_read();
  if(setting_mode==1 || bt2_longpress==1)
  {
    second_set++;
    if(second_set>=30) //after 15 secs idle
    {
      //write_data_eeprom();
      setting_mode=0;
      bt2_longpress=0;
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
    write_eeprom(relay_heat,0x04);
    write_eeprom(relay_cool,0x05);
  }
}
void interrupt_config(void)
{
  //EXTI_CR1_PBIS = 2;
  ITC_SPR4 = 0x03<<2;        //timer1 level 3
  ITC_SPR6 = 0x01<<6;        // uart2 level 1
  ITC_SPR5 = 0x00<<2;        //timer3 level 1        
  ITC_SPR7 = 0x02<<2;        //timer4 level 2      
  //asm("rim") ; //enable global interrupt
}
//MAIN==========================================================================
void main(void)
{
  //Initalization
  stm8_init();
  gpio_init();
  uart2_init();
  timer1_init();
  timer2_init();
  timer3_init();
  timer4_init();
  reload_data_eeprom();
  adc_init();
  interrupt_config();
  __enable_interrupt();
  adc_read();
 /*
  L1 = log(2207); 
  L2 = log(10000);
  L3 = log(77523);
  Y1 = 1/(273.15+70); 
  Y2 = 1/(273.15+25);
  Y3 = 1/(273.15-20);
  re_cal_ntc();*/                                                                                                                                                                                                                                                                                                  
  //loop
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
      else
      {
        if(setting_mode == 0)
        {
          if (show_mode==0)
          {
            if(adc_value==0||adc_value==1023)
              error_show(NO_NTC);
            else
              tem_show(Tc);
          }
          else if (show_mode==1) 
            setting_show(modbus_slave_addr);
          else if(show_mode == 2)
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
          else if (show_mode==5)
          {
            tem_show(buffer_T);
          }
        }
      }
    }
  }
}
//Functions=====================================================================
void reload_data_eeprom(void)
{
  eeprom_rw_enable();
  modbus_slave_addr =         read_eeprom(0x00);
  delta  =                    read_eeprom(0x01);
  setting_T = load_float_eeprom(0x02);
  //relay_heat =                read_eeprom(0x06)&0x01;
  //relay_cool =                read_eeprom(0x07)&0x01;
  sample_time =               read_eeprom(0x08);
  buffer_T =  load_float_eeprom(0x09);
  A = load_float_eeprom(0x0D);
  B = load_float_eeprom(0x11);
  C = load_float_eeprom(0x15);
  L1 = load_float_eeprom(0x19);
  L2 = load_float_eeprom(0x1D);
  L3 = load_float_eeprom(0x21);
  Y1 = load_float_eeprom(0x25);
  Y2 = load_float_eeprom(0x29);
  Y3 = load_float_eeprom(0x2D);
  eeprom_rw_disable();
}
float load_float_eeprom(char start_addr)
{
  int byte = read_eeprom(start_addr)*0x1000000;
  byte |=    read_eeprom(start_addr+1)*0x10000;
  byte |=    read_eeprom(start_addr+2)<<8;
  byte |=    read_eeprom(start_addr+3);
  return *((float*)&byte);
}
void write_data_eeprom(void)
{
  eeprom_rw_enable();
  write_eeprom(modbus_slave_addr,0x00);
  write_eeprom(delta,0x01);
  write_float_eeprom(setting_T,0x02);
  write_eeprom(relay_heat,0x06);
  write_eeprom(relay_cool,0x07);
  write_eeprom(sample_time,0x08);
  write_float_eeprom(buffer_T,0x09);
  write_float_eeprom(A,0x0D);
  write_float_eeprom(B,0x11);
  write_float_eeprom(C,0x15);
  write_float_eeprom(L1,0x19);
  write_float_eeprom(L2,0x1D);
  write_float_eeprom(L3,0x21);
  write_float_eeprom(Y1,0x25);
  write_float_eeprom(Y2,0x29);
  write_float_eeprom(Y3,0x2D);
  eeprom_rw_disable();
}
void write_float_eeprom(float value,char startAddr)
{
  int byte = *((int*)&value);
  write_eeprom(byte&0xFF,startAddr+3);
  byte>>=8;
  write_eeprom(byte&0xFF,startAddr+2);
  byte>>=8;
  write_eeprom(byte&0xFF,startAddr+1);
  byte>>=8;
  write_eeprom(byte&0xFF,startAddr);
}
//LED===========================================================================
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
int power(int a,int b)
{
  if(b==0) return 1;
  if(b==1) return a;
  if(b%2==1)
    return a*power(a,b/2)*power(a,b/2);
  else
    return power(a,b/2)*power(a,b/2);
}
float old_value;
void tem_show(float value) 
{
  if(old_value==value && led7_value[1]!=0) return;
  old_value=value;
  int i=0;
  int count=0;
  int max = 3;
  if(value<0)
  {
    led7_value[i] = key__;
    i=i+1;
    max=max-1;
    value = - value;
  }
  while(count<max && value<power(10,max))
  {
    value*=10.0;
    count=count+1;
  }
  int temp = (int)value;
  while(i<4)
  {
    led7_value[i] = seg7_hex[(temp/power(10,max))%10];
    max=max-1;
    i=i+1;
  }
  if(count!=0)
    led7_value[3-count] += 0x80;
}
void error_show(int code) 
{
  if(code == NO_NTC)
  {
    led7_value[0]=key_E;
    led7_value[1]=key_r;
    led7_value[2]=key_r;
    led7_value[3]=seg7_hex[code];
  }
}
void setting_show(int value)
{
  //first digit
  if(bt2_longpress == 1)
    led7_value[0]=key_F;
  else
    if (show_mode==1)
      led7_value[0]=key_A;
    else if (show_mode==2)
      led7_value[0]=key_d;
    else if (show_mode==4)
      led7_value[0]=seg7_hex[5]; //S key
  //Second digit
  led7_value[1]=0x00; // blank
  //Third digit
  led7_value[2]=seg7_hex[(value)/10];
  //Forth digit
  led7_value[3]=seg7_hex[(value)%10];
}
void led_show(int value)
{
  //first digit
  led7_value[0]=seg7_hex[value/1000]; 
  //Second digit
  led7_value[1]=seg7_hex[(value/100)%10];
  //Third digit
  led7_value[2]=seg7_hex[(value/10)%10];
  //Forth digit
  led7_value[3]=seg7_hex[(value)%10];
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
      if(timer2_get_couter() > 150) //Long press
      {
        if(bt2_longpress==1) //reset
        {
          setting_T = 20;
          delta = 3;
          buffer_T = 0;
          modbus_slave_addr = 1;
          sample_time = 3;
          write_data_eeprom();
          setting_mode=0;
          bt2_longpress=0;
          show_mode=0;
          timer2_set_couter(0);
          return;
        }
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
      if(timer2_get_couter()<100) //int press
      {
        if(setting_mode==1)
        {
          if(show_mode == 5 && buffer_T !=0)
            re_cal_ntc();
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
      second_set = 0;
      timer2_set_couter(0);
      if(bt2_longpress==1)
      {
        show_mode=Fn;
        bt2_longpress=0;
        setting_mode = 1;
      }
    }
    else
    {
      if(timer2_get_couter() > 150) //longpress
      {
        if(bt2_longpress==0 && setting_mode == 0)
        {
          Fn=1;
          bt2_longpress = 1;
        }
      }     
    }
  }   
  else
  {
    if(bt2_temp == 0) //release
    {
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
      second_set=0;
      if(bt2_longpress==1) // still holding
      {
        if(Fn<Fmax)
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
          else if(show_mode==5)
          {
            if(buffer_T<99.9)
              if(bt2==0)
                buffer_T+=1.0;
              else
              buffer_T+=0.1;
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
      second_set=0;
      if(bt2_longpress==1) // still holding
      {
        if(Fn>1)
          Fn--;
        else
          Fn=Fmax;
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
          else if(show_mode==5)
          {
            if(buffer_T>-99.9)
              if(bt2==0)
                buffer_T-=1.0;
              else
              buffer_T-=0.1;
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
  //PB_CR2 |= 0xC0; // Use interrupt PB2 PB3
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
  TIM4_PSCR = 0x04; //8MHz / 2^(4+1) ~ 250000Hz
  TIM4_ARR = 0x7D;  //125000 / 125
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
void timer1_init(void)
{
  TIM1_CR1_CEN = 1; //Counter enable
  TIM1_CR1_URS = 1;
  TIM1_CR1_OPM = 0;
  TIM1_CR1_ARPE= 1; 
  TIM1_IER = 0x01; //Update interrupt enabled
  TIM1_PSCRH = 0x00;
  TIM1_PSCRL = 40; //8MHz / 2^(4+1) ~ 25000Hz
  TIM1_ARRH = 0x00; 
  TIM1_ARRL = 100; //125
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
int timer2_get_couter(void)
{
  int counter = TIM2_CNTRH<<8;
  counter |= TIM2_CNTRL;
  return counter;
}
void timer2_set_couter(int value)
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
  ADC_CR1_CONT = 0;  //0=Single conversion mode 
  ADC_CR2_ALIGN = 1;//Right alignment
  //ADC_CSR_CH = 0x5; // Use PB5 channel 5
  //ADC_CR2_EXTSEL = 0x2;
  //ADC_CR2_EXTTRIG = 0;
  ADC_CSR_CH = 4;
  //ADC_CR2_ALIGN = 1;
  ADC_CR1_ADON = 1;
}
void adc_read(void)
{
  //ADC_CR1_ADON = 1;
  ADC_CR1_ADON = 1;
  while(ADC_CSR_EOC == 0);
  ADC_CSR_EOC = 0;
  adc_value = ADC_DRL;
  adc_value |= ADC_DRH<<8;
  //ADC_CR1_ADON = 0;
  if(adc_value==0 || adc_value==1023)
    return;
  R = 10000.0 * (1023.0/adc_value - 1 );
  logRt = log(R);
  /*Tc = (1.0 / (A + B*logRt + C*logRt*logRt + D*logRt*logRt*logRt)) 
    - 273.15;*/
  Tc = (1.0 / (A + B*logRt + C*logRt*logRt*logRt)) - 273.15;
  //Tc = T - 273.15 + buffer_T; // Convert Kelvin to Celsius
  //temperature = (signed int)(Tc);
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
void write_eeprom(unsigned char data, unsigned int eep_address)
{
  unsigned char *address;
  address = 0x4000 + (unsigned char*)eep_address;
  *address = data;
}
unsigned char read_eeprom(unsigned int eep_address)
{
  unsigned char data;
  char *address;
  address  = 0x4000 + (char*)eep_address;
  data = *address;
  return data;
}
//NTC calculator================================================================
void re_cal_ntc()
{
  float buf;
  if (Tc > 40)
  {
    buf = (30.0/(Tc-40))*buffer_T;
    //L1 -= log(pow(0.95, buf));
    //L2 -= log(pow(0.95, buf*(25.0/Tc)));
    Y1 = 1/(273.15+70+buf); 
    //Y2 = 1/(273.15+25+buffer_T/2);
  }
  else if (Tc > 10)
  {
    /*if(Tc>0)
      buf = (25.0/(25.0-Tc))*buffer_T;
    else
      buf = (25.0/(25.0+Tc))*buffer_T;
    L2 -= log(pow(0.95, buf));*/
    buf = (30.0/(40-Tc))*buffer_T;
    //Y1 = 1/(273.15+70+buffer_T/2); 
    Y2 = 1/(273.15+25+buf);
    //Y3 = 1/(273.15-20+buffer_T/2);
  }
  else
  {
    /*buf = (-25.0/Tc)*buffer_T;
    L2 -= log(pow(0.95, buf*(-25.0/Tc)));
    L3 -= log(pow(0.95, buf));*/
    //Y2 = 1/(273.15+25+buffer_T*0.6);
    buf = (30.0/(10-Tc))*buffer_T;
    Y3 = 1/(273.15-20+buffer_T);
  }
    y2 = (Y2 - Y1)/(L2 - L1);
    y3 = (Y3 - Y1)/(L3 - L1);
    C = (y3 - y2) / (L3 - L2);
    C/= (L1 + L2 + L3);
    B = y2 - C*(L1*L1 + L1*L2 + L2*L2);
    A = Y1 - L1*(B + C*L1*L1);
}