#include "wifi.h"

#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"

#ifndef QUEUE_BUFFER_SIZE
#define QUEUE_BUFFER_SIZE		 	2048
#endif


unsigned short GNET_MAINTAIN_NUM =  0;

os_event_t mqtt_procTaskQueue[MQTT_TASK_QUEUE_SIZE];
extern uint8_t mcuBaseInfo[100];
extern TemporaryServer temporaryServer;
extern UartActivate st_uartActivate;
extern MQTT_Client mqttClient;
struct UartDataBuf mcuSerialBuf;
extern unsigned short tcpip_sn[2];
extern bool base64Flag;

char upgrade_file_host[32] = {0};
char upgrade_file_path[64] = {0};
char tcpSendBuf[1024];
CirQueue stQueue;

/*< wifi 连接维护定时器 */
static ETSTimer WiFiLinker;
static ETSTimer SendDataTimer;

WifiCallback wifiCb = NULL;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
    struct ip_info ipConfig;
    static uint8 ledStatus = 0;

    os_timer_disarm(&WiFiLinker);
    wifi_get_ip_info(STATION_IF, &ipConfig);
    uart0_txstr_buffer();
    wifiStatus = wifi_station_get_connect_status();
    if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
    {

        os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
        os_timer_arm(&WiFiLinker, 2000, 0);
    }
    else
    {
        if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
        {
                wifi_station_connect();
        }
        else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
        {
                wifi_station_connect();
        }
        else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
        {
                wifi_station_connect();
        }
        else
        {

        }
        os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
        os_timer_arm(&WiFiLinker, 2000, 0);
    }
    if(wifiStatus != lastWifiStatus)
    {
        lastWifiStatus = wifiStatus;
        if(wifiCb)
        {
            wifiCb(wifiStatus);
        }
    }

    //网络指示灯处理
    {
        if((mqttClient.netState&0x04) == 0x04) //已连服务器状态
        {
            GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0);
        }
        else if((mqttClient.netState&0x80) == 0x80) //配网状态
        {
            if(ledStatus == 0)
            {
                ledStatus = 1;
                GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0);
            }
            else
            {
                ledStatus = 0;
                GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
            }
        }
        else
        {
            GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
        }
    }
}

