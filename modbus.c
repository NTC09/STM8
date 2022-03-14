#include <modbus.h>

char modbus_slave_addr;
char request_command[20];

char check_sum;
char buffer_count = 0;
char start_receive = 0;
int char_time = 0;
char start_char_time = 0;
char _sum;

int max_eep_addr = 0x3FF; // 0x00 43FF

struct Response_data{
  char data[100];
  int byte_count;
  int size;
  char check_sum;
} rp;
//RTU code =====================================================================
void receive_handler_rtu(char rData)
{
  start_char_time = 0;
  if(start_receive == 1)
  {
    request_command[buffer_count++]= rData;
    start_char_time = 1;
  }
  else if(rData == modbus_slave_addr)
  {
    request_command[0] = rData;
    buffer_count=1;
    start_receive = 1;
    start_char_time = 1;
  }
}
void modbus_rtu_handler(void)
{
  if (CRC16(request_command, buffer_count-2) != 
      (request_command[buffer_count-2] | request_command[buffer_count-1]<<8))
  return;
  function_rtu_handler();
}
void function_rtu_handler(void)
{
  rp.data[0] = modbus_slave_addr;
  rp.data[1] = request_command[1];
  switch(rp.data[1])
  {
  case 0x03: //Read_Holding_Registers
    {
    int first_addr = (request_command[2]<<0x08) | request_command[3];
    int quantity  = (request_command[4]<<0x08) | request_command[5];
    rp.byte_count = 0;
    rp.size = 3;
    for(int i=first_addr; i < quantity  + first_addr; i++)
    {
      signed int temp = (signed int)read_eeprom(i);
      rp.data[rp.size] = temp>>0x8;
      rp.data[rp.size+1] = temp&0xFF;
      rp.size+=2;
      rp.byte_count+=2;
    }
    rp.data[2] = rp.byte_count;
    int crc_cal = CRC16(rp.data, rp.size);
    rp.size+=2;
    rp.data[rp.size-1] = crc_cal>>0x8;
    rp.data[rp.size-2] = crc_cal&0xFF; 
    uart2_send_msg(rp.data,rp.size);
    break;
    }
  case 0x06: //Write_Single_Register
    {
    int register_addr = request_command[2]<<0x08;
    register_addr |= request_command[3];
    int wr_data =  request_command[4]<<0x08;
    wr_data|= request_command[5];
    write_eeprom((signed char)wr_data,register_addr);
    reload_data_eeprom();
    uart2_send_msg(request_command,buffer_count);
    break;
    }
  case 0x10: //Write_Multiple_Registers
    {
    int first_addr = request_command[2]<<0x08;
    first_addr |= request_command[3];
    int quantity  = request_command[4]<<0x08;
    quantity  |= request_command[5];
    //int _byte_count = request_command[6];
    int i=7;
    for(int _addr=first_addr; _addr<first_addr + quantity; _addr++ )
    {
      int wr_data =  request_command[i]<<0x08;
      wr_data|= request_command[i+1];
      write_eeprom((signed char)wr_data,_addr);
      i+=2;
    }
    reload_data_eeprom();
    //Response
    rp.data[2]=request_command[2];
    rp.data[3]=request_command[3];
    rp.data[4]=request_command[4];
    rp.data[5]=request_command[5];
    int crc_cal = CRC16(rp.data, rp.size);
    rp.data[7] = crc_cal>>0x8;
    rp.data[6] = crc_cal&0xFF; 
    uart2_send_msg(rp.data,8);
    break;
    }
  }
}
//ASCII=========================================================================
void Receive_handler_ascii(char rData)
{
  if(rData == CR)
  {
    start_receive = 0;
    if(modbus_ascii_handler() == '0')
      err_respond();
    return;
  }
  if(start_receive == 1)
  {
    request_command[buffer_count++]= rData;
    return;
  }
  if(rData == ':')
  {
    buffer_count = 0;
    start_receive = 1;
    return;
  }
}
//Handler main function=========================================================
char modbus_ascii_handler(void)
{
  char received_address;
  received_address = hex(request_command[0])<<0x4;
  received_address |= hex(request_command[1]);
  if(received_address != modbus_slave_addr)
    return '0';
  check_sum = hex(request_command[buffer_count-1]);
  check_sum |= hex(request_command[buffer_count-2])<<4;
  _sum = asc_check_sum(request_command,buffer_count-2);
  if(_sum != check_sum)
    return '0';
  rp.data[0] = asc(modbus_slave_addr>>0x4);
  rp.data[1] = asc(modbus_slave_addr&0xF);
  rp.data[2] = request_command[2]; //Function
  rp.data[3] = request_command[3];
  char Function = (hex(request_command[2])<<4) + hex(request_command[3]);
  Function_code_handler(Function);
    return '1';
}
void err_respond(void)
{
  uart2_send_char(CR);
  uart2_send_char(LF);
}
//Function code handler=========================================================
void Function_code_handler(char Function)
{
  for(char i=4;i<50;i++)
    rp.data[i] = 0;
  switch(Function)
  {
    case Read_Holding_Registers:
      holding_registers_read();
      break;
    case Write_Multiple_Registers:
      write_multi_registers();
      break;
    case Write_Single_Register:
      
      break;
  }
}
char asc(char input) // 0x0 to 0xF => '0' to 'F'
{
  if(input > 0x9) return input + 0x37;
  else return input + 0x30;
}
char hex(char input) //'0' to 'F' => 0x0 to 0xF
{
  if(input > '9') return input - 0x37;
  else return input - 0x30;
}
void int_to_4asc(int _hex, char *_key1,char *_key2,char *_key3, char *_key4)
{
  *_key1 = asc((_hex&0xF000)>>0xC);
  *_key2 = asc((_hex&0x0F00)>>0x8);
  *_key3 = asc((_hex&0x00F0)>>0x4);
  *_key4 = asc((_hex&0x000F));
}
void asc4_to_int(int *_int, 
      char _key1,char _key2,char _key3, char _key4)
{
  *_int  = hex(_key1) <<0xC;
  *_int |= hex(_key2) <<0x8;
  *_int |= hex(_key3) <<0x4;
  *_int |= hex(_key4);
}
//Output Coils==================================================================
void holding_registers_read(void)
{
  int first_addr;
  asc4_to_int(&first_addr 
  ,request_command[4]
  ,request_command[5]
  ,request_command[6]
  ,request_command[7]);
  int num_addr;
  asc4_to_int(&num_addr 
  ,request_command[8]
  ,request_command[9]
  ,request_command[10]
  ,request_command[11]);
  unsigned char j=6;
  rp.byte_count = 0;
  for (int i=first_addr; i < first_addr + num_addr 
                        && i <= max_eep_addr; i++)
  {
    int_to_4asc(read_eeprom(i),
    &rp.data[j],&rp.data[j+1],&rp.data[j+2],&rp.data[j+3]);
    j+=4;
    rp.byte_count+=2;
  }
  rp.data[4] = asc(rp.byte_count>>4);
  rp.data[5] = asc(rp.byte_count&0xF);
  rp.check_sum = asc_check_sum(rp.data,j+1);
  rp.data[j] = asc(rp.check_sum>>4);
  rp.data[j+1] = asc(rp.check_sum&0xF);
  uart2_send_char(':');
  uart2_send_msg(rp.data,rp.size);
  uart2_send_char(CR);
  uart2_send_char(LF);
}
void write_single_registers(void)
{
  reload_data_eeprom();
}
void write_multi_registers(void)
{
  int first_addr;
  asc4_to_int(&first_addr 
  ,request_command[4]
  ,request_command[5]
  ,request_command[6]
  ,request_command[7]);
  int num_addr;
  asc4_to_int(&num_addr 
  ,request_command[8]
  ,request_command[9]
  ,request_command[10]
  ,request_command[11]);
  char byte_count = hex(request_command[12]);
  byte_count |= hex(request_command[13])<<4;
  
  write_eeprom(10, 0x00);
  
  uart2_send_char(':');
  for(int i=0; i<rp.byte_count ;i++)
    rp.data[i] = request_command[i];
  
  uart2_send_char(CR);
  uart2_send_char(LF);
}
//LRC check=====================================================================
char asc_check_sum(char *nData, int wLength)
{
  char nLRC = 0 ; // LRC char initialized
  for (int i = 0; i < wLength; i+=2)
    nLRC += ((hex(nData[i])<<4) | (hex(nData[i+1])&0x0F));
  return -nLRC;
}
//CRC check=====================================================================
unsigned int CRC16 (char *puchMsg, char usDataLen )
{
  unsigned char uchCRCHi = 0xFF ; /* high byte of CRC initialized */
  unsigned char uchCRCLo = 0xFF ; /* low byte of CRC initialized */
  unsigned uIndex ; /* will index into CRC lookup table */
  while (usDataLen--) /* pass through message buffer */
  {
    uIndex = uchCRCLo ^ *puchMsg++ ; /* calculate the CRC */
    uchCRCLo = uchCRCHi ^ auchCRCHi[uIndex] ;
    uchCRCHi = auchCRCLo[uIndex] ;
  }
  return (uchCRCHi << 8 | uchCRCLo) ;
}