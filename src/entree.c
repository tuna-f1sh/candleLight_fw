#include <string.h>
#include "config.h"
#include "led.h"
#include "entree.h"
#include "stusb4500.h"
#include "can.h"
#include "flash.h"
#include "i2c.h"

#if BOARD == BOARD_entree
extern led_data_t hLED;
extern can_data_t hCAN;
static STUSB_GEN1S_RDO_REG_STATUS_RegTypeDef srdo;

static bool entree_send_reply(can_data_t *hcan, uint8_t msg, uint8_t *data, size_t len);
static led_seq_step_t nvm_set_seq[] = {
  { .state = 0x03, .time_in_10ms = 10 },
  { .state = 0x00, .time_in_10ms = 10 }
};

void process_entree_config(struct entree_config_t *pconfig) {
  bool nvm = pconfig->payload[0] == 0x01;
  uint8_t setting = pconfig->payload[1];
  uint16_t voltage;
  uint16_t current;

  switch (pconfig->msg) {
    case ENTREE_FMSG_VBUS:
      if (nvm) {
        // setting is true to assert VBUS EN so invert and will return true is needs flash
        if (stusb_nvm_power_above5v_only(!(setting == 0x01))) {
          stusb_nvm_flash();
          stusb_soft_reset();
          led_run_sequence(&hLED, nvm_set_seq, 20);
        } else {
          led_run_sequence(&hLED, nvm_set_seq, 1);
        }
      } else {
        if (setting == 0x01) {
          stusb_set_vbus_en();
          stusb_soft_reset();
        }
      }
      break;
    case ENTREE_FMSG_SPDO:
      // set pdo profile config
      memcpy(&voltage, &pconfig->payload[2], 2);
      memcpy(&current, &pconfig->payload[4], 2);
      if (nvm) {
        // PDO 1 is not configurable in NVM
        if (setting != 1) {
          stusb_nvm_set_voltage(setting, (float) voltage / 1000);
          stusb_nvm_set_current(setting, (float) current / 1000);
          stusb_nvm_flash();
          stusb_soft_reset();
          led_run_sequence(&hLED, nvm_set_seq, 20);
        }
      } else {
        stusb_update_pdo(setting, voltage, current);
        stusb_soft_reset();
      }
      break;
    case ENTREE_FMSG_PDON:
      // set number of profiles in use
      if (setting <= 3) {
        if (nvm) {
          stusb_nvm_set_pdo_number(setting);
          stusb_nvm_flash();
          stusb_soft_reset();
          led_run_sequence(&hLED, nvm_set_seq, 20);
        } else {
          stusb_set_valid_pdo(setting);
          stusb_soft_reset();
        }
      }
      break;
    case ENTREE_FMSG_SVLT:
      // set voltage request on VBUS now
      memcpy(&voltage, &pconfig->payload[0], 2);
      stusb_set_vbus(voltage);
      break;
    case ENTREE_FMSG_GRDO:
      // update current profile negotiated
      stusb_read_rdo(&srdo);
      // send on bus
      setting = srdo.b.Object_Pos;
      entree_send_reply(&hCAN, ENTREE_FMSG_GRDO, (uint8_t*) &setting, 1);
      break;
    case ENTREE_FMSG_SAVE_CAN:
      // will blink if flash is writen due to setting change
      if (flash_write_can_settings(nvm ? CAN_SETTINGS_SAVED : CAN_SETTINGS_EMPTY)) {
        led_run_sequence(&hLED, nvm_set_seq, 20);
      } else {
        led_run_sequence(&hLED, nvm_set_seq, 1);
      }
      break;
    default:
      break;
  }
}

void entree_init(void) {
  // init i2c
  MX_I2C1_Init();
  // populate local nvm sectors from stusb4500
  stusb_nvm_read();

  // use GPIO ctrl as a NVM flag for whether set to default or not since this is not written by application and factory default is 1
  if (stusb_nvm_config_gpio(0)) { // software GPIO control
    stusb_nvm_flash_defaults();
    stusb_soft_reset();
    led_run_sequence(&hLED, nvm_set_seq, 20);
  }

  // get current power delivery profile
  if (stusb_read_rdo(&srdo) == HAL_OK) {
    if (stusb_nvm_power_above5v_only(!HAL_GPIO_ReadPin(VBUS_ALWAYS_GPIO_Port, VBUS_ALWAYS_Pin))) {
      stusb_nvm_flash();
      stusb_soft_reset();
      led_run_sequence(&hLED, nvm_set_seq, 20);
    }
  }
}

static bool entree_send_reply(can_data_t *hcan, uint8_t msg, uint8_t *data, size_t len) {
  struct gs_host_frame frame;
  bool ret;

  if (len <= 6) {
    frame.can_id = ENTREE_CONFIG_ID;
    frame.can_dlc = len;
    frame.data[0] = msg;
    frame.data[7] = ENTREE_KEY;
    memcpy(&frame.data[1], data, len);

    ret = can_send(hcan, &frame);
  } else {
    ret = false;
  }

  return ret;
}

#endif
