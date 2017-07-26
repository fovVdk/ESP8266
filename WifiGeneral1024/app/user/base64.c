#include "osapi.h"
#include "c_types.h"
#include "base64.h"

bool base64Flag = false;

static const unsigned char pr2six[256] = {
/* ASCII table */
64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
                64, 64, 64, 64, 64, 64, 64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
                45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

/**
 * @param  bufbase64
 * @return strlen()
 */
int ICACHE_FLASH_ATTR
plain_len_of_base64(const char *bufbase64)
{
#if 0
        int nbytesdecoded;
        unsigned char *bufin;
        int nprbytes;

        bufin = ( unsigned char *) bufbase64;
        while (pr2six[*(bufin++)] <= 63);

        nprbytes = (bufin - ( unsigned char *) bufbase64) - 1;
        nbytesdecoded = ((nprbytes + 3) / 4) * 3;

        return nbytesdecoded + 1;
#else
        int len = os_strlen(bufbase64);
        int equ = 0;
        int i;
        for (i = 0; i < 2; ++i)
        {
                if (bufbase64[len - 1 - i] == '=')
                        ++equ;
                else
                        break;
        }
        return (len + 3) / 4 * 3 - equ;
#endif
}

/**
 * @param  bufplain
 * @param  bufbase64
 * @return
 */
int ICACHE_FLASH_ATTR
decode_base64_to_plain(char bufplain[], const char *bufbase64)
{
        int nbytesdecoded;
        unsigned char *bufin;
        unsigned char *bufout;
        int nprbytes;

        bufin = (unsigned char *) bufbase64;
        while (pr2six[*(bufin++)] <= 63)
                ;
        nprbytes = (bufin - (unsigned char *) bufbase64) - 1;
        nbytesdecoded = ((nprbytes + 3) / 4) * 3;

        bufout = (unsigned char *) bufplain;
        bufin = (unsigned char *) bufbase64;

        while (nprbytes > 4)
        {
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
                bufin += 4;
                nprbytes -= 4;
        }

        /* Note: (nprbytes == 1) would be an error, so just ingore that case */
        if (nprbytes > 1)
        {
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        }
        if (nprbytes > 2)
        {
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        }
        if (nprbytes > 3)
        {
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        }

        // *(bufout++) = '\0';
        *bufout = '\0';
        nbytesdecoded -= (4 - nprbytes) & 3;
        return nbytesdecoded;
}

static const char basis64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @param
 * @return strlen()
 */
int ICACHE_FLASH_ATTR
base64_len_of_plain(int plain_len)
{
        // return ((plain_str_len + 2) / 3 * 4) + 1;
        return (plain_len + 2) / 3 * 4;
}

/**
 * @param  bufbase64
 * @param  bufplain
 * @param
 * @return
 */
int ICACHE_FLASH_ATTR
encode_plain_to_base64(char bufbase64[], const char *bufplain, int plain_len)
{
        int i;
        char *p;

        p = bufbase64;
        for (i = 0; i < plain_len - 2; i += 3)
        {
                *p++ = basis64[(bufplain[i] >> 2) & 0x3F];
                *p++ = basis64[((bufplain[i] & 0x3) << 4) | ((int) (bufplain[i + 1] & 0xF0) >> 4)];
                *p++ = basis64[((bufplain[i + 1] & 0xF) << 2) | ((int) (bufplain[i + 2] & 0xC0) >> 6)];
                *p++ = basis64[bufplain[i + 2] & 0x3F];
        }
        if (i < plain_len)
        {
                *p++ = basis64[(bufplain[i] >> 2) & 0x3F];
                if (i == (plain_len - 1))
                {
                        *p++ = basis64[((bufplain[i] & 0x3) << 4)];
                        *p++ = '=';
                }
                else
                {
                        *p++ = basis64[((bufplain[i] & 0x3) << 4) | ((int) (bufplain[i + 1] & 0xF0) >> 4)];
                        *p++ = basis64[((bufplain[i + 1] & 0xF) << 2)];
                }
                *p++ = '=';
        }

        // *p++ = '\0';
        *p = '\0';
        return (p - bufbase64);
}
