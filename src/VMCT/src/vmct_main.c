/* -------------------------------------------------------------------------
 * File Name   : vmct_main.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP VMCT main fn.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "bpapp.h"

#include "xprint.h"
#include "logdir.h"
#include "kutil.h"


#include "vmct_main.h"
#include "vmct_conf.h"
#include "vmct_task.h"
#include "vmct_sock.h"
#include "vmct_debug.h"


int  RunState = 0;

pthread_mutex_t TcpHostMutex;

int            ThreadFlags[MAX_PTASK];
int            ThreadAlive[MAX_PTASK];
pthread_t      ThreadArray[MAX_PTASK];
pthread_attr_t Thread_Attr[MAX_PTASK];


static void vmct_shutdown(void);
static void vmct_signal_handler(int signum);
static void InitDebug(char *path, unsigned int mask, unsigned char level, unsigned int trace);


static void vmct_shutdown(void)
{
    int i;
    static unsigned char once = 0;

    if (once) {
        return;
    } else {
        once = 1;
    }

    /* socket fd close (HOST TCP - Client)  */
    pthread_mutex_lock(&TcpHostMutex);
    if (tlinkHOST.mode == LINK_MODE_CLIENT) {
        if (tlinkHOST.ListenFD > 0) {
            close(tlinkHOST.ListenFD);
            FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
            tlinkHOST.ListenFD = -1;
        }

    } else if (tlinkHOST.mode == LINK_MODE_SERVER) {
        for (i = 0; i < MAX_LISTENQ; i++) {
            if (alist[i].AcceptFD > 0) {
                close(alist[i].AcceptFD);
                FD_CLR(alist[i].AcceptFD, &allsetHOST);
                alist[i].AcceptFD = -1;
            }
        }
        if (tlinkHOST.ListenFD > 0) {
            close(tlinkHOST.ListenFD);
            FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
            tlinkHOST.ListenFD = -1;
        }

    }
    pthread_mutex_unlock(&TcpHostMutex);

    for (i = 0; i < MAX_PTASK; i++) {
        if (ThreadFlags[i] != TRUE) {
            continue;
        }
        ThreadAlive[i] = FALSE;
        xprint(FMS_VMC | FMS_LOOKS, "%s :: ThreadFlags(%d)\n", __func__, i);
        pthread_cancel(ThreadArray[i]);
        nanoSleep(10);
    }

    pthread_mutex_destroy(&TcpHostMutex);

    close_xprint();
    terminateLog();
    exit(0);
}


/* Signal handler */
static void vmct_signal_handler(int signum)
{
    xprint(FMS_VMC | FMS_LOOKS, "Received Signal(%d)\n", signum);

    vmct_shutdown();

    RunState = 0;
}

static void InitDebug(char *path, unsigned int mask, unsigned char level, unsigned int trace)
{
    char LogPath[1024];
    unsigned int  DbgMask  = mask;
    unsigned int  DbgTrace = trace;
    unsigned char DbgLevel = level;

    if (init_xprint(DbgMask, DbgLevel, DbgTrace) < 0) {
        fprintf(stderr, "init_xprint error, So exit\n");
        exit(0);
    }

    /* 로그 디렉토리 및 로그파일의 초기화 */
    sprintf(LogPath, "%s/%s", path, "log/VMCT");
    if (initializeLog(LogPath) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "initializeLog error, So exit\n");
        close_xprint();
        exit(0);
    }

    return;
}

int main(int argc, char *argv[])
{
    char *pEnv;

    /*--------------------------------------------------------------------------
      중복 프로세스 체크
      ---------------------------------------------------------------------------*/
    if (dupcheckbyname(argv[0]) != 0) {
        fprintf(stderr, "oops!!! duplicated process(%s). So, exit\n", argv[0]);
        exit(0);
    }

    /*--------------------------------------------------------------------------
      시그널 등록
      ---------------------------------------------------------------------------*/
    setupSignal(vmct_signal_handler);

    /*--------------------------------------------------------------------------
      Init Environment
      ---------------------------------------------------------------------------*/
    /* get environment variable */
    if ((pEnv = getenv(BPAPP_ENV_NAME)) == NULL){
        fprintf(stderr, "'%s' environment variable not found(%s)\n", BPAPP_ENV_NAME, strerror(errno));
        exit(0);
    }

    /*--------------------------------------------------------------------------
      디버그 프린트 및 로그 초기화
      ---------------------------------------------------------------------------*/
#if 0
    InitDebug(pEnv, FMS_VMC | FMS_CLB, FMS_INFO1, DBG_TERMINAL | DBG_FILE | DBG_THIS);
#else
    InitDebug(pEnv, FMS_VMC | FMS_CLB, FMS_INFO1, DBG_FILE | DBG_THIS);
#endif

    xprint(FMS_VMC | FMS_LOOKS, "\n");
    xprint(FMS_VMC | FMS_LOOKS, "*********************************************************************\n");
    xprint(FMS_VMC | FMS_LOOKS, "     BPAPP-VMCT : START \n");
    xprint(FMS_VMC | FMS_LOOKS, "*********************************************************************\n");
    xprint(FMS_VMC | FMS_LOOKS, "\n");

    /* Init Global Variable */
    if (InitVariable() < 0) {
        xprint(FMS_VMC | FMS_FATAL, "Initialize failed, So exit\n");
        terminateLog();
        close_xprint();
        exit(0);
    }

    /* Init config from cfgFile */
    if (read_config(pEnv) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "Global Configuration error, So exit\n");
        close_xprint();
        exit(0);
    }

    /* Init Mutex & MsgQCmdTmr */
    InitMutexCmdTmr();

#ifdef __DEBUG__
    DebugSizeofStruct();
    DebugVmctInfo();
    DebugVmProcessInfo();
#endif

    /*--------------------------------------------------------------------------
      pthread Create
     ---------------------------------------------------------------------------*/
    /* TCP Recv Control (HOST) */
    if (ThreadFlags[PTASK0] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK0]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK0], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK0], &Thread_Attr[PTASK0], TcpRcvTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK0]);
        ThreadFlags[PTASK0] = TRUE;
    }

    /* System Info HOST Send */
    if (ThreadFlags[PTASK1] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK1]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK1], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK1], &Thread_Attr[PTASK1], TcpSessionTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK1]);
        ThreadFlags[PTASK1] = TRUE;
    }

    /* Timer Cmd (TCP Reconnect) */
    if (ThreadFlags[PTASK2] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK2]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK2], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK2], &Thread_Attr[PTASK2], TmrCmdTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK2]);
        ThreadFlags[PTASK2] = TRUE;
    }

    RunState = 1;

    while(RunState != 0) {
        nanoSleep(1000);
    }

    pthread_join(ThreadArray[PTASK0], NULL);
    pthread_join(ThreadArray[PTASK1], NULL);
    pthread_join(ThreadArray[PTASK2], NULL);

    return(0);
}

