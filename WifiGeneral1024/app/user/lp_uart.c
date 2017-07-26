#include "lp_uart.h"


#define uart_recvTaskQueueLen  10
os_event_t uart_recvTaskQueue[uart_recvTaskQueueLen];

UartDevice UartDev;
UartActivate st_uartActivate;

struct UartDataBuf mcuSerialBuf;
uint8_t mcuCurrentInfo[300]={0x00};
uint8_t mcuBaseInfo[100]={0x00};
os_timer_t obtainBaseMcuTimer;
unsigned short tcpip_sn[2];

extern MQTT_Client mqttClient;
extern bool base64Flag;

LOCAL os_timer_t configNetTimer;

STATUS uart0_tx_one_char(uint8 c) //串口发送单个字符
{

//    if (((READ_PERI_REG(UART_STATUS(UART0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) < 126)
//    {
//            WRITE_PERI_REG(UART_FIFO(UART0), c);
//    }
//    return OK;
    while(true)
    {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(UART0)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
        {
            break;
        }
    }
    WRITE_PERI_REG(UART_FIFO(UART0), c);
    return OK;
}

void ICACHE_FLASH_ATTR
uart0_send_buffer(char *buf, int len)  //串口发送数据，指定发送数据长度
{
    int i;
    for (i = 0; i < len; ++i)
    {
            uart0_tx_one_char(buf[i]);
    }
}

void ICACHE_FLASH_ATTR
uart0_txstr_buffer(char *buf)
{
    while (*buf)
    {
        uart0_tx_one_char(*buf++);
    }
}


static void zyt_uart_rx_intr_disable(uint8 uart_no)
{
#if 1
        CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
#else
        ETS_UART_INTR_DISABLE();
#endif
}

static void zyt_uart_rx_intr_enable(uint8 uart_no)
{
#if 1
        SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
#else
        ETS_UART_INTR_ENABLE();
#endif
}

//开机3S后
void ICACHE_FLASH_ATTR
obtainBaseMcuInfoTimerCb()
{
    if(mqttClient.obtainInfoFlg == 0)
    {
        char ret[5] = {0xAA, 0x00, 0x01, 0x01,0x0A};
        char mac[13] = {0};
        char send_sn_str[6] = {0};
        char *js_send = NULL;
        make_json_to_send(NULL);
        js_send = make_json_to_send("{\"sn\":");
        if (tcpip_sn[0] >= 65535)
            tcpip_sn[0] = 0;
        os_sprintf(send_sn_str, "%u", (tcpip_sn[0])++);
        js_send = make_json_to_send(send_sn_str);
        js_send = make_json_to_send(",\"cmd\":\"error\",\"ret\":404,\"mac\":\"");
        lp_station_get_mac_str(mac);
        js_send = make_json_to_send(mac);
        js_send = make_json_to_send("\"}");
        SendDataToServer(js_send);
        make_json_to_send(NULL);
        uart0_send_buffer(ret,5);
    }
    else
    {
        if(mqttClient.loginStatus == 2)
        {
            os_timer_disarm(&obtainBaseMcuTimer);
        }
        else
        {
            LoginServer(&mcuBaseInfo[0]);
        }
    }
}
static void ICACHE_FLASH_ATTR
configNetTimerCb(void *arg)
{
    mqttClient.netState = mqttClient.netState & 0x7F;
    os_timer_disarm(&configNetTimer);
	os_delay_us(10000);
	system_restart();
}
static void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
        switch (status)
        {
        case SC_STATUS_WAIT:
                break;
        case SC_STATUS_FIND_CHANNEL:
                break;
        case SC_STATUS_GETTING_SSID_PSWD:
        {
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH)
            {

            }
            else
            {
            }
        }break;
        case SC_STATUS_LINK:
        {
            struct station_config *sta_conf = pdata;
            wifi_station_set_config(sta_conf);
            wifi_station_disconnect();
            wifi_station_connect();
        }break;
        case SC_STATUS_LINK_OVER:
            if (pdata != NULL)
            {
                uint8 phone_ip[4] = {0};
                os_memcpy(phone_ip, (uint8*) pdata, 4);
            }
			os_timer_disarm(&configNetTimer);
            smartconfig_stop();
            break;
        }
}

void ICACHE_FLASH_ATTR
SendNetStateToMcu()
{
    uint8_t sendData[8];
    sendData[0]=0xAA;
    sendData[1]=0x00;
    sendData[2]=0x04;
    sendData[3]=0x05;
    if((mqttClient.netState & 0x04) == 0x04)
    {
        sendData[4]=0x01;//配置路由器
        sendData[5]=0x01;//连接路由器
        sendData[6]=0x01;//连接服务器
    }
    else if((mqttClient.netState & 0x01) == 0x01)
    {
        sendData[4]=0x01;//配置路由器
        sendData[5]=0x01;//连接路由器
        sendData[6]=0x00;//连接服务器
    }
    else if((mqttClient.netState & 0x80) == 0x80)
    {
        sendData[4]=0x01;//配置路由器
        sendData[5]=0x00;//连接路由器
        sendData[6]=0x00;//连接服务器
    }
    else
    {
        sendData[4]=0x00;//配置路由器
        sendData[5]=0x00;//连接路由器
        sendData[6]=0x00;//连接服务器
    }
    sendData[7]=0x0A;
    uart0_send_buffer(&sendData[0],8);
}

LOCAL void ICACHE_FLASH_ATTR
uart_tasks_proc()
{
    uint16 payloadLength;
    //校验头和尾
    if((mcuSerialBuf.Buff[0] == 0xAA)&&(mcuSerialBuf.Buff[mcuSerialBuf.length-1] == 0x0A))
    {
        payloadLength = mcuSerialBuf.Buff[1]*256 + mcuSerialBuf.Buff[2];

        switch (mcuSerialBuf.Buff[3])
        {
            case 0x01://查询设备基础信息
            {
                convert_pkt2json_field(&mcuSerialBuf.Buff[4],payloadLength-1,&mcuBaseInfo[0]);//长度减一
                mqttClient.obtainInfoFlg = 1;
            }break;
            case 0x02://设备控制
            {
                //00 表示执行成功;01表示执行失败
            }break;
            case 0x03://上传状态
            {
                char *js_send = NULL;
                char send_sn_str[6] = {0};
                int n;
                char *js_data_value = NULL;

                //st_uartActivate.sendIntervalTimer = 0;
                st_uartActivate.revTimerOut       = 0;

                if(base64Flag == true)
                {
                    js_data_value = (char*) os_malloc(base64_len_of_plain(payloadLength) + 3);
                    js_data_value[0] = '"';
                    n = encode_plain_to_base64(js_data_value + 1, &mcuSerialBuf.Buff[4], payloadLength - 1);
                    js_data_value[n + 1] = '"';
                    js_data_value[n + 2] = '\0';
                    os_memcpy(&mcuCurrentInfo[0],&js_data_value[0],os_strlen(js_data_value));//备份一份状态信息，以被查询
                }
                else
                {
                    js_data_value = (char *) os_malloc(payloadLength + 128);
                    n = convert_pkt2json_field(&mcuSerialBuf.Buff[4], payloadLength - 1, js_data_value);
                    os_memcpy(&mcuCurrentInfo[0],&js_data_value[0],os_strlen(js_data_value));//备份一份状态信息，以被查询
                }

                make_json_to_send(NULL);
                js_send = make_json_to_send("{\"sn\":");
                if (tcpip_sn[0] >= 65535)
                    tcpip_sn[0] = 0;
                os_sprintf(send_sn_str, "%u", (tcpip_sn[0])++);
                js_send = make_json_to_send(send_sn_str);
                js_send = make_json_to_send(",\"cmd\":\"upload\",\"mac\":\"");
                char mac[13] = {0};
                lp_station_get_mac_str(mac);
                js_send = make_json_to_send(mac);
                js_send = make_json_to_send("\",\"data\":[");
                js_send = make_json_to_send(&js_data_value[0]);
                js_send = make_json_to_send("]}");

                UploadStatusServer(js_send);
                make_json_to_send(NULL);
                os_free(js_data_value);

//                mac[0] = 0xAA;mac[1] = 0x00;mac[2] = 0x01;mac[3] = 0x03;mac[4] = 0x0A;
//                uart0_send_buffer(&mac[0],5);
            }break;
            case 0x04://错误事件
            {

            }break;
            case 0x05://查询网络状态信息
            {
                SendNetStateToMcu();
                st_uartActivate.sendIntervalTimer = 0;
                st_uartActivate.revTimerOut       = 0;
            }break;
            case 0x06://查询设备状态信息
            {
                char *js_send = NULL;
                char send_sn_str[6] = {0};
                int n;
                char *js_data_value = NULL;
                //st_uartActivate.sendIntervalTimer = 0;
                st_uartActivate.revTimerOut       = 0;
                if(base64Flag == true)
                {
                    js_data_value = (char*) os_malloc(base64_len_of_plain(payloadLength) + 3);
                    js_data_value[0] = '"';
                    n = encode_plain_to_base64(js_data_value + 1, &mcuSerialBuf.Buff[4], payloadLength - 1);
                    js_data_value[n + 1] = '"';
                    js_data_value[n + 2] = '\0';
                    os_memcpy(&mcuCurrentInfo[0],&js_data_value[0],os_strlen(js_data_value));//备份一份状态信息，以被查询
                }
                else
                {
                    js_data_value = (char *) os_malloc(payloadLength + 128);
                    n = convert_pkt2json_field(&mcuSerialBuf.Buff[4], payloadLength - 1, js_data_value);
                    os_memcpy(&mcuCurrentInfo[0],&js_data_value[0],os_strlen(js_data_value));//备份一份状态信息，以被查询
                }

                make_json_to_send(NULL);
                js_send = make_json_to_send("{\"sn\":");
                if (tcpip_sn[0] >= 65535)
                    tcpip_sn[0] = 0;
                os_sprintf(send_sn_str, "%u", (tcpip_sn[0])++);
                js_send = make_json_to_send(send_sn_str);
                js_send = make_json_to_send(",\"cmd\":\"upload\",\"mac\":\"");
                char mac[13] = {0};
                lp_station_get_mac_str(mac);
                js_send = make_json_to_send(mac);
                js_send = make_json_to_send("\",\"data\":[");
                js_send = make_json_to_send(&js_data_value[0]);
                js_send = make_json_to_send("]}");
                UploadStatusServer(js_send);
                make_json_to_send(NULL);
                os_free(js_data_value);
            }break;
            case 0x07://开启smartlink配网
            {
                char ret[6] = {0xAA, 0x00, 0x02, 0x07, 0xC8, 0x0A};
                uart0_send_buffer(ret,6);
                mqttClient.netState = 0x80;

                /*start loginserver flg*/
                mqttClient.loginStatus = 1;
                os_timer_disarm(&obtainBaseMcuTimer);
                os_timer_setfn(&obtainBaseMcuTimer, (os_timer_func_t *)obtainBaseMcuInfoTimerCb, NULL);
                os_timer_arm(&obtainBaseMcuTimer, 5000,1);
                /*end */

                smartconfig_stop();
                smartconfig_start(smartconfig_done);    
                os_timer_disarm(&configNetTimer);
                os_timer_setfn(&configNetTimer, (os_timer_func_t *)configNetTimerCb, NULL);
                os_timer_arm(&configNetTimer, 60000, 0);//定时70秒用于SmartLink计时
                SendNetStateToMcu();
            }break;
            case 0x08://wifi信号强度
            {
                char ret[] = {0xAA, 0x00, 0x02, 0x08, 0, 0x0A, 0x00};
                ret[4] = station_get_ap_quality();
                uart0_send_buffer(ret, 6);
            }break;

            default:
                break;
        }
    }
}

