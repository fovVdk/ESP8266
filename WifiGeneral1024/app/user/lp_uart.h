#ifndef LP_UART_H_
#define LP_UART_H_

#include "stdbool.h"
#include "uart_register.h"
#include "eagle_soc.h"
#include "osapi.h"
#include "c_types.h"

#include "user_interface.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"
#include "wifi.h"
#include "smartconfig.h"

#include "base64.h"

#define PACKET_HEAD      0xAA
#define PACKET_END       0x0A
#define PAYLOAD_COLON    0x3A
#define PAYLOAD_GAP      0x00
#define PAYLOAD_LEN_MAX  1024
#define PACKET_LEN       1030


#define UART0   0
#define UART1   1

#define UART_BUFF_SIZE   600

typedef struct _UartActivate
{
    uint16 sendIntervalTimer;
    uint16 revTimerOut;
}UartActivate;

typedef enum
{
        FIVE_BITS = 0x0, SIX_BITS = 0x1, SEVEN_BITS = 0x2, EIGHT_BITS = 0x3
} UartBitsNum4Char;

typedef enum
{
        ONE_STOP_BIT = 0x1, ONE_HALF_STOP_BIT = 0x2, TWO_STOP_BIT = 0x3
} UartStopBitsNum;

typedef enum
{
        NONE_BITS = 0x2, ODD_BITS = 1, EVEN_BITS = 0
} UartParityMode;

typedef enum
{
        STICK_PARITY_DIS = 0, STICK_PARITY_EN = 1
} UartExistParity;

typedef enum
{
        UART_None_Inverse = 0x0,
        UART_Rxd_Inverse = UART_RXD_INV,
        UART_CTS_Inverse = UART_CTS_INV,
        UART_Txd_Inverse = UART_TXD_INV,
        UART_RTS_Inverse = UART_RTS_INV,
} UART_LineLevelInverse;

typedef enum
{
        BIT_RATE_300 = 300,
        BIT_RATE_600 = 600,
        BIT_RATE_1200 = 1200,
        BIT_RATE_2400 = 2400,
        BIT_RATE_4800 = 4800,
        BIT_RATE_9600 = 9600,
        BIT_RATE_19200 = 19200,
        BIT_RATE_38400 = 38400,
        BIT_RATE_57600 = 57600,
        BIT_RATE_74880 = 74880,
        BIT_RATE_115200 = 115200,
        BIT_RATE_230400 = 230400,
        BIT_RATE_460800 = 460800,
        BIT_RATE_921600 = 921600,
        BIT_RATE_1843200 = 1843200,
        BIT_RATE_3686400 = 3686400,
} UartBautRate;

typedef enum
{
        NONE_CTRL, HARDWARE_CTRL, XON_XOFF_CTRL
} UartFlowCtrl;

typedef enum
{
        USART_HardwareFlowControl_None = 0x0,
        USART_HardwareFlowControl_RTS = 0x1,
        USART_HardwareFlowControl_CTS = 0x2,
        USART_HardwareFlowControl_CTS_RTS = 0x3
} UART_HwFlowCtrl;

typedef enum
{
        EMPTY, UNDER_WRITE, WRITE_OVER
} RcvMsgBuffState;

typedef struct
{
        uint32 RcvBuffSize;
        uint8 *pRcvMsgBuff;
        uint8 *pWritePos;
        uint8 *pReadPos;
        uint8 TrigLvl; //JLU: may need to pad
        RcvMsgBuffState BuffState;
} RcvMsgBuff;

typedef struct
{
        uint32 TrxBuffSize;
        uint8 *pTrxBuff;
} TrxMsgBuff;

typedef enum
{
        BAUD_RATE_DET, WAIT_SYNC_FRM, SRCH_MSG_HEAD, RCV_MSG_BODY, RCV_ESC_CHAR,
} RcvMsgState;

typedef struct
{
        UartBautRate baut_rate;
        UartBitsNum4Char data_bits;
        UartExistParity exist_parity;
        UartParityMode parity;
        UartStopBitsNum stop_bits;
        UartFlowCtrl flow_ctrl;
        RcvMsgBuff rcv_buff;
        TrxMsgBuff trx_buff;
        RcvMsgState rcv_state;
        int received;
        int buff_uart_no;  //indicate which uart use tx/rx buffer
} UartDevice;

struct UartBuffer
{
        uint32 UartBuffSize;
        uint8 *pUartBuff;
        uint8 *pInPos;
        uint8 *pOutPos;
        STATUS BuffState;
        uint16 Space;  //remanent space of the buffer
        uint8 TcpControl;
        struct UartBuffer * nextBuff;
};

struct UartRxBuff
{
        uint32 UartRxBuffSize;
        uint8 *pUartRxBuff;
        uint8 *pWritePos;
        uint8 *pReadPos;
        STATUS RxBuffState;
        uint32 Space;  //remanent space of the buffer
};

struct UartDataBuf{
    uint16 length;
    uint8 Buff[UART_BUFF_SIZE];
    uint16 lengthTx;
    uint8 Buff_Tx[UART_BUFF_SIZE];
};
STATUS uart0_tx_one_char(uint8 c);
void ICACHE_FLASH_ATTR uart0_send_buffer(char *buf, int len);
void ICACHE_FLASH_ATTR uart0_txstr_buffer(char *buf);
void ICACHE_FLASH_ATTR SendNetStateToMcu();
#endif
