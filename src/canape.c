// TODO refactor nvm functions in stusb4500
// TODO all nvm functions to check for parity with nvm first
#include <string.h>
#include "config.h"
#include "led.h"
#include "canape.h"
#include "usbpd_nvm.h"
#include "stusb4500.h"
#include "can.h"

extern led_data_t hLED;
extern can_data_t hCAN;
static STUSB_GEN1S_RDO_REG_STATUS_RegTypeDef srdo;

static bool canape_send_reply(can_data_t *hcan, uint8_t msg, uint8_t *data, size_t len);

void process_canape_config(struct canape_config_t *pconfig) {
  led_seq_step_t set_seq[] = {
      { .state = 0x03, .time_in_10ms = 10 },
      { .state = 0x00, .time_in_10ms = 10 }
  };
  bool nvm = pconfig->bytes[0] == 0x01;
  uint8_t setting = pconfig->bytes[1];
  uint16_t voltage;
  uint16_t current;

  switch (pconfig->msg) {
    case CANAPE_FMSG_VBUS:
      if (nvm) {
        // get current
        stusb_nvm_read();
        // setting is true to assert VBUS EN so invert and will return true is needs flash
        if (setPowerAbove5vOnly(!(setting == 0x01))) {
          stusb_nvm_flash();
          stusb_soft_reset();
          led_run_sequence(&hLED, set_seq, 20);
        } else {
          led_run_sequence(&hLED, set_seq, 0);
        }
      } else {
        if (setting == 0x01) setting = stusb_vbus_enable();
      }
      canape_send_reply(&hCAN, CANAPE_FMSG_VBUS, (uint8_t*) &setting, 1);
      break;
    case CANAPE_FMSG_SPDO:
      // set pdo profile config
      memcpy(&voltage, &pconfig->bytes[1], 2);
      memcpy(&current, &pconfig->bytes[3], 2);
      if (nvm) {
        setVoltage(setting, (float) voltage / 1000);
        setCurrent(setting, (float) current / 1000);
        stusb_nvm_flash();
        stusb_soft_reset();
        led_run_sequence(&hLED, set_seq, 20);
      } else {
        setting = stusb_update_pdo(setting, voltage, current);
        /* stusb_soft_reset(); */
      }
      canape_send_reply(&hCAN, CANAPE_FMSG_SPDO, (uint8_t*) &setting, 1);
      break;
    case CANAPE_FMSG_PDON:
      // set number of profiles in use
      if (setting <= 3) {
        if (nvm) {
          setPdoNumber(setting);
          stusb_nvm_flash();
          stusb_soft_reset();
          led_run_sequence(&hLED, set_seq, 20);
        } else {
          setting = stusb_set_valid_pdo(setting);
          /* stusb_soft_reset(); */
        }
      }
      canape_send_reply(&hCAN, CANAPE_FMSG_PDON, (uint8_t*) &setting, 1);
      break;
    case CANAPE_FMSG_SVLT:
      // set voltage on request on VBUS now
      memcpy(&voltage, &pconfig->bytes[1], 2);
      setting = stusb_set_vbus(voltage);
      canape_send_reply(&hCAN, CANAPE_FMSG_SVLT, (uint8_t*) &setting, 1);
      break;
    case CANAPE_FMSG_GRDO:
      // update current profile negotiated
      stusb_read_rdo(&srdo);
      // send on bus
      setting = srdo.b.Object_Pos;
      canape_send_reply(&hCAN, CANAPE_FMSG_GRDO, (uint8_t*) &setting, 1);
      break;
    default:
      break;
  }
}

void canape_init(void) {
  // get current power delivery profile
  if (stusb_read_rdo(&srdo) == HAL_OK) {
    // check VBUS always pin and enable VBUS if asserted and a profile is not enabled
    if (HAL_GPIO_ReadPin(VBUS_ALWAYS_GPIO_Port, VBUS_ALWAYS_Pin) && srdo.b.Object_Pos == 0) {
      stusb_vbus_enable();
    }
  }
}

static bool canape_send_reply(can_data_t *hcan, uint8_t msg, uint8_t *data, size_t len) {
  struct gs_host_frame frame;
  bool ret;

  if (len <= 6) {
    frame.can_id = CANAPE_CONFIG_ID;
    frame.can_dlc = len;
    frame.data[0] = msg;
    frame.data[7] = CANAPE_KEY;
    memcpy(&frame.data[1], data, len);

    ret = can_send(hcan, &frame);
  } else {
    ret = false;
  }

  return ret;
}
