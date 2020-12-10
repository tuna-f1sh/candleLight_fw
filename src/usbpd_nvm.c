#include "i2c.h"
#include "usbpd_nvm.h"

#define STUSB4500_I2C_SLAVE_ADDR_7BIT  0x28
#define STUSB4500_I2C_SLAVE_ADDR_8BIT  0x50
#define I2C_Write_USB_PD(b,c,d) I2C_Write_USB_PD(STUSB4500_I2C_SLAVE_ADDR_7BIT, b,c,d)
#define I2C_Read_USB_PD(b,c,d) I2C_Read_USB_PD(STUSB4500_I2C_SLAVE_ADDR_7BIT, b,c,d)

// Default
/* static uint8_t sector[5][8] = {{0x00,0x00,0xB0,0xAA,0x00,0x45,0x00,0x00}, */
/*                                {0x10,0x40,0x9C,0x1C,0xFF,0x01,0x3C,0xDF}, */
/*                                {0x02,0x40,0x0F,0x00,0x32,0x00,0xFC,0xF1}, */
/*                                {0x00,0x19,0x56,0xAF,0xF5,0x35,0x5F,0x00}, */
/*                                {0x00,0x4B,0x90,0x21,0x43,0x00,0x40,0xFB}}; */


// SBC-CAN: Only enable VBUS when PD compatable and supplying 12 V 1.5 A (profile 2 - PDOK2)
static uint8_t sector[5][8] = {{0x00,0x00,0xB0,0xAA,0x00,0x45,0x00,0x00},
                               {0x10,0x40,0x9C,0x1C,0xF0,0x01,0x00,0xDF},
                               {0x02,0x40,0x0F,0x00,0x32,0x00,0xFC,0xF1},
                               {0x00,0x19,0x15,0xAF,0xF5,0x35,0x5F,0x00},
                               {0x00,0x3C,0x90,0x59,0x42,0x00,0x48,0xFB}};

void CUST_EnterWriteMode(unsigned char ErasedSector);
void CUST_EnterReadMode();
void CUST_ReadSector(char SectorNum, unsigned char *SectorData);
void CUST_WriteSector(char SectorNum, unsigned char *SectorData);
void CUST_ExitTestMode();

void nvm_flash(void)
{
  CUST_EnterWriteMode(SECTOR_0 |SECTOR_1  |SECTOR_2 |SECTOR_3  | SECTOR_4 );

  CUST_WriteSector(0,&sector[0][0]);
  CUST_WriteSector(1,&sector[1][0]);
  CUST_WriteSector(2,&sector[2][0]);
  CUST_WriteSector(3,&sector[3][0]);
  CUST_WriteSector(4,&sector[4][0]);

  CUST_ExitTestMode();
}

void nvm_read(void)
{
  uint8_t Buffer[2];
  //Read Current Parameters
  //-Enter Read Mode
  //-Read Sector[x][-]
  //---------------------------------
  //Enter Read Mode
  Buffer[0]=FTP_CUST_PASSWORD;  /* Set Password 0x95->0x47*/
  I2C_Write_USB_PD(FTP_CUST_PASSWORD_REG,Buffer,1);

  Buffer[0]= 0; /* NVM internal controller reset 0x96->0x00*/
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

  Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits 0x96->0xC0*/
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

  //--- End of CUST_EnterReadMode

  for(uint8_t i=0;i<5;i++)
  {
    Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits 0x96->0xC0*/
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

    Buffer[0]= (READ & FTP_CUST_OPCODE);  /* Set Read Sectors Opcode 0x97->0x00*/
    I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1);

    Buffer[0]= (i & FTP_CUST_SECT) |FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);  /* Load Read Sectors Opcode */

    do
    {
      I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ); //The FTP_CUST_REQ is cleared by NVM controller when the operation is finished.

    I2C_Read_USB_PD(RW_BUFFER,&sector[i][0],8);
  }

  CUST_ExitTestMode();
}


void soft_reset() {
  uint8_t set = 1;
  I2C_Write_USB_PD(0x23, &set, 1);
}