LOCAL void ICACHE_FLASH_ATTR
uart_recvTask(os_event_t *events)
{
    if (0 == events->sig)
    {
        int i = 0;
        int fifo_len;
        fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;//fifo剩余能用的长度
        mcuSerialBuf.length = 0;
        for (i = 0; i < fifo_len; ++i)//读串口数据
        {
            mcuSerialBuf.Buff[mcuSerialBuf.length++] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
        }
        if(mcuSerialBuf.length>0)
        {
            uart_tasks_proc();
        }
        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
        zyt_uart_rx_intr_enable(UART0);
    }
}

static void uart0_rx_intr_handler(void *para)
{
    if (UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_FRM_ERR_INT_ST))
    {
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_FRM_ERR_INT_CLR);
    }
    else if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST))
    {
            zyt_uart_rx_intr_disable(UART0);
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
            system_os_post(USER_TASK_PRIO_0, 0, 0);
    }
    else if (UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST))
    {
            zyt_uart_rx_intr_disable(UART0);
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
            system_os_post(USER_TASK_PRIO_0, 0, 0);
    }
    else if (UART_RXFIFO_OVF_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_OVF_INT_ST))
    {
            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_OVF_INT_CLR);

    }
}
static void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
    if (uart_no == UART1)
    {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    }
    else
    {
        /* rcv_buff size if 0x100 */
        ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, &(UartDev.rcv_buff));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    }
    uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate)); // SET BAUDRATE

    WRITE_PERI_REG(UART_CONF0(uart_no), ((UartDev.exist_parity & UART_PARITY_EN_M) << UART_PARITY_EN_S) //SET BIT AND PARITY MODE
                    | ((UartDev.parity & UART_PARITY_M) << UART_PARITY_S) | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S) | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));

    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);//RESET FIFO
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

    if (uart_no == UART0)
    {
        //set rx fifo trigger
        #if 1
        WRITE_PERI_REG(UART_CONF1(uart_no),
            ((120 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) | (0x08 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S | UART_RX_TOUT_EN | ((0x10 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S)); //wjl
        #else
        WRITE_PERI_REG(UART_CONF1(uart_no),
            ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) | (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S | UART_RX_TOUT_EN | ((0x10 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S)); //wjl
        #endif
        SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA);
    }
    else
    {
        WRITE_PERI_REG(UART_CONF1(uart_no),
            ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S)); //TrigLvl default val == 1
    }
    WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
}

void ICACHE_FLASH_ATTR
lp_uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
    system_os_task(uart_recvTask, USER_TASK_PRIO_0, uart_recvTaskQueue, uart_recvTaskQueueLen);
    UartDev.baut_rate = uart0_br;
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();


    st_uartActivate.revTimerOut = 0;
    st_uartActivate.sendIntervalTimer = 0;

    os_timer_disarm(&obtainBaseMcuTimer);
    os_timer_setfn(&obtainBaseMcuTimer, (os_timer_func_t *)obtainBaseMcuInfoTimerCb, NULL);
    os_timer_arm(&obtainBaseMcuTimer, 5000,1);

}



