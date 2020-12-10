#include "led.h"
#include "canape.h"
#include "usbpd_nvm.h"

extern led_data_t hLED;

void process_canape_config(struct canape_config_t *pconfig) {
  led_seq_step_t set_seq[] = {
      { .state = 0x03, .time_in_10ms = 10 },
      { .state = 0x00, .time_in_10ms = 10 }
  };

  switch (pconfig->msg) {
    case CANAPE_FMSG_VBUS:
      // TODO do this without NVM?
      // get current
      nvm_read();
      // setting is true to assert VBUS EN so invert and will return true is needs flash
      if (setPowerAbove5vOnly(!(pconfig->setting & 0x01))) {
        nvm_flash();
        soft_reset();
        led_run_sequence(&hLED, set_seq, 20);
      } else {
        led_run_sequence(&hLED, set_seq, 0);
      }
      break;
    case CANAPE_FMSG_SPDO:
      // TODO set pdo profile config
      break;
    case CANAPE_FMSG_PDON:
      // TODO set number of profiles in use
      break;
    case CANAPE_FMSG_SVI:
      // TODO set voltage and current
      break;
    case CANAPE_FMSG_GVI:
      // TODO get voltage and current
      break;
    default:
      break;
  }
}

