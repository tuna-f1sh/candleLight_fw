#ifndef __CANAPE_H__
#define __CANAPE_H__

#include <stdbool.h>

#define CANAPE_CONFIG_ID             0x001UL
#define CANAPE_FMSG_VBUS             0x01
#define CANAPE_FMSG_SPDO             0x02
#define CANAPE_FMSG_PDON             0x03
#define CANAPE_FMSG_SVI              0x04
#define CANAPE_FMSG_GVI              0x05

struct canape_config_t {
  uint8_t msg;
  uint8_t setting;
  uint8_t reserved;
} __packed;

void process_canape_config(struct canape_config_t *pconfig);

#endif
