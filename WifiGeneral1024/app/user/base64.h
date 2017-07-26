#ifndef BASE64_H_
#define BASE64_H_

#include "stdbool.h"

#include "base64.h"

int plain_len_of_base64(const char *bufbase64);
int decode_base64_to_plain(char bufplain[], const char *bufbase64);
int base64_len_of_plain(int plain_len);
int encode_plain_to_base64(char bufbase64[], const char *bufplain, int plain_len);

#endif
