/* -------------------------------------------------------------------------
 * File Name   : vmct_task.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP-VMCT Task fn.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __VMCT_TASK_H__
#define __VMCT_TASK_H__


#define TMR_TCP_RECONNECT_MSG   0x0100
#define TMR_TEST_MSGQ_MSG       0x0200

#define DBM_CHECKTIME_DBTABLE   ((3600*4)+(60*4)+4) /* 04:04:04 */

extern void *TcpRcvTask(void *argc);
extern void *TcpSessionTask(void *argc);
extern void *TmrCmdTask(void *argc);

#endif /* __VMCT_TASK_H__ */
