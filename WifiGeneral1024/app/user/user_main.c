#include "ets_sys.h"
#include "lp_uart.h"
#include "osapi.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"

MQTT_Client mqttClient;
TemporaryServer temporaryServer;
void wifiConnectCb(uint8_t status)
{
    if(status == STATION_GOT_IP){
            MQTT_Connect(&mqttClient);
            mqttClient.netState = mqttClient.netState|0x1;
    } else {
            MQTT_Disconnect(&mqttClient);
            mqttClient.netState = mqttClient.netState&0xF8;
            SendNetStateToMcu();//网络状态发生变化即下报
    }
}

void ICACHE_FLASH_ATTR InitNetLed()
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
}

void ICACHE_FLASH_ATTR
InitParm()
{
    mqttClient.security          = 0;
    mqttClient.netState          = 0x00;
    mqttClient.obtainInfoFlg     = 0;
    mqttClient.loginStatus       = 0;
    mqttClient.keepAliveTick     = 0;
    mqttClient.heartCount        = 60;
    mqttClient.heartStopNum      = 0;
    mqttClient.forbidNum         = 0;
    mqttClient.forbidCount       = 0;
    mqttClient.allowSendFlg      = 1;
    mqttClient.sendingFlg        = false;
    mqttClient.trySendNum        = 0;
    mqttClient.recordTimer       = 0;
    mqttClient.abortResetFlg     = 0;
}

void ICACHE_FLASH_ATTR
sys_init_done_cb()
{
    WIFI_Connect("HUAWEI-B315-C2ED", "81047675", wifiConnectCb);
    char ret[5] = {0xAA, 0x00, 0x01, 0x01,0x0A};
    uart0_send_buffer(ret,5);
}

void user_init(void)
{
//        uart_init(BIT_RATE_115200, BIT_RATE_115200);
        os_delay_us(1000000);

        os_strcpy(&(temporaryServer.serverAddr[0]),"t3.skyware.com.cn");
        temporaryServer.port = 9999;
        InitNetLed();
        UART_SetPrintPort(1);
        lp_uart_init(BIT_RATE_9600, BIT_RATE_9600);
        //CFG_Load();
        MQTT_InitConnection(&mqttClient,&(temporaryServer.serverAddr[0]),temporaryServer.port,0);
        //MQTT_InitConnection(&mqttClient, "t3.skyware.com.cn", 9999,0);
        //MQTT_InitConnection(&mqttClient, "d1.skyware.com.cn", 9999,0);
        //MQTT_InitConnection(&mqttClient, "192.168.20.188", 9999, 0);

        InitParm();//初始化参��?放入MQTT_InitConnection��?

        system_init_done_cb(sys_init_done_cb);
}
