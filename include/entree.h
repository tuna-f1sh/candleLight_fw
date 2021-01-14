#ifndef __ENTREE_H__
#define __ENTREE_H__

#include <stdbool.h>

#define ENTREE_CONFIG_ID             0x010UL
#define ENTREE_KEY                   0xAF

#define ENTREE_FMSG_VBUS             0x01
#define ENTREE_FMSG_SPDO             0x02
#define ENTREE_FMSG_PDON             0x03
#define ENTREE_FMSG_SVLT             0x04
#define ENTREE_FMSG_GRDO             0x05
#define ENTREE_FMSG_SAVE_CAN         0x06

#define ENTREE_MAX_I                 1000 // max current (mA) hardware supports
#define ENTREE_IDS_ALWAYS            false

struct entree_config_t {
  uint8_t msg;
  uint8_t payload[6];
  uint8_t key;
} __packed;

void process_entree_config(struct entree_config_t *pconfig);
void entree_init(void);

#endif
