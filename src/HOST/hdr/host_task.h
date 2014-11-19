/* -------------------------------------------------------------------------
 * File Name   : host_task.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : HOST Task
 * History     : 14/08/15    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_TASK_H__
#define __HOST_TASK_H__


extern void *tcp_rcvTask(void *argc);
extern void *fifo_getTask(void *argc);
extern void *CheckVmStatusTask(void *argc);
extern void *CheckTcpStatusTask(void *argc);
extern void *ResetWatchdogTask(void *argc);
extern void *ProcessVmStateTask(void *argc);

extern int SetBypassWatchdog(void);
extern int ResetBypassWatchdog(void);

#endif /* __HOST_TASK_H__ */