uint8_t setPowerAbove5vOnly(uint8_t value)
{
  if(value != 0) value = 1;

  // load POWER_ONLY_ABOVE_5V (sector 4, byte 6, bit 3)
  // only set if not already set
  if (((sector[4][6] >> 3) & 1) != value) {
    sector[4][6] &= 0xF7; //clear bit 3
    sector[4][6] |= (value<<3); //set bit 3
    return 1;
  } else {
    return 0;
  }
}

void setVoltage(uint8_t pdo_numb, float voltage)
{
  //Constrain voltage variable to 5-20V
  if(voltage < 5) voltage = 5;
  else if(voltage > 20) voltage = 20;

  //PDO1 Fixed at 5V

  if(pdo_numb == 2) //PDO2
  {
	sector[4][1] = voltage/0.2; //load Voltage (sector 4, byte 1, bits 0:7)
  }
  else //PDO3
  {
	// Load voltage (10-bit)
    // -bit 8:9 - sector 4, byte 3, bits 0:1
    // -bit 0:7 - sector 4, byte 2, bits 0:7
    uint16_t setVoltage = voltage/0.05;   //convert voltage to 10-bit value
    sector[4][2] = 0xFF & setVoltage;   //load bits 0:7
    sector[4][3] &= 0xFC;               //clear bits 0:1
    sector[4][3] |= (setVoltage>>8);    //load bits 8:9
  }
}

void setCurrent(uint8_t pdo_numb, float current)
{
  /*Convert current from float to 4-bit value
    -current from 0.5-3.0A is set in 0.25A steps
    -current from 3.0-5.0A is set in 0.50A steps
  */
  if(current < 0.5)     current = 0;
  else if(current <= 3) current = (4*current)-1;
  else                  current = (2*current)+5;

  if(pdo_numb == 1) //PDO1
  {
	//load current (sector 3, byte 2, bits 4:7)
    sector[3][2] &= 0x0F;             //clear bits 4:7
    sector[3][2] |= ((int)current<<4);    //load new amperage for PDO1
  }
  else if(pdo_numb == 2) //PDO2
  {
	//load current (sector 3, byte 4, bits 0:3)
    sector[3][4] &= 0xF0;             //clear bits 0:3
    sector[3][4] |= (int)current;     //load new amperage for PDO2
  }
  else //PDO3
  {
	//load current (sector 3, byte 5, bits 4:7)
    sector[3][5] &= 0x0F;           //clear bits 4:7
    sector[3][5] |= ((int)current<<4);  //set amperage for PDO3
  }
}

void setLowerVoltageLimit(uint8_t pdo_numb, uint8_t value)
{
  //Constrain value to 5-20%
  if(value < 5) value = 5;
  else if(value > 20) value = 20;

  //UVLO1 fixed

  if(pdo_numb == 2) //UVLO2
  {
    //load UVLO (sector 3, byte 4, bits 4:7)
    sector[3][4] &= 0x0F;             //clear bits 4:7
    sector[3][4] |= (value-5)<<4;  //load new UVLO value
  }
  else if(pdo_numb == 3) //UVLO3
  {
    //load UVLO (sector 3, byte 6, bits 0:3)
    sector[3][6] &= 0xF0;
    sector[3][6] |= (value-5);
  }
}

void setUpperVoltageLimit(uint8_t pdo_numb, uint8_t value)
{
  //Constrain value to 5-20%
  if(value < 5) value = 5;
  else if(value > 20) value = 20;

  if(pdo_numb == 1) //OVLO1
  {
    //load OVLO (sector 3, byte 3, bits 4:7)
    sector[3][3] &= 0x0F;             //clear bits 4:7
    sector[3][3] |= (value-5)<<4;  //load new OVLO value
  }
  else if(pdo_numb == 2) //OVLO2
  {
    //load OVLO (sector 3, byte 5, bits 0:3)
    sector[3][5] &= 0xF0;             //clear bits 0:3
    sector[3][5] |= (value-5);     //load new OVLO value
  }
  else if(pdo_numb == 3) //OVLO3
  {
    //load OVLO (sector 3, byte 6, bits 4:7)
    sector[3][6] &= 0x0F;
    sector[3][6] |= ((value-5)<<4);
  }
}

