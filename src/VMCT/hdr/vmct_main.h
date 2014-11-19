/* -------------------------------------------------------------------------
 * File Name   : vmct_main.h
 * Author      : Hyenog-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP VMCT main header.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __VMCT_MAIN_H__
#define __VMCT_MAIN_H__

#define BPAPP_ENV_NAME     "BPAPP_HOME"     /* Home Directory Path */

#ifndef TRUE
#define TRUE     1
#endif
#ifndef FALSE
#define FALSE    0
#endif

/* pthread */
#define PTASK0           0  /* TcpRcvTask         */
#define PTASK1           1  /* TcpSessionTask     */
#define PTASK2           2  /* TmrCmdTask         */


#define MAX_PTASK        3


extern pthread_mutex_t TcpHostMutex;


extern int            ThreadFlags[MAX_PTASK];
extern int            ThreadAlive[MAX_PTASK];
extern pthread_t      ThreadArray[MAX_PTASK];
extern pthread_attr_t Thread_Attr[MAX_PTASK];



#endif /* __VMCT_MAIN_H__ */
