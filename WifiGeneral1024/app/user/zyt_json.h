#ifndef APP_INCLUDE_ZYT_JSON_H_
#define APP_INCLUDE_ZYT_JSON_H_

#include "stdbool.h"
struct zyt_json_store
{
	char *json;  
	int json_len;
	int mem_len; 
	bool valid;  
	bool lock;   
};

bool ICACHE_FLASH_ATTR is_string_number(char *str, int n);
char * ICACHE_FLASH_ATTR purify_json_str(char *str);
bool ICACHE_FLASH_ATTR is_json_valid(char *json_str);
int ICACHE_FLASH_ATTR get_json_value(void *jsonstr, char *key, int idx, char dest[]);
void * ICACHE_FLASH_ATTR  make_json_to_send(char *datastr);
int ICACHE_FLASH_ATTR convert_json2pkt_field(void *jsonstr, char **key_excluded, int key_excluded_cnt, char dest[]);
int ICACHE_FLASH_ATTR convert_pkt2json_field(char *src, int src_len, char dest[]);

#endif /* APP_INCLUDE_ZYT_JSON_H_ */