void setFlexCurrent(float value)
{
  //Constrain value to 0-5A
  if(value > 5) value = 5;
  else if(value < 0) value = 0;

  uint16_t flex_val = value*100;

  sector[4][3] &= 0x03;                 //clear bits 2:6
  sector[4][3] |= ((flex_val&0x3F)<<2); //set bits 2:6

  sector[4][4] &= 0xF0;                 //clear bits 0:3
  sector[4][4] |= ((flex_val&0x3C0)>>6);//set bits 0:3
}

void setPdoNumber(uint8_t value)
{
  if(value > 3) value = 3;

  //load PDO number (sector 3, byte 2, bits 2:3)
  sector[3][2] &= 0xF9;
  sector[3][2] |= (value<<1);
}

void setExternalPower(uint8_t value)
{
  if(value != 0) value = 1;

  //load SNK_UNCONS_POWER (sector 3, byte 2, bit 3)
  sector[3][2] &= 0xF7; //clear bit 3
  sector[3][2] |= (value)<<3;
}

void setUsbCommCapable(uint8_t value)
{
  if(value != 0) value = 1;

  //load USB_COMM_CAPABLE (sector 3, byte 2, bit 0)
  sector[3][2] &= 0xFE; //clear bit 0
  sector[3][2] |= (value);
}

void setConfigOkGpio(uint8_t value)
{
  if(value < 2) value = 0;
  else if(value > 3) value = 3;

  //load POWER_OK_CFG (sector 4, byte 4, bits 5:6)
  sector[4][4] &= 0x9F; //clear bit 3
  sector[4][4] |= value<<5;
}

void setGpioCtrl(uint8_t value)
{
  if(value > 3) value = 3;

  //load GPIO_CFG (sector 1, byte 0, bits 4:5)
  sector[1][0] &= 0xCF; //clear bits 4:5
  sector[1][0] |= value<<4;
}

/*
   FTP Registers
   o FTP_CUST_PWR (0x9E b(7), ftp_cust_pwr_i in RTL); power for FTP
   o FTP_CUST_RST_N (0x9E b(6), ftp_cust_reset_n_i in RTL); reset for FTP
   o FTP_CUST_REQ (0x9E b(4), ftp_cust_req_i in RTL); request bit for FTP operation
   o FTP_CUST_SECT (0x9F (2:0), ftp_cust_sect1_i in RTL); for customer to select between sector 0 to 4 for read/write operations (functions as lowest address bit to FTP, remainders are zeroed out)
   o FTP_CUST_SER[4:0] (0x9F b(7:4), ftp_cust_ser_i in RTL); customer Sector Erase Register; controls erase of sector 0 (00001), sector 1 (00010), sector 2 (00100), sector 3 (01000), sector 4 (10000) ) or all (11111).
   o FTP_CUST_OPCODE[2:0] (0x9F b(2:0), ftp_cust_op3_i in RTL). Selects opcode sent to
   FTP. Customer Opcodes are:
   o 000 = Read sector
   o 001 = Write Program Load register (PL) with data to be written to sector 0 or 1
   o 010 = Write Sector Erase Register (SER) with data reflected by state of FTP_CUST_SER[4:0]
   o 011 = Read Program Load register (PL)
   o 100 = Read SER;
   o 101 = Erase sector 0 to 4  (depending upon the mask value which has been programmed to SER)
   o 110 = Program sector 0  to 4 (depending on FTP_CUST_SECT1)
   o 111 = Soft program sector 0 to 4 (depending upon the value which has been programmed to SER)*/

