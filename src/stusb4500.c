/*
 * STUSB4500 I2C Driver
 *
 * Largely based on ST firmware example: https://github.com/usb-c/STUSB4500/tree/master/Firmware/Project/Src
 * Support added by J.Whittington 2020
 *
 */
#include "stusb4500.h"
#include "canape.h"

USB_PD_SNK_PDO_TypeDef pdo_profile[3];
extern I2C_HandleTypeDef hi2c1;

static HAL_StatusTypeDef read_register(uint8_t device, uint8_t reg, uint8_t *data, uint8_t len)
{
  return HAL_I2C_Mem_Read(&hi2c1, (device << 1), (uint16_t) reg, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

static HAL_StatusTypeDef write_register(uint8_t device, uint8_t reg, uint8_t *data, uint8_t len)
{
  return HAL_I2C_Mem_Write(&hi2c1, (device << 1), (uint16_t) reg, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

#define I2C_Write_USB_PD(b,c,d) write_register(STUSB4500_ADDR, b,c,d)
#define I2C_Read_USB_PD(b,c,d) read_register(STUSB4500_ADDR, b,c,d)

HAL_StatusTypeDef stusb_read_rdo(STUSB_GEN1S_RDO_REG_STATUS_RegTypeDef *Nego_RDO) {
  HAL_StatusTypeDef ret;
  ret = read_register(STUSB4500_ADDR, (uint16_t) RDO_REG_STATUS, (uint8_t *) Nego_RDO, 4);

  return ret;
}

HAL_StatusTypeDef stusb_update_pdo(uint8_t pdo_number, uint16_t voltage_mv, uint16_t current_ma) {
  HAL_StatusTypeDef ret;
  uint16_t addr;
  uint8_t data[40];
  uint8_t i, j = 0;

  // get existing
  addr = DPM_SNK_PDO1;
  ret = read_register(STUSB4500_ADDR, addr, data, 12);
  for (i = 0 ; i < 3 ; i++) { pdo_profile[i].d32 = (uint32_t) (data[j] +(data[j+1]<<8)+(data[j+2]<<16)+(data[j+3]<<24));
    j += 4;
  }

  // update
  if ((pdo_number == 1) || (pdo_number == 2) || (pdo_number == 3)) {
    pdo_profile[pdo_number - 1].fix.Operationnal_Current = current_ma / 10;
    if (pdo_number == 1) {
      //force 5V for PDO_1 to follow the USB PD spec
      pdo_profile[pdo_number - 1].fix.Voltage = 100; // 5000/50=100
      pdo_profile[pdo_number - 1].fix.USB_Communications_Capable = 1;
    } else {
      pdo_profile[pdo_number - 1].fix.Voltage = voltage_mv / 50;
    }

    addr = DPM_SNK_PDO1 + (4 * (pdo_number - 1));
    ret = write_register(STUSB4500_ADDR, addr, (uint8_t *) &pdo_profile[pdo_number - 1].d32, 4);
  }

  return ret;
}

HAL_StatusTypeDef stusb_set_valid_pdo(uint8_t valid_count) {
  HAL_StatusTypeDef ret = -1;
  if (valid_count <= 3) {
    ret = write_register(STUSB4500_ADDR, DPM_PDO_NUMB, &valid_count, 1);
  }
  return ret;
}

HAL_StatusTypeDef stusb_set_vbus(uint16_t voltage_mv) {
  HAL_StatusTypeDef ret;

  if( (voltage_mv < 5000) || (voltage_mv > 20000) ) {
    return HAL_ERROR;
  }

  ret += stusb_set_valid_pdo(2);
  ret += stusb_update_pdo(1, 5000, CANAPE_MAX_I);
  ret += stusb_update_pdo(2, voltage_mv, CANAPE_MAX_I);
  ret += stusb_soft_reset();

  return ret;
}

HAL_StatusTypeDef stusb_set_vbus_en(void) {
  // enable VBUS by setting PDO to 1: standard USB
  stusb_update_pdo(1, 5000, CANAPE_MAX_I);
  return stusb_set_valid_pdo(1);
}

HAL_StatusTypeDef stusb_soft_reset(void) {
  uint8_t set = 1;
  return write_register(STUSB4500_ADDR, RESET_CTRL, &set, 1);
}


// NVM functions

// Default
static uint8_t sector[5][8] = {{0x00,0x00,0xB0,0xAA,0x00,0x45,0x00,0x00},
                               {0x10,0x40,0x9C,0x1C,0xFF,0x01,0x3C,0xDF},
                               {0x02,0x40,0x0F,0x00,0x32,0x00,0xFC,0xF1},
                               {0x00,0x19,0x56,0xAF,0xF5,0x35,0x5F,0x00},
                               {0x00,0x4B,0x90,0x21,0x43,0x00,0x40,0xFB}};

static void CUST_EnterWriteMode(unsigned char ErasedSector);
static void CUST_WriteSector(char SectorNum, unsigned char *SectorData);
static void CUST_ExitTestMode();

void stusb_nvm_flash(void) {
  CUST_EnterWriteMode(SECTOR_0 |SECTOR_1  |SECTOR_2 |SECTOR_3  | SECTOR_4 );

  CUST_WriteSector(0,&sector[0][0]);
  CUST_WriteSector(1,&sector[1][0]);
  CUST_WriteSector(2,&sector[2][0]);
  CUST_WriteSector(3,&sector[3][0]);
  CUST_WriteSector(4,&sector[4][0]);

  CUST_ExitTestMode();
}

void stusb_nvm_read(void) {
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

uint8_t stusb_nvm_power_above5v_only(uint8_t value) {
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

void stusb_nvm_set_voltage(uint8_t pdo_numb, float voltage) {
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

void stusb_nvm_set_curent(uint8_t pdo_numb, float current) {
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

void stusb_nvm_set_pdo_number(uint8_t value) {
  if(value > 3) value = 3;

  //load PDO number (sector 3, byte 2, bits 2:3)
  sector[3][2] &= 0xF9;
  sector[3][2] |= (value<<1);
}


uint8_t stusb_nvm_comms_capable(uint8_t value) {
  if(value != 0) value = 1;

  //load USB_COMM_CAPABLE (sector 3, byte 2, bit 0)
  if (((sector[3][2] >> 0) & 1) != value) {
    sector[3][2] &= 0xFE; //clear bit 0
    sector[3][2] |= (value);
    return 1;
  } else {
    return 0;
  }
}

void stusb_nvm_config_powerok(uint8_t value) {
  if(value < 2) value = 0;
  else if(value > 3) value = 3;

  //load POWER_OK_CFG (sector 4, byte 4, bits 5:6)
  sector[4][4] &= 0x9F; //clear bit 3
  sector[4][4] |= value<<5;
}

static void CUST_EnterWriteMode(unsigned char ErasedSector) {
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

static void CUST_WriteSector(char SectorNum, unsigned char *SectorData) {
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

static void CUST_ExitTestMode() {
  unsigned char Buffer[2];

  Buffer[0]= FTP_CUST_RST_N; Buffer[1]=0x00;  /* clear registers */
  I2C_Write_USB_PD(FTP_CTRL_0,Buffer,2);
  Buffer[0]=0x00;
  I2C_Write_USB_PD(FTP_CUST_PASSWORD_REG,Buffer,1);  /* Clear Password */

}
