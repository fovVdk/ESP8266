
#include "fifo.h"


/*	$Function		:  Init_Queue		
==  ===============================================================
==	Description		:	
==	Argument		:	*Q
==					:	
== 	Return			:	none
==	Modification	:	2014-03-05        Lp		Create
==  ===============================================================
*/
void InitQueue(CirQueue *Q)
{
    uint8 i;
    uint16 j;
	Q->front = Q->rear = 0;
	Q->count = 0;
    for(i = 0;i < 3;i++)
    {
        for(j = 0;j < BUFF_SIZE;j++)
        {
            Q->receive[i][j] = 0x00;
        }
        Q->receiveSize[i] = 0;
    }
}



/*	$Function		:  QueueEmpty		
==  ===============================================================
==	Description		:	
==	Argument		:	*Q
==					:	
== 	Return			:	if count =0 is return 1,if not return 0
==	Modification	:	2014-03-05        Lp		Create
==  ===============================================================
*/

uint16 QueueEmpty(CirQueue *Q)
{
	return Q->count == 0;           
}



/*	$Function		:  QueueFull		
==  ===============================================================
==	Description		:	
==	Argument		:	*Q
==					:	
== 	Return			:	if count = Queuesize is return 1,if not return 0
==	Modification	:	2014-03-05        Lp		Create
==  ===============================================================
*/

uint16 QueueFull(CirQueue *Q)
{
    return Q->count == QUEUE_SIZE;
}


uint8 QueuePop(CirQueue *Q,uint8 * buf,uint16 length)
{
    uint16 i;
    if(QueueFull(Q))
    {
        return 0;
    }

    Q->count++;
    for(i = 0;i < length;i++)
    {
        Q->receive[Q->rear][i] = *buf++;
    }
    Q->receiveSize[Q->rear] = length;
    Q->rear = (Q->rear+1)%QUEUE_SIZE;
    return 1;
}

uint16 QueuePush(CirQueue *Q,uint8 * dat)
{
    uint16 i;
    if(QueueEmpty(Q))
    {
        return 0;
    }
    for(i = 0;i < Q->receiveSize[Q->front];i++)
    {
        *dat = Q->receive[Q->front][i];
        dat++;
    }
    Q->receiveSize[Q->front] = 0;
    Q->front = (Q->front +1)%QUEUE_SIZE;
    Q->count--;
    return i;
}


