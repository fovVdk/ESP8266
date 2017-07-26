#include "update.h"


#define SKY_VERSION_main 0
#define SKY_VERSION_sub  2


#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,zh,en-US;q=0.8\r\n\r\n"

static struct espconn *pespconn = NULL;
static struct upgrade_server_info *upServer = NULL;
static ip_addr_t host_ip;
static os_timer_t tm_upgrade_delayCheck;


extern char upgrade_file_host[32];
extern char upgrade_file_path[64];
extern unsigned short tcpip_sn[2];

static bool upgrade_dup_flag = false;

static void ICACHE_FLASH_ATTR
at_upDate_discon_cb(void *arg)
{
        struct espconn *pespconn = (struct espconn *) arg;

        if (pespconn->proto.tcp != NULL)
        {
                os_free(pespconn->proto.tcp);
        }
        if (pespconn != NULL)
        {
                os_free(pespconn);
        }

        if (system_upgrade_start(upServer))
        {
            //更新成功
              uart0_txstr_buffer("update succeed...\r\n");
        }
}

static void ICACHE_FLASH_ATTR
at_upDate_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;
    upgrade_dup_flag = false;
    if (server->upgrade_flag)
    {
            char *js_send = NULL;
            make_json_to_send(NULL);
            js_send = make_json_to_send("{\"sn\":");
            char sn_str[6] = {0};
            os_sprintf(sn_str, "%d", tcpip_sn[0]++);
            js_send = make_json_to_send(sn_str);
            js_send = make_json_to_send(",\"cmd\":\"updatewifi\",\"ret\":201}");
            SendDataToServer(js_send);
            make_json_to_send(NULL);
            system_upgrade_reboot();
    }
    else
    {
            char *js_send = NULL;
            make_json_to_send(NULL);
            js_send = make_json_to_send("{\"sn\":");
            char sn_str[6] = {0};
            os_sprintf(sn_str, "%d", tcpip_sn[0]++);
            js_send = make_json_to_send(sn_str);
            js_send = make_json_to_send(",\"cmd\":\"updatewifi\",\"ret\":401}");
            SendDataToServer(js_send);
            make_json_to_send(NULL);
    }
    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;


}

static void ICACHE_FLASH_ATTR
at_upDate_recv(void *arg, char *pusrdata, unsigned short len)
{
        struct espconn *pespconn = (struct espconn *) arg;

        os_timer_disarm(&tm_upgrade_delayCheck);
        if (upgrade_dup_flag)
        {
                return;
        }
        upgrade_dup_flag = true;
        uart0_txstr_buffer("update 3..........\r\n");
        upServer = (struct upgrade_server_info *) os_zalloc(sizeof(struct upgrade_server_info));

        os_memcpy(upServer->ip, pespconn->proto.tcp->remote_ip, 4);
        upServer->port = pespconn->proto.tcp->remote_port;

        os_sprintf(upServer->pre_version, "v%d.%d", SKY_VERSION_main, SKY_VERSION_sub);
        os_sprintf(upServer->upgrade_version, "v%d.%d", SKY_VERSION_main, SKY_VERSION_sub + 1);
        upServer->upgrade_version[5] = '\0';
        upServer->pespconn = pespconn;
        upServer->check_cb = at_upDate_rsp;
        upServer->check_times = 60000;
        if (upServer->url == NULL)
        {
                upServer->url = (uint8 *) os_zalloc(512);
        }
        os_sprintf(upServer->url, "GET %s HTTP/1.1\r\nHost: %s\r\n"pheadbuffer, upgrade_file_path, upgrade_file_host);

}

static void ICACHE_FLASH_ATTR
at_upDate_wait(void *arg)
{
    struct espconn *pespconn = arg;
    os_timer_disarm(&tm_upgrade_delayCheck);
    if (pespconn != NULL)
    {
            espconn_disconnect(pespconn);
    }
    else
    {

    }
}

