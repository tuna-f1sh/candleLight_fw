#ifndef __CANAPE_H__
#define __CANAPE_H__

#include <stdbool.h>

#define CANAPE_CONFIG_ID             0x001UL
#define CANAPE_KEY                   0xAF

#define CANAPE_FMSG_VBUS             0x01
#define CANAPE_FMSG_SPDO             0x02
#define CANAPE_FMSG_PDON             0x03
#define CANAPE_FMSG_SVLT             0x04
#define CANAPE_FMSG_GRDO             0x05

#define CANAPE_MAX_I                 1000 // max current (mA) hardware supports

struct canape_config_t {
  uint8_t msg;
  uint8_t bytes[6];
  uint8_t key;
} __packed;

void process_canape_config(struct canape_config_t *pconfig);
void canape_init(void);

#endif
