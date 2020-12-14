#ifndef __STUSB4500_H
#define __STUSB4500_H

#include "stm32f0xx_hal.h"

#define STUSB4500_ADDR         0x28
#define RDO_REG_STATUS         0x91

typedef union {
  uint32_t d32;
  struct {
    uint32_t MaxCurrent                     :       10; //Bits 9..0
    uint32_t OperatingCurrent               :       10;
    uint8_t reserved_22_20                  :       3;
    uint8_t UnchunkedMess_sup               :       1;
    uint8_t UsbSuspend                      :       1;
    uint8_t UsbComCap                       :       1;
    uint8_t CapaMismatch                    :       1;
    uint8_t GiveBack                        :       1;
    uint8_t Object_Pos                      :       3; //Bits 30..28 (3-bit)
    uint8_t reserved_31                     :       1; //Bits 31
  } b;
} STUSB_GEN1S_RDO_REG_STATUS_RegTypeDef;

#define RESET_CTRL             0x23

#define VBUS_CTRL              0x27 // this is read only
#define DPM_PDO_NUMB           0x70
#define DPM_SNK_PDO1           0x85

typedef union {
  uint32_t d32;
  struct {
    uint32_t Operationnal_Current :10;
    uint32_t Voltage :10;
    uint8_t Reserved_22_20  :3;
    uint8_t Fast_Role_Req_cur : 2;  /* must be set to 0 in 2.0*/
    uint8_t Dual_Role_Data    :1;
    uint8_t USB_Communications_Capable :1;
    uint8_t Unconstrained_Power :1;
    uint8_t Higher_Capability :1;
    uint8_t Dual_Role_Power :1;
    uint8_t Fixed_Supply :2;
  } fix;

  struct {
    uint32_t Operating_Current :10;
    uint32_t Min_Voltage:10;
    uint32_t Max_Voltage:10;
    uint8_t VariableSupply:2;
  } var;

  struct {
    uint32_t Operating_Power :10;
    uint32_t Min_Voltage:10;
    uint32_t Max_Voltage:10;
    uint8_t Battery:2;
  } bat;

} USB_PD_SNK_PDO_TypeDef;

HAL_StatusTypeDef stusb_read_rdo(STUSB_GEN1S_RDO_REG_STATUS_RegTypeDef *Nego_RDO);
HAL_StatusTypeDef stusb_update_pdo(uint8_t pdo_number, uint16_t voltage_mv, uint16_t current_ma);
HAL_StatusTypeDef stusb_set_valid_pdo(uint8_t valid_count);
HAL_StatusTypeDef stusb_set_vbus(uint16_t voltage_mv);
HAL_StatusTypeDef stusb_set_vbus_en(void);
HAL_StatusTypeDef stusb_soft_reset(void);

/*NVM FLasher Registers Definition */

#define FTP_CUST_PASSWORD_REG 0x95
#define FTP_CUST_PASSWORD  0x47
#define FTP_CTRL_0              0x96
#define FTP_CUST_PWR 0x80
#define FTP_CUST_RST_N 0x40
#define FTP_CUST_REQ 0x10
#define FTP_CUST_SECT 0x07
#define FTP_CTRL_1              0x97
#define FTP_CUST_SER 0xF8
#define FTP_CUST_OPCODE 0x07
#define RW_BUFFER 0x53

#define READ 0x00
#define WRITE_PL 0x01
#define WRITE_SER 0x02
#define READ_PL 0x03
#define READ_SER 0x04
#define ERASE_SECTOR 0x05
#define PROG_SECTOR 0x06
#define SOFT_PROG_SECTOR 0x07

#define SECTOR_0  0x01
#define SECTOR_1  0x02
#define SECTOR_2  0x04
#define SECTOR_3  0x08
#define SECTOR_4  0x10
#define SECTOR_5  0x20

void stusb_nvm_flash(void);
void stusb_nvm_flash_defaults(void);
void stusb_nvm_read(void);
void stusb_nvm_set_voltage(uint8_t pdo_numb, float voltage);
void stusb_nvm_set_current(uint8_t pdo_numb, float current);
uint8_t stusb_nvm_power_above5v_only(uint8_t value);
uint8_t stusb_nvm_set_pdo_number(uint8_t value);
uint8_t stusb_nvm_comms_capable(uint8_t value);
uint8_t stusb_nvm_config_powerok(uint8_t value);
uint8_t stusb_nvm_config_gpio(uint8_t value);

#endif
