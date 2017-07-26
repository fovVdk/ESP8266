
#ifndef __FIFO_H_
#define __FIFO_H_

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


#define BUFF_SIZE  512
#define QUEUE_SIZE  3

typedef struct _TagCirQueue
{
    uint16 front;
    uint16 rear;
    uint16 count;
    uint16 receiveSize[3];
    uint8  receive[QUEUE_SIZE][BUFF_SIZE];
}CirQueue;


uint8 QueuePop(CirQueue *Q,uint8 * buf,uint16 length);
uint16 QueuePush(CirQueue *Q,uint8 * dat);
void InitQueue(CirQueue * Q);



#endif




