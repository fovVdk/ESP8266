#include "stdbool.h"

#include "c_types.h"
#include "osapi.h"
#include "mem.h"

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#include "zyt_json.h"
#include "lp_uart.h"

bool ICACHE_FLASH_ATTR
is_string_number(char *str, int n)    //判断是否为数字串
{
	int i;
	for (i = 0; (n > 0) ? (str[i] && (i < n)) : str[i]; ++i)
	{
		if (str[i] < '0' || '9' < str[i])
		{
			return false;
		}
	}
	return true;
}


/*
 * @return NULL:error / 闈濶ULL:鏂扮殑鍫嗗湴鍧�
 */
char * ICACHE_FLASH_ATTR
purify_json_str(char *str)      //净化json数据串
{
	int i, j;
	int len = os_strlen(str) + 1;
	if (len <= 1)
		return NULL;

	char *s = (char *) os_malloc(len);
	if (s == NULL)
		return NULL;

	if (os_strchr(str, ' ') || os_strchr(str, '\t') || os_strchr(str, '\r'))
	{
		for (i = 0, j = 0; i < len; ++i)
		{
			if ((str[i] == ' ') || (str[i] == '\t') || (str[i] == '\r'))
			{
				continue;
			}
			s[j++] = str[i];
		}
		// s[j] = '\0';
	}
	else
	{
		os_memcpy(s, str, len);
	}

	return s;
}

/*
 * 瀛楃涓叉槸鍚︾鍚坖son鎴戠殑鏍煎紡
 */
bool ICACHE_FLASH_ATTR
is_json_valid(char *json_str)    //判断json数据是否有效
{
	bool object_start = false;
	bool array_start = false;
	bool string_start = false;

	bool value_expecting = false;
	bool err = false;

	int i;
	int len = os_strlen(json_str);
	if (json_str[0] != '{' || json_str[len - 1] != '}')
	{
		return false;
	}

	for (i = 0; i < len; ++i)
	{
		switch (json_str[i])
		{
		case '{':
			if (object_start)
				err = true;
			else
				object_start = true;
			break;

		case '}':
			if (!object_start)
			{
				err = true;
			}
			else
			{
				object_start = false;
				value_expecting = false;
			}
			break;

		case '[':
			if (object_start && (!string_start))
			{
				if (array_start)
					err = true;
				else
					array_start = true;
			}
			else
				err = true;
			break;
		case ']':
			if (object_start && (!string_start))
			{
				if (array_start)
					array_start = false;
				else
					err = true;
			}
			else
				err = true;
			break;

		case '\"':
			if (object_start)
			{
				string_start = !string_start;
			}
			else
				err = true;
			break;

		case ':':
			if (object_start)
			{
				if (!array_start)
				{
					if (string_start)
					{
						err = true;
					}
					else
					{
						if (value_expecting)
							err = true;
						else
							value_expecting = true;
					}
				}
			}
			else
				err = true;
			break;

		case ',':
			if (object_start)
			{
				if (string_start)
				{
					err = true;
				}
				else
				{
					if (!array_start)
					{
						if (value_expecting)
							value_expecting = false;
						else
							err = true;
					}
				}
			}
			else
				err = true;
			break;

		default:
			break;
		}

		if (err)
			break;
	}

	return !(err || object_start || array_start || string_start || value_expecting);
}

/*
 * @return -1:error
 */
int ICACHE_FLASH_ATTR
get_json_value(void *jsonstr, char *key, int idx, char dest[])      //获取json数据
{
	int ret = -1;
	char *str = (char *) jsonstr;
	char *key_begin = (char *)os_strstr(str, key);
	int key_len = os_strlen(key);

	if (key_begin == NULL)
	{
		return -1;
	}

	if (key_begin[key_len] == ':')
	{
		char *value_begin = NULL;
		char *value_end = NULL;
		if (idx > 0 || key_begin[key_len + 1] == '[')
		{
			char *tmp = NULL;
			char *array_bracket_right = NULL;
			int i;

			value_begin = key_begin + key_len + 2;
			if (NULL == (array_bracket_right = os_strchr(value_begin, ']')))
				return -1;

			for (i = 0, tmp = value_begin - 1; i <= idx; ++i)
			{
				value_begin = tmp + 1;

				tmp = os_strchr(value_begin, ',');
				if (NULL == tmp || tmp > array_bracket_right)
					tmp = os_strchr(value_begin, ']');

				if (tmp && (tmp <= array_bracket_right))
					value_end = tmp;
				else
					return -1;
			}
		}
		else
		{
			value_begin = key_begin + key_len + 1;
			if (NULL == (value_end = os_strchr(value_begin, ',')))
				value_end = os_strchr(value_begin, '}');
			if (NULL == value_end)
				return -1;
		}

		int cnt = value_end - value_begin;
		os_memcpy(dest, value_begin, cnt);
		dest[cnt] = '\0';
		ret = cnt + 1;
	}
	else
		return -1;

	return ret;
}