void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb)
{
        struct station_config stationConf;

        wifi_set_opmode_current(STATION_MODE);
        wifiCb = cb;
//        os_memset(&stationConf, 0, sizeof(struct station_config));
//        os_sprintf(stationConf.ssid, "%s", ssid);
//        os_sprintf(stationConf.password, "%s", pass);
//        wifi_station_set_config_current(&stationConf);


        wifi_station_get_config_default(&stationConf);
        wifi_station_set_config(&stationConf);

        os_timer_disarm(&WiFiLinker);
        os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
        os_timer_arm(&WiFiLinker, 2000, 0);
        wifi_station_connect();

        wifi_station_get_config_default(&stationConf);
        wifi_station_set_config(&stationConf);
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_delete(MQTT_Client *mqttClient)
{
    if (mqttClient->pCon != NULL)
    {
        if (mqttClient->security)
        {
            espconn_secure_disconnect(mqttClient->pCon);
        }
        else
        {
            espconn_disconnect(mqttClient->pCon);
        }

        espconn_delete(mqttClient->pCon);
        if (mqttClient->pCon->proto.tcp)
                os_free(mqttClient->pCon->proto.tcp);
        os_free(mqttClient->pCon);
        mqttClient->pCon = NULL;
    }
}

/**
  * @brief  Delete MQTT client and free all memory
  * @param  mqttClient: The mqtt client
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_client_delete(MQTT_Client *mqttClient)
{
        mqtt_tcpclient_delete(mqttClient);
        if (mqttClient->host != NULL) {
                os_free(mqttClient->host);
                mqttClient->host = NULL;
        }
//        if (mqttClient->user_data != NULL) {
//                os_free(mqttClient->user_data);
//                mqttClient->user_data = NULL;
//        }
//        if(mqttClient->in_buffer != NULL) {
//                os_free(mqttClient->in_buffer);
//                mqttClient->in_buffer = NULL;
//        }
//        if(mqttClient->out_buffer != NULL) {
//                os_free(mqttClient->out_buffer);
//                mqttClient->out_buffer = NULL;
//        }
}

void ICACHE_FLASH_ATTR
MQTT_Task(os_event_t *e)
{
        MQTT_Client* client = (MQTT_Client*)e->par;
        if (e->par == 0)
                return;
        switch (client->connState) {
        case TCP_RECONNECT_REQ:
                break;
        case TCP_RECONNECT:
                mqtt_tcpclient_delete(client);
                MQTT_Connect(client);
                client->connState = TCP_CONNECTING;
                break;
        case MQTT_DELETING:
        case TCP_DISCONNECTING:
        case TCP_RECONNECT_DISCONNECTING:
//                if (client->security)
//                {
//                    espconn_secure_disconnect(client->pCon);
//                    espconn_delete(client->pCon);
//                    MQTT_Connect(client);//lp为了避免重起的时候连两次
//                }
//                else {
//                    espconn_disconnect(client->pCon);
//                    espconn_delete(client->pCon);
//                    MQTT_Connect(client);
//                }
                mqtt_tcpclient_delete(client);
                break;
        case TCP_DISCONNECTED:
                mqtt_tcpclient_delete(client);
                MQTT_Connect(client);
                break;
        case MQTT_DELETED:
                mqtt_client_delete(client);
                break;
        default:break;
        }
}

void ICACHE_FLASH_ATTR
MQTT_InitConnection(MQTT_Client *mqttClient, uint8_t* host, uint32_t port, uint8_t security)
{
        uint32_t temp;

        os_memset(mqttClient, 0, sizeof(MQTT_Client));
        temp = os_strlen(host);
        mqttClient->host = (uint8_t*)os_zalloc(temp + 1);
        os_strcpy(mqttClient->host, host);
        mqttClient->host[temp] = 0;
        mqttClient->port = port;
        mqttClient->security = security;

        system_os_task(MQTT_Task, MQTT_TASK_PRIO, mqtt_procTaskQueue, MQTT_TASK_QUEUE_SIZE);
        InitQueue(&stQueue);

        os_timer_disarm(&SendDataTimer);
        os_timer_setfn(&SendDataTimer, (os_timer_func_t *)SendDataThread,NULL);
        os_timer_arm(&SendDataTimer,250,1);
}

void ICACHE_FLASH_ATTR UploadStatusServer(char *pData)
{
    if(mqttClient.allowSendFlg == 1)
    {
        mqttClient.allowSendFlg = 0;
        mqttClient.forbidNum    = 0;
        if(mqttClient.forbidCount != -1)
        {
            SendDataToServer(pData);
        }
    }
    if(mqttClient.forbidCount == 0)
    {
        mqttClient.allowSendFlg = 1;
    }
}

void ICACHE_FLASH_ATTR HeartBeat()
{
    if (((mqttClient.netState&0x02) == 0x02)&&(((mqttClient.netState&0x04) == 0x04)))
    {
        char *js_send = NULL;
        char mac[13] = {0};
        char send_sn_str[6] = {0};
        make_json_to_send(NULL);   //构建json数据包
        js_send = make_json_to_send("{\"sn\":");
        if (tcpip_sn[0] >= 65535)
            tcpip_sn[0] = 0;
        os_sprintf(send_sn_str, "%u", (tcpip_sn[0])++);
        js_send = make_json_to_send(send_sn_str);
        js_send = make_json_to_send(",\"cmd\":\"heartbeat\",\"mac\":\"");

        lp_station_get_mac_str(mac);
        js_send = make_json_to_send(mac);
        js_send = make_json_to_send("\"}");
        SendDataToServer(js_send);  //发送json数据
        make_json_to_send(NULL);
    }
}



void ICACHE_FLASH_ATTR mqtt_timer(void *arg)
{
    MQTT_Client* client = (MQTT_Client*)arg;

    client->keepAliveTick++;
    if(client->keepAliveTick > client->heartCount)
    {
        client->keepAliveTick = 0;
        client->heartStopNum++;
        if(client->heartStopNum >= HEART_STOP_COUNT)//网络已经不正常了
        {
            //做不正常处理，重新起动设备
             client->abortResetFlg = 1;
             if (mqttClient.security)
             {
                 espconn_secure_disconnect(mqttClient.pCon);
             }
             else
             {
                 espconn_disconnect(mqttClient.pCon);
             }
             espconn_delete(mqttClient.pCon);
             os_delay_us(10000);
             system_restart();
        }
        HeartBeat();
    }
    if (client->connState == TCP_RECONNECT_REQ)
    {
        client->reconnectTick ++;
        if (client->reconnectTick > MQTT_RECONNECT_TIMEOUT)
        {
            client->reconnectTick = 0;
            client->connState = TCP_RECONNECT;
            system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
        }
    }

    client->forbidNum++;
    if(client->forbidNum > 6000)
        client->forbidNum = 0;
    if(client->forbidNum >= client->forbidCount)
    {
        client->allowSendFlg = 1;
    }
}


LOCAL void ICACHE_FLASH_ATTR
mqtt_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    MQTT_Client* client = (MQTT_Client *)pConn->reverse;

    if (ipaddr == NULL)
    {
        //client->connState = TCP_RECONNECT_REQ;//此处暂不做处理
        return;
    }
    if (client->ip.addr == 0 && ipaddr->addr != 0)
    {
        os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
        if (client->security)
        {
            espconn_secure_connect(client->pCon);

        }
        else
        {
            espconn_connect(client->pCon);
        }
    }
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
    char *myjson = purify_json_str(pdata);  //净化json数据串
    char tmpStr[50] = {0};
    char *ptmpStr = &tmpStr[0];
    struct espconn *pCon = (struct espconn*)arg;
    MQTT_Client *client = (MQTT_Client *)pCon->reverse;

    client->keepAliveTick = 0;
//    uart0_txstr_buffer("recv data is:\r\n");
//    uart0_txstr_buffer(pdata);//lp 从服务器接收到的信息
    if (len < MQTT_BUF_SIZE && len > 0)
    {
        if (get_json_value(myjson, "\"cmd\"", 0, tmpStr) > 0)
        {
                char recv_sn_str[6] = {0};
                if (get_json_value(myjson, "\"sn\"", 0, recv_sn_str) > 0)//获取sn号
                {
                        //tcpip_sn[1] = (uint16) (atoi(recv_sn_str) + 1);//将sn号从字符串转为数字
                }else{}
                if (os_strcmp(tmpStr, "\"login\"") == 0)//登陆返回 server->wifi
                {

                    if (get_json_value(myjson, "\"ret\"", 0, tmpStr) > 0)
                    {
                        if (os_strcmp(tmpStr, "403") == 0)//未授权阻止输入,注意异常处理
                        {

                        }
                        else if((os_strcmp(tmpStr, "200") == 0))
                        {
                            if(client->loginStatus == 1)
                            {
                                client->loginStatus = 2;
                                client->netState = client->netState|0x04;
                                SendNetStateToMcu();
                            }
                        }
                        else if((os_strcmp(tmpStr, "302") == 0))//临时重定向
                        {
                            if (get_json_value(myjson, "\"serverip\"", 0, tmpStr) > 0)
                            {
                                uint8 i = 0;
                                uint8 iport[6];
                                ptmpStr++;
                                while((*ptmpStr !='\0')&&(*ptmpStr != ':'))
                                {
                                    temporaryServer.serverAddr[i++] = *ptmpStr;
                                    ptmpStr++;
                                }
                                temporaryServer.serverAddr[i] = '\0';
                                i = 0;
                                ptmpStr++;
                                while((*ptmpStr != '\0')&&(*ptmpStr != '\"'))
                                {
                                    iport[i++] = *ptmpStr;
                                    ptmpStr++;
                                }
                                iport[i] = '\0';
                                temporaryServer.port = atoi(iport);
                                MQTT_InitConnection(&mqttClient,&(temporaryServer.serverAddr[0]),temporaryServer.port,0);//重新初始化服务器参数
                                MQTT_Connect(&mqttClient);
                            }
                        }
                        else
                        {}
                    }
                    os_memset(tmpStr, 0, sizeof(tmpStr));
                    if (get_json_value(myjson, "\"heart\"", 0, tmpStr) > 0)//获取心跳包时间间隔
                    {
                        if (is_string_number(tmpStr, 6))
                        {
                            uint32 i = atoi(tmpStr);
                            {
                                //设置心跳时间轮询
                                client->heartCount = i;
                            }
                        }
                    }
                }
                else if (os_strcmp(tmpStr, "\"info\"") == 0)//设备状态查询
                {
                    char *js_send = NULL;
                    make_json_to_send(NULL);
                    js_send = make_json_to_send("{\"sn\":");
                    js_send = make_json_to_send(recv_sn_str);
                    js_send = make_json_to_send(",\"cmd\":\"info\",\"ret\":200}");
                    SendDataToServer(js_send);
                    make_json_to_send(NULL);
                    mcuSerialBuf.Buff_Tx[0]=0xAA;
                    mcuSerialBuf.Buff_Tx[1]=0x00;
                    mcuSerialBuf.Buff_Tx[2]=0x01;
                    mcuSerialBuf.Buff_Tx[3]=0x06;
                    mcuSerialBuf.Buff_Tx[4]=0x0A;
                    uart0_send_buffer(&(mcuSerialBuf.Buff_Tx[0]),5);//向设备查询状态
                    st_uartActivate.sendIntervalTimer = 0;
                }
                else if (os_strcmp(tmpStr, "\"heartbeat\"") == 0)//心跳回复
                {
                    client->heartStopNum = 0;//clear心跳计数,已正常接收到心跳回应
                    client->keepAliveTick = 0;
                    //uart0_txstr_buffer(pdata);//lpdebug
                    if (get_json_value(myjson, "\"period\"", 0, tmpStr) > 0)
                    {
                        if (is_string_number(tmpStr, 0))
                        {
                            uint32 i = atoi(tmpStr);
                            {
                                //设置心跳时间
                                client->heartCount = i;
                            }
                        }
                    }
                }
                else if (os_strcmp(tmpStr, "\"download\"") == 0)//控制设备指令
                {
                    char *js_send = NULL;
                    char *ignore_key[] = {"sn", "cmd"};
                    char payload[PAYLOAD_LEN_MAX] = {0};
                    uint16 pl_len,i;
                    make_json_to_send(NULL);
                    js_send = make_json_to_send("{\"sn\":");
                    js_send = make_json_to_send(recv_sn_str);
                    js_send = make_json_to_send(",\"cmd\":\"download\",\"ret\":200}");
                    SendDataToServer(js_send);//和上报时间太短，导致数据丢失
                    make_json_to_send(NULL);
                    if (base64Flag)                 //解base64，向下抛数据
                    {
                        char val[1024] = {0};
                        if (get_json_value(myjson, "\"data\"", 0, val) > 0)
                        {
                            if (val[0] == '"')
                            {
                                    val[os_strlen(val) - 1] = '\0';
                                    pl_len = decode_base64_to_plain(payload, val + 1);//解析到内存
                            }
                            else
                            {
                                    pl_len = decode_base64_to_plain(payload, val);
                            }
                        }
                    }
                    else
                    {
                        pl_len = convert_json2pkt_field(myjson, ignore_key, sizeof(ignore_key) / sizeof((char *) 0), payload);
                    }
                    pl_len = pl_len + 1;//长度包括命令字节
                    mcuSerialBuf.Buff_Tx[0]=0xAA;
                    mcuSerialBuf.Buff_Tx[1]=((pl_len>>8)&0x00FF);
                    mcuSerialBuf.Buff_Tx[2]=(pl_len&0x00FF);
                    mcuSerialBuf.Buff_Tx[3]=0x02;
                    pl_len = pl_len - 1;//填充数据字节,长度应减1
                    for(i=0;i<pl_len;i++)
                    {
                        mcuSerialBuf.Buff_Tx[4+i] = payload[i];
                    }
                    mcuSerialBuf.Buff_Tx[4+i] = 0x0A;
                    uart0_send_buffer(&((mcuSerialBuf.Buff_Tx[0])),pl_len+5);
                    st_uartActivate.sendIntervalTimer = 0;
                }
                else if (os_strcmp(tmpStr, "\"upload\"") == 0)//主动上传状态后的回应
                {

                }
                else if (os_strcmp(tmpStr, "\"silence\"") == 0)//上传限制指令
                {
                    char *js_send = NULL;
                    make_json_to_send(NULL);
                    js_send = make_json_to_send("{\"sn\":");
                    js_send = make_json_to_send(recv_sn_str);
                    js_send = make_json_to_send(",\"cmd\":\"silence\",\"ret\":200}");

                    char val[6] = {0};
                    if (get_json_value(myjson, "\"switch\"", 0, val) > 0)
                    {
                        if (os_strcmp(val, "\"off\"") == 0)
                        {
                            client->forbidCount = -1;
                            client->allowSendFlg = 0;
                        }
                        else
                        {
                            os_memset(val, 0, sizeof(val));

                            if (get_json_value(myjson, "\"period\"", 0, val) > 0)
                            {
                                int i;
                                if (is_string_number(val, 0))
                                {
                                        i = atoi(val);
                                        if(i < 0)
                                            client->forbidCount = 0;
                                        if(i > 6000)
                                            client->forbidCount = 6000;

                                }
                            }
                        }
                    }
                    SendDataToServer(js_send);
                    make_json_to_send(NULL);
                }
                else if (os_strcmp(tmpStr, "\"updatewifi\"") == 0)//固件升级指令
                {
                    char *js_send = NULL;
                    make_json_to_send(NULL);
                    js_send = make_json_to_send("{\"sn\":");
                    js_send = make_json_to_send(recv_sn_str);
                    js_send = make_json_to_send(",\"cmd\":\"updatewifi\",\"ret\":200}");
                    //SendDataToServer(js_send);
                    make_json_to_send(NULL);
                    char val[96] = {0};
                    if (get_json_value(myjson, "\"url\"", 0, val) > 0)
                    {
                        if (get_upgrade_host_and_path(val, upgrade_file_host, upgrade_file_path) >= 0)
                        {
                            uart0_txstr_buffer("parse path ok..........\r\n");
                            fw_self_update(0);
                        }
                        else
                        {
                            uart0_txstr_buffer("parse path fail..........\r\n");
                        }
                    }

                }
            else if (os_strcmp(tmpStr, "\"error\"") == 0)
            {
            }
            else
            {
            }
        }
        os_free(myjson);
        GNET_MAINTAIN_NUM = 0;
    }
    else
    {

    }
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_sent_cb(void *arg)
{
    char aa[10];
    mqttClient.sendingFlg = false;
    mqttClient.trySendNum = 0;
    mqttClient.recordTimer = 0;
//    os_sprintf(&aa[0], "%u", (tcpip_sn[0]));//lp
//    uart0_txstr_buffer(&aa[0]);//lpDebug
//    struct espconn *pCon = (struct espconn *)arg;
//    MQTT_Client* client = (MQTT_Client *)pCon->reverse;
//    INFO("TCP: Sent\r\n");
//    client->sendTimeout = 0;
//    client->keepAliveTick =0;
//    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_discon_cb(void *arg)
{

    struct espconn *pespconn = (struct espconn *)arg;
    MQTT_Client* client = (MQTT_Client *)pespconn->reverse;

    client->netState = ((client->netState)&0xF9);

    if(mqttClient.abortResetFlg == 0)
    {
        SendNetStateToMcu();

        client->connState = TCP_RECONNECT_REQ;

        if (client->disconnectedCb)
        {
            client->disconnectedCb((uint32_t*)client);
        }
        system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    }
    //uart0_txstr_buffer("disconnect force by host...\r\n");
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_connect_cb(void *arg)
{
    struct espconn *pCon = (struct espconn *)arg;
    MQTT_Client* client = (MQTT_Client *)pCon->reverse;

    espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);
    espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);
    espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);
    client->netState = client->netState|0x02;;//已连接到服务器
    client->sendTimeout = MQTT_SEND_TIMOUT;
    if (client->security) {
    #ifdef MQTT_SSL_ENABLE
            //espconn_secure_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    #else
            //INFO("TCP: Do not support SSL\r\n");
    #endif
    }
    else
    {
        //espconn_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    }
    uart0_txstr_buffer("connected...");//lpDebug
    LoginServer(&mcuBaseInfo[0]);
}

/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recon_cb(void *arg, sint8 errType)
{
    struct espconn *pCon = (struct espconn *)arg;
    MQTT_Client* client = (MQTT_Client *)pCon->reverse;
    client->connState = TCP_RECONNECT_REQ;
    //system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
MQTT_Disconnect(MQTT_Client *mqttClient)
{
    mqttClient->connState = TCP_DISCONNECTING;
    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);
    os_timer_disarm(&mqttClient->mqttTimer);
}

void ICACHE_FLASH_ATTR
MQTT_Connect(MQTT_Client *mqttClient)
{
    uart0_txstr_buffer("Connecting.......\r\n");//lp
    if (mqttClient->pCon)
    {
        // Clean up the old connection forcefully - using MQTT_Disconnect
        // does not actually release the old connection until the
        // disconnection callback is invoked.
        mqtt_tcpclient_delete(mqttClient);
    }
    mqttClient->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
    mqttClient->pCon->type = ESPCONN_TCP;
    mqttClient->pCon->state = ESPCONN_NONE;
    mqttClient->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    mqttClient->pCon->proto.tcp->local_port = espconn_port();
    mqttClient->pCon->proto.tcp->remote_port = mqttClient->port;
    mqttClient->pCon->reverse = mqttClient;
    espconn_regist_connectcb(mqttClient->pCon, mqtt_tcpclient_connect_cb);
    espconn_regist_reconcb(mqttClient->pCon, mqtt_tcpclient_recon_cb);

    mqttClient->keepAliveTick = 0;
    mqttClient->reconnectTick = 0;
    mqttClient->trySendNum = 0;
    mqttClient->recordTimer = 0;
    mqttClient->sendingFlg = false;
//    mqttClient->loginStatus = 0;//在开机初始化参数初始化
    mqttClient->netState = 0x01;
    SendNetStateToMcu();

//    mqttClient->in_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
//    mqttClient->in_buffer_length = MQTT_BUF_SIZE;
//    mqttClient->out_buffer =  (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
//    mqttClient->out_buffer_length = MQTT_BUF_SIZE;

    os_timer_disarm(&mqttClient->mqttTimer);
    os_timer_setfn(&mqttClient->mqttTimer, (os_timer_func_t *)mqtt_timer, mqttClient);
    os_timer_arm(&mqttClient->mqttTimer, 1000, 1);

    if (UTILS_StrToIP(mqttClient->host, &mqttClient->pCon->proto.tcp->remote_ip))
    {
        if (mqttClient->security)
        {
                espconn_secure_connect(mqttClient->pCon);
        }
        else
        {
            espconn_connect(mqttClient->pCon);
        }
    }
    else
    {
        espconn_gethostbyname(mqttClient->pCon, mqttClient->host, &mqttClient->ip, mqtt_dns_found);
    }
    mqttClient->connState = TCP_CONNECTING;
}

void ICACHE_FLASH_ATTR
MQTT_OnConnected(MQTT_Client *mqttClient, MqttCallback connectedCb)
{
    mqttClient->connectedCb = connectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnDisconnected(MQTT_Client *mqttClient, MqttCallback disconnectedCb)
{
    mqttClient->disconnectedCb = disconnectedCb;
}

void ICACHE_FLASH_ATTR SendDataThread()
{
    static uint16 tcpBufLength = 0;
    //数据发送
    if((mqttClient.netState & 0x02) == 0x02)
    {
        if(((mqttClient.sendingFlg == false))&&(mqttClient.recordTimer == 0))//如果队列非空
        {

            tcpBufLength = QueuePush(&stQueue,&tcpSendBuf[0]);//数据出队
            if(tcpBufLength > 0)//如果网络正常
            {
                mqttClient.sendingFlg = true;
                espconn_send(mqttClient.pCon, &tcpSendBuf[0], tcpBufLength);
//                uart0_txstr_buffer("sendData is :\r\n");
//                uart0_send_buffer(&tcpSendBuf[0],tcpBufLength);
//                uart0_txstr_buffer("\r\n");
            }
            else
            {
            }
        }
        else if(mqttClient.sendingFlg == true)
        {
            mqttClient.recordTimer++;
            if((mqttClient.recordTimer)%10 == 0)
            {

                mqttClient.trySendNum++;
                if(mqttClient.trySendNum > 3)//发送3次均失败，可认为网络异常
                {
                    mqttClient.trySendNum = 0;
                    mqttClient.sendingFlg = false;
                    mqttClient.abortResetFlg = 1;
                    if (mqttClient.security)
                    {
                        espconn_secure_disconnect(mqttClient.pCon);
                    }
                    else
                    {
                        espconn_disconnect(mqttClient.pCon);
                    }
                    espconn_delete(mqttClient.pCon);
                    os_delay_us(10000);
                    system_restart();
                }
                else
                {
                    mqttClient.sendingFlg = true;
                    espconn_send(mqttClient.pCon, &tcpSendBuf[0], tcpBufLength);
//                    uart0_txstr_buffer("re senddata is:\r\n");
//                    uart0_send_buffer(&tcpSendBuf[0],tcpBufLength);
//                    uart0_txstr_buffer("\r\n");
                }
            }
            else
            {
            }
        }
        else
        {
        }
    }

    //此处为串口维护一个心跳计时
    if(mqttClient.loginStatus == 2)
    {
        st_uartActivate.sendIntervalTimer++;
        st_uartActivate.revTimerOut++;
        if(st_uartActivate.sendIntervalTimer > 1200)//每10分钟检测
        {
            st_uartActivate.sendIntervalTimer = 0;
            mcuSerialBuf.Buff_Tx[0]=0xAA;
            mcuSerialBuf.Buff_Tx[1]=0x00;
            mcuSerialBuf.Buff_Tx[2]=0x01;
            mcuSerialBuf.Buff_Tx[3]=0x06;
            mcuSerialBuf.Buff_Tx[4]=0x0A;
            uart0_send_buffer(&(mcuSerialBuf.Buff_Tx[0]),5);//向设备查询状态
        }
        if(st_uartActivate.revTimerOut > 1203)//发送到接收超时0.75秒
        {
            st_uartActivate.revTimerOut = 0;
            mqttClient.abortResetFlg = 1;
            if (mqttClient.security)
            {
                espconn_secure_disconnect(mqttClient.pCon);
            }
            else
            {
                espconn_disconnect(mqttClient.pCon);
            }
            espconn_delete(mqttClient.pCon);
            os_delay_us(10000);
            system_restart();
        }
    }
    {

        GNET_MAINTAIN_NUM++;
        if(GNET_MAINTAIN_NUM > 2400)
        {
            mqttClient.abortResetFlg = 1;
            if (mqttClient.security)
            {
                espconn_secure_disconnect(mqttClient.pCon);
            }
            else
            {
                espconn_disconnect(mqttClient.pCon);
            }
            espconn_delete(mqttClient.pCon);
            os_delay_us(10000);
            system_restart();
        }
    }
}

void ICACHE_FLASH_ATTR SendDataToServer(char *data)
{
    unsigned short tcpBufLength = os_strlen(data);
    //数据入列
    QueuePop(&stQueue,data,tcpBufLength);
}

void ICACHE_FLASH_ATTR LoginServer(char *pMcuBaseInfo)
{
    char *js_send = NULL;
    uint16 tmplen = 0;
    char mac_sn[13] = {0};

    make_json_to_send(NULL);

    js_send = make_json_to_send("{\"sn\":");
    tcpip_sn[0] = 0;//连接服务器置Sn号为0
    os_sprintf(mac_sn, "%u", (tcpip_sn[0])++);
    js_send = make_json_to_send(mac_sn);
    //js_send = make_json_to_send("0");
    js_send = make_json_to_send(",\"cmd\":\"login\",\"mac\":\"");
    lp_station_get_mac_str(&mac_sn[0]);
    js_send = make_json_to_send(&mac_sn[0]);
    js_send = make_json_to_send("\",\"pv\":\"");
    js_send = make_json_to_send(LOGIN_PV);
    js_send = make_json_to_send("\",\"sv\":\"");
    js_send = make_json_to_send(LOGIN_SV);
    js_send = make_json_to_send("\",\"uv\":\"");
    js_send = make_json_to_send(LOGIN_UV);
    js_send = make_json_to_send("\"");

    if(pMcuBaseInfo[0] == 0x00)//没有获取到mcu的基本信息
    {

    }
    else//获取到了MCU的基本信息
    {
        js_send = make_json_to_send(",\"data\":[");
        js_send = make_json_to_send(pMcuBaseInfo);
        js_send = make_json_to_send("]");
        mqttClient.loginStatus = 1;//标记此处发送的登陆信息
    }
    js_send = make_json_to_send("}");
    SendDataToServer(js_send);
    tmplen = os_strlen(js_send);
    if(tmplen > 10)
    {
        if((js_send[tmplen-8]== 'b')&&(js_send[tmplen-7]== 'i')&&(js_send[tmplen-6]== 'n'))
        {
            if(js_send[tmplen-4] == '1')
                base64Flag = true;
            else
                base64Flag = false;
        }
        else
        {
            base64Flag = 0;
        }
    }
    make_json_to_send(NULL);
}

/**
  获取MAC 地址
  */
bool ICACHE_FLASH_ATTR
lp_station_get_mac_str(char *mac)
{
    uint8_t bssid[6] = {0};
    wifi_get_macaddr(STATION_IF, bssid);//查询 MAC 地址
    os_sprintf(mac, "%02X%02X%02X%02X%02X%02X" "\0", MAC2STR(bssid));
}

sint8 ICACHE_FLASH_ATTR station_get_rssi()
{
    if (wifi_station_get_connect_status() == STATION_GOT_IP)
    {
        return wifi_station_get_rssi();
    }
    else
    {
        return 31;
    }

}

char ICACHE_FLASH_ATTR station_get_ap_quality()
{
    sint8 rssi = -100;
    char quality = 0;
    if ((rssi = station_get_rssi()) != 31)
    {
        if (rssi <= -100)
                quality = 0;
        else if (rssi >= -50)
                quality = 100;
        else
                quality = (rssi + 100) << 1;
    }
    return quality;
}