void CUST_EnterWriteMode(unsigned char ErasedSector)
{
  unsigned char Buffer[2];

  Buffer[0]=FTP_CUST_PASSWORD;   /* Set Password*/
  I2C_Write_USB_PD(FTP_CUST_PASSWORD_REG,Buffer,1);

  Buffer[0]= 0 ;   /* this register must be NULL for Partial Erase feature */
  I2C_Write_USB_PD(RW_BUFFER,Buffer,1);

  {  
    //NVM Power-up Sequence
    //After STUSB start-up sequence, the NVM is powered off.

    Buffer[0]=0;  /* NVM internal controller reset */
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

    Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits */
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);
  }

  Buffer[0]=((ErasedSector << 3) & FTP_CUST_SER) | ( WRITE_SER & FTP_CUST_OPCODE) ;  /* Load 0xF1 to erase all sectors of FTP and Write SER Opcode */
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1); /* Set Write SER Opcode */

  Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ ; 
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1); /* Load Write SER Opcode */
  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ); 
  Buffer[0]=  SOFT_PROG_SECTOR & FTP_CUST_OPCODE ;  
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1);  /* Set Soft Prog Opcode */

  Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ ; 
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1); /* Load Soft Prog Opcode */

  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ);
  Buffer[0]= ERASE_SECTOR & FTP_CUST_OPCODE ;  
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1); /* Set Erase Sectors Opcode */

  Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ ;  
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1); /* Load Erase Sectors Opcode */

  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ);	



}

void CUST_EnterReadMode()
{
  unsigned char Buffer[2];

  Buffer[0]=FTP_CUST_PASSWORD;  /* Set Password*/
  I2C_Write_USB_PD(FTP_CUST_PASSWORD_REG,Buffer,1);

  {  
    //NVM Power-up Sequence
    //After STUSB start-up sequence, the NVM is powered off.

    Buffer[0]=0;  /* NVM internal controller reset */
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

    Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits */
    I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);
  }
}

void CUST_ReadSector(char SectorNum, unsigned char *SectorData)
{
  unsigned char Buffer[2];

  Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits */
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

  Buffer[0]= (READ & FTP_CUST_OPCODE);
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1);/* Set Read Sectors Opcode */

  Buffer[0]= (SectorNum & FTP_CUST_SECT) |FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);  /* Load Read Sectors Opcode */
  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ);

  I2C_Read_USB_PD(RW_BUFFER,&SectorData[0],8); /* Sectors Data are available in RW-BUFFER @ 0x53 */

  Buffer[0] = 0 ;  /* NVM internal controller reset */
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);


}

void CUST_WriteSector(char SectorNum, unsigned char *SectorData)
{
  unsigned char Buffer[2];

  //Write the 64-bit data to be written in the sector
  I2C_Write_USB_PD(RW_BUFFER,SectorData,8);

  Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N; /*Set PWR and RST_N bits*/
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

  //NVM Program Load Register to write with the 64-bit data to be written in sector
  Buffer[0]= (WRITE_PL & FTP_CUST_OPCODE); /*Set Write to PL Opcode*/
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1);

  Buffer[0]=FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;  /* Load Write to PL Sectors Opcode */  
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1);

  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }		 
  while(Buffer[0] & FTP_CUST_REQ) ;  //FTP_CUST_REQ clear by NVM controller


  Buffer[0]= (PROG_SECTOR & FTP_CUST_OPCODE);
  I2C_Write_USB_PD(FTP_CTRL_1,Buffer,1);/*Set Prog Sectors Opcode*/

  Buffer[0]=(SectorNum & FTP_CUST_SECT) |FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,1); /* Load Prog Sectors Opcode */  

  do 
  {
    I2C_Read_USB_PD(FTP_CTRL_0,Buffer,1); /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ); //FTP_CUST_REQ clear by NVM controller

}

void CUST_ExitTestMode()
{
  unsigned char Buffer[2];

  Buffer[0]= FTP_CUST_RST_N; Buffer[1]=0x00;  /* clear registers */
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,2);
  Buffer[0]=0x00; 
  I2C_Write_USB_PD(FTP_CUST_PASSWORD_REG,Buffer,1);  /* Clear Password */

}