static struct zyt_json_store _zyt_json_send = {
		.json = NULL,
		.json_len = 0, .mem_len = 0,
		.valid = false, .lock = false
	};
void * ICACHE_FLASH_ATTR
make_json_to_send(char *datastr)   //转成json格式以备发送
{
	if (datastr == NULL)
	{
		if (_zyt_json_send.json != NULL)
			os_free(_zyt_json_send.json);
		_zyt_json_send.json = NULL;
		_zyt_json_send.json_len = 0;
		_zyt_json_send.mem_len = 0;
		return NULL;
	}

	if (_zyt_json_send.json == NULL)
	{
		_zyt_json_send.json = (char *)os_malloc(128);
		if (_zyt_json_send.json != NULL)
		{
			_zyt_json_send.json_len = 1;
			_zyt_json_send.mem_len = 128;
			// memset(_zyt_json_send.json, '\0', 128);
			_zyt_json_send.json[0] = '\0';
		}
		else
			return NULL;
	}
	int data_len = os_strlen(datastr);
	while (_zyt_json_send.json_len + data_len > _zyt_json_send.mem_len)
	{
		_zyt_json_send.json = (char *)os_realloc(_zyt_json_send.json, _zyt_json_send.mem_len + 32);
		// memset(_zyt_json_send.json + _zyt_json_send.mem_len, 32);
		_zyt_json_send.mem_len += 32;
	}
	os_strcat(_zyt_json_send.json, datastr);
	// _zyt_json_send.json_len = strlen(_zyt_json_send.json) + 1;
	_zyt_json_send.json_len += data_len;

	return _zyt_json_send.json;
}

/*
 * @return -1:error
 */