static void ICACHE_FLASH_ATTR
at_upDate_sent_cb(void *arg)
{
        struct espconn *pespconn = arg;
        os_timer_disarm(&tm_upgrade_delayCheck);
        os_timer_setfn(&tm_upgrade_delayCheck, (os_timer_func_t *) at_upDate_wait, pespconn);
        os_timer_arm(&tm_upgrade_delayCheck, 5000, 0);
}

static void ICACHE_FLASH_ATTR
at_upDate_connect_cb(void *arg)
{
        struct espconn *pespconn = (struct espconn *) arg;
        uint8 user_bin[9] = {0};
        char *temp;
        uart0_txstr_buffer("update 2..........\r\n");
        espconn_regist_disconcb(pespconn, at_upDate_discon_cb);
        espconn_regist_recvcb(pespconn, at_upDate_recv); ////////
        espconn_regist_sentcb(pespconn, at_upDate_sent_cb);

        temp = (uint8 *) os_zalloc(512);
        os_sprintf(temp, "GET %s HTTP/1.1\r\nHost: %s\r\n"pheadbuffer, upgrade_file_path, upgrade_file_host);

        espconn_send(pespconn, temp, os_strlen(temp));
        os_free(temp);
}

/**
 * @brief  Tcp client connect repeat callback function.
 * @param  arg: contain the ip link information
 * @retval None
 */
static void ICACHE_FLASH_ATTR
at_upDate_recon_cb(void *arg, sint8 errType)
{
    struct espconn *pespconn = (struct espconn *) arg;
    if (pespconn->proto.tcp != NULL)
    {
        os_free(pespconn->proto.tcp);
    }
    os_free(pespconn);
    if (upServer != NULL)
    {
        os_free(upServer);
        upServer = NULL;
    }
}




static void ICACHE_FLASH_ATTR
upServer_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{

    struct espconn *pespconn = (struct espconn *) arg;
    if (ipaddr == NULL)
    {
        return;
    }
    uart0_txstr_buffer("update 1..........\r\n");
    if (host_ip.addr == 0 && ipaddr->addr != 0)
    {
        if (pespconn->type == ESPCONN_TCP)
        {
            os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);
            espconn_regist_connectcb(pespconn, at_upDate_connect_cb);
            espconn_regist_reconcb(pespconn, at_upDate_recon_cb);
            espconn_connect(pespconn);
        }
    }
}


void ICACHE_FLASH_ATTR
fw_self_update(uint8 id)
{
        pespconn = (struct espconn *) os_zalloc(sizeof(struct espconn));
        pespconn->type = ESPCONN_TCP;
        pespconn->state = ESPCONN_NONE;
        pespconn->proto.tcp = (esp_tcp *) os_zalloc(sizeof(esp_tcp));
        pespconn->proto.tcp->local_port = espconn_port();
        pespconn->proto.tcp->remote_port = 80;
        espconn_gethostbyname(pespconn, upgrade_file_host, &host_ip, upServer_dns_found);
        uart0_txstr_buffer("update start..........\r\n");
}

int ICACHE_FLASH_ATTR
get_upgrade_host_and_path(char *httpstr, char host[], char path[])
{
        char *start = NULL;
        char *end = NULL;
        char *httpstr_end = NULL;

        if (httpstr == NULL)
                return -1;
        httpstr_end = httpstr + os_strlen(httpstr);

        start = httpstr;
        if (*start == '\"')
                start += 1;
        if (os_strstr(start, "http://"))
                start += 7;

        if (end = os_strchr(start, '/'))
        {
                if (start >= end)
                        return -1;
        }
        else
        {
                return -1;
        }
        os_memcpy(host, start, end - start);

        start = end;
        end = httpstr_end;
        if (start >= end)
                return -1;
        os_memcpy(path, start, end - start);
        int len = os_strlen(path);
        if (len <= 0)
                return -1;
        else if (path[len - 1] == '\"')
                path[len - 1] = '\0';
//        uart0_txstr_buffer(host);
//        uart0_txstr_buffer(path);
        return 0;
}
