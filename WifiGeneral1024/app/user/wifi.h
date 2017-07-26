#ifndef WIFI_H_
#define WIFI_H_

#include "os_type.h"
#include "user_interface.h"
#include "utils.h"
#include "zyt_json.h"
#include "lp_uart.h"
#include "wifi.h"
#include "base64.h"
#include "gpio.h"
#include "fifo.h"


#define MQTT_RECONNECT_TIMEOUT 	10	/*second*/
#define MQTT_BUF_SIZE		1024

#define MQTT_TASK_PRIO        	2
#define MQTT_TASK_QUEUE_SIZE    1
#define MQTT_SEND_TIMOUT	5

#define HEART_STOP_COUNT        3//若3次无心跳，则认为网络异常


#define LOGIN_PV  "1.4"
#define LOGIN_SV  "0.2"
#define LOGIN_UV  "1"

typedef struct _TemporaryServer
{
    char serverAddr[50];//主机名最长为49字节
    uint32_t port;
}TemporaryServer;

typedef enum {
        WIFI_INIT,
        WIFI_CONNECTING,
        WIFI_CONNECTING_ERROR,
        WIFI_CONNECTED,
        DNS_RESOLVE,
        TCP_DISCONNECTING,
        TCP_DISCONNECTED,
        TCP_RECONNECT_DISCONNECTING,
        TCP_RECONNECT_REQ,
        TCP_RECONNECT,
        TCP_CONNECTING,
        TCP_CONNECTING_ERROR,
        TCP_CONNECTED,
        MQTT_CONNECT_SEND,
        MQTT_CONNECT_SENDING,
        MQTT_SUBSCIBE_SEND,
        MQTT_SUBSCIBE_SENDING,
        MQTT_DATA,
        MQTT_KEEPALIVE_SEND,
        MQTT_PUBLISH_RECV,
        MQTT_PUBLISHING,
        MQTT_DELETING,
        MQTT_DELETED,
        TCP_HEART,
} tConnState;

typedef void (*MqttCallback)(uint32_t *args);
typedef void (*MqttDataCallback)(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t lengh);

typedef void (*WifiCallback)(uint8_t);

typedef struct
{
        struct espconn *pCon;
        uint8_t security;
        uint8_t* host;
        uint32_t port;
        ip_addr_t ip;
        //mqtt_state_t  mqtt_state;
//        uint8_t* in_buffer;
//        uint8_t* out_buffer;
//        int in_buffer_length;
//        int out_buffer_length;
        uint8_t netState;
        uint8_t obtainInfoFlg;
        uint8_t loginStatus;//未获取到设备信息：0；已获取到设备信息：1；成功完成注册：2。
        //mqtt_connect_info_t connect_info;
        MqttCallback connectedCb;
        MqttCallback disconnectedCb;
        MqttCallback publishedCb;
        MqttCallback timeoutCb;
        MqttDataCallback dataCb;
        ETSTimer mqttTimer;
        uint16_t keepAliveTick;
        uint16_t heartCount;
        uint8_t  heartStopNum;
        uint16_t reconnectTick;
        uint32_t sendTimeout;
        short    forbidNum;
        short    forbidCount;
        uint8    allowSendFlg;
        bool     sendingFlg;
        uint8    trySendNum;
        uint8    recordTimer;
        uint8    abortResetFlg;
        tConnState connState;
        //void* user_data;
} MQTT_Client;

#define SEC_NONSSL 0
#define SEC_SSL	1

void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb);
void ICACHE_FLASH_ATTR MQTT_InitConnection(MQTT_Client *mqttClient, uint8_t* host, uint32_t port, uint8_t security);
void ICACHE_FLASH_ATTR MQTT_Connect(MQTT_Client *mqttClient);
bool ICACHE_FLASH_ATTR lp_station_get_mac_str(char *mac);
void ICACHE_FLASH_ATTR LoginServer(char *pMcuBaseInfo);
void ICACHE_FLASH_ATTR SendDataToServer(char *data);

char ICACHE_FLASH_ATTR station_get_ap_quality();
sint8 ICACHE_FLASH_ATTR station_get_rssi();
void ICACHE_FLASH_ATTR HeartBeat();
void ICACHE_FLASH_ATTR UploadStatusServer(char *pData);
void ICACHE_FLASH_ATTR SendDataThread();
#endif


