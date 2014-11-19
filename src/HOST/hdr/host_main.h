/* -------------------------------------------------------------------------
 * File Name   : host_main.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : HOST(BPAPP HOST Server)
 * History     : 14/07/03    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_MAIN_H__
#define __HOST_MAIN_H__


#define BPAPP_ENV_NAME     "BPAPP_HOME"     /* Home Directory Path */


#ifndef TRUE
#define TRUE               1
#endif

#ifndef FALSE
#define FALSE              0
#endif


/* pthread */
#define PTASK0             0 /* tcp_rcvTask          */
#define PTASK1             1 /* fifo get Task        */
#define PTASK2             2 /* check vmstatus Task  */
#define PTASK3             3 /* check tcpstatus Task */
#define PTASK4             4 /* Reset Watchdog Task  */
#define PTASK5             5 /* Process Vm State Task*/

#define MAX_PTASK          6

/* bpyass mode */
#define BYPASS_MODE_ON     0x01
#define BYPASS_MODE_OFF    0x02


extern pthread_mutex_t TcpMutex;
extern pthread_mutex_t BypassMutex;
extern pthread_mutex_t StateMutex;

extern int            ThreadFlags[MAX_PTASK];
extern int            ThreadAlive[MAX_PTASK];
extern pthread_t      ThreadArray[MAX_PTASK];
extern pthread_attr_t Thread_Attr[MAX_PTASK];

extern int  RunState;
extern char LogPath[512];

extern int CHECK_VMCSTATUS_SEC; /* sec  */
extern int CHECK_TCPSTATUS_MIN; /* min  */
extern int CHECK_WATCHDOG_MSEC; /* msec */

#endif /* __HOST_MAIN_H__ */


