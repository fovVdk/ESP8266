#ifndef UPDATE_H_
#define UPDATE_H_

#include "stdbool.h"
#include "user_interface.h"
#include "c_types.h"
#include "osapi.h"
#include "mem.h"
#include "espconn.h"
#include "ip_addr.h"
#include "zyt_json.h"
#include "wifi.h"
#include "lp_uart.h"
#include "upgrade.h"


void ICACHE_FLASH_ATTR fw_self_update(uint8 id);
int ICACHE_FLASH_ATTR get_upgrade_host_and_path(char *httpstr, char host[], char path[]);

#endif