int ICACHE_FLASH_ATTR
convert_json2pkt_field(void *jsonstr, char **key_excluded, int key_excluded_cnt, char dest[]) //json格式转成数据包
{
	int i;
	int ret = 0;
	int dest_idx = 0;
	char *str = (char *) jsonstr;

	char *prev_kv_end = NULL;
	char *key_begin = NULL;
	char *pair_mid = NULL;
	char *value_begin = NULL;
	char *value_end = NULL;

	bool ex_flag = false;
	bool err = false;

	prev_kv_end = str;
	do
	{
		pair_mid = (char *)os_strstr(prev_kv_end, "\":");
		if (pair_mid == NULL)
			return ret;

		ex_flag = false;
		if (*(prev_kv_end + 1) == '\"')
		{
			for (i = 0; i < key_excluded_cnt; ++i)
			{
				// printf("%s\n", key_excluded[i]);
				char * tmp = NULL;
				if (tmp = (char *)os_strstr(prev_kv_end, key_excluded[i]))
				{
					if ((tmp < pair_mid) && (*(tmp - 1) == '\"')
							&& (tmp + os_strlen(key_excluded[i]) == pair_mid))
					{
						// if (tmp + strlen(key_excluded[i]) == pair_mid)
						// {
						ex_flag = true;
						break;
						// }
					}
				}
			}
			if (ex_flag)
			{
				if (pair_mid[2] == '[')
				{
					value_begin = pair_mid + 3;
					value_end = os_strchr(value_begin, ']');
					if (value_end != NULL)
					{
						if (*(value_end + 1) == ',')
							prev_kv_end = value_end + 1;
						else
							prev_kv_end = value_end;
					}
					else
						err = true;
				}
				else
				{
					value_begin = pair_mid + 2;
					if (NULL == (value_end = os_strchr(value_begin, ',')))
						value_end = os_strchr(value_begin, '}');
					if (value_end != NULL)
						prev_kv_end = value_end;
					else
						err = true;
				}

				if (err)
					return -1;
			}
			else
			{
				if (pair_mid[2] == '[')
				{
					value_begin = pair_mid + 3;
					value_end = os_strchr(value_begin, ']');
					if (value_end != NULL)
					{
						if (*(value_end + 1) == ',')
							prev_kv_end = value_end + 1;
						else
							prev_kv_end = value_end;

						if (!!dest_idx)
						{
							dest[dest_idx++] = PAYLOAD_GAP;  // PAYLOAD_GAP
							++ret;
						}

						int len = value_end - value_begin;
						for (i = 0; i < len; ++i)
						{
							if (value_begin[i] == '\"')
								continue;
							// if (value_begin[i] == ':' && value_begin[i - 1] == ':')
							//     continue;

							if (value_begin[i] == ',') // && value_begin[i - 1] == '\"'
								dest[dest_idx++] = PAYLOAD_GAP;
							else
								dest[dest_idx++] = value_begin[i];
							++ret;
						}
					}
					else
						err = true;
				}
				else
				{
					key_begin = prev_kv_end + 1;
					if (*key_begin != '\"')
						err = true;
					if (NULL == (value_end = os_strchr(pair_mid, ',')))
						value_end = os_strchr(pair_mid, '}');
					if (value_end != NULL)
						prev_kv_end = value_end;
					else
						err = true;
					value_begin = pair_mid + 2;

					if (!!dest_idx)
					{
						dest[dest_idx++] = 0x00;  // PAYLOAD_GAP
						++ret;
					}

					int len = value_end - key_begin;
					for (i = 0; i < len; ++i)
					{
						if (key_begin[i] == '\"')
							continue;
						dest[dest_idx++] = key_begin[i];
						++ret;
					}
#if 0
					if (value_begin[0] == '\"')
					{
						int i;
						int len = value_end - key_begin;
						for (i = 0; i < len; ++i)
						{
							if (key_begin[i] == '\"')
							continue;
							dest[dest_idx++] = key_begin[i];
							++ret;
						}
					}
					else
					{
						int i;
						int len = value_begin - key_begin;
						for (i = 0; i < len; ++i)
						{
							if (key_begin[i] == '\"')
							continue;
							dest[dest_idx++] = key_begin[i];
							++ret;
						}
						char num[10] =
						{	0};
						memcpy(num, value_begin, value_end - value_begin);
						unsigned int n = (unsigned int) atoi(num);
						if (n < 256)
						{
							*((unsigned char*) (dest + dest_idx)) = (unsigned char) n;
							++dest_idx;
							++ret;
						}
						else if (n < 65536)
						{
							*((unsigned short*) (dest + dest_idx)) = (unsigned short) n;
							dest_idx += 2;
							ret += 2;
						}
						else if (n < 4294967296)
						{
							*((unsigned int*) (dest + dest_idx)) = (unsigned int) n;
							dest_idx += 4;
							ret += 4;
						}
						// else
						// {
						// 	*((long long*)(dest+dest_idx)) = (long long)n;
						// 	dest_idx += 8;
						// 	ret += 8;
						// }
					}
#endif
				}

				if (err)
					return -1;
			}
		}
		else
			return -1;
	} while (1);
	return ret;
}

/*
 * @return -1:error
 */
int ICACHE_FLASH_ATTR
convert_pkt2json_field(char *src, int src_len, char dest[])  //数据包转成json
{
	int i;
	int ret = 0;
	int dest_idx = 0;

	for (i = 0; i < src_len; ++i)
	{
		if (i == 0)
		{
			dest[dest_idx++] = '\"';
			dest[dest_idx++] = src[i];
			ret += 2;
			continue;
		}
		// if (src[i] == ':')
		// {
		// 	dest[dest_idx++] = ':';
		// 	dest[dest_idx++] = ':';
		// 	ret += 2;
		// 	continue;
		// }
		if (src[i] == 0x00)
		{
			dest[dest_idx++] = '\"';
			++ret;
			continue;
		}

		if (src[i] != 0x00 && src[i - 1] == 0x00)
		{
			dest[dest_idx++] = ',';
			dest[dest_idx++] = '\"';
			dest[dest_idx++] = src[i];
			ret += 3;
			continue;
		}

		dest[dest_idx++] = src[i];
		++ret;
	}
	if (!!dest_idx)
	{
		dest[dest_idx++] = '\"';
		dest[dest_idx] = '\0';
		ret += 2;
	}

	return ret;
}

