/* -------------------------------------------------------------------------
 * File Name   : host_main.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : HOST(BPAPP HOST Server
 * History     : 14/07/02    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "bpapp.h"

#ifdef __PGSQL__
#include "pqmb.h"
#endif
#include "dbmb.h"

#include "xprint.h"
#include "logdir.h"
#include "kutil.h"

#include "host_main.h"
#include "host_conf.h"
#include "host_sock.h"
#include "host_task.h"

#include "host_debug.h"


void host_shutdown(void);
void host_signal_handler(int signum);
void InitDebug(char *path, unsigned int mask, unsigned char level, unsigned int trace);

int  RunState = 0;
char LogPath[512];

int CHECK_VMCSTATUS_SEC = 5;    /* sec */
int CHECK_TCPSTATUS_MIN = 3;    /* min */
int CHECK_WATCHDOG_MSEC = 2000; /* msec */

pthread_mutex_t TcpMutex;
pthread_mutex_t BypassMutex;
pthread_mutex_t StateMutex;

int            ThreadFlags[MAX_PTASK];
int            ThreadAlive[MAX_PTASK];
pthread_t      ThreadArray[MAX_PTASK];
pthread_attr_t Thread_Attr[MAX_PTASK];

void host_shutdown(void)
{
    int i;
    static unsigned char once = 0;

    if (once) {
        return;
    } else {
        once = 1;
    }

    /* socket fd close */
    if (tlink.mode == LINK_MODE_SERVER) {
        for (i = 0; i < MAX_LISTENQ; i++) {
            if (alist[i].AcceptFD > 0) {
                close(alist[i].AcceptFD);
                FD_CLR(alist[i].AcceptFD, &allset);
                alist[i].AcceptFD = -1;
            }
        }
        if (tlink.ListenFD > 0) {
            close(tlink.ListenFD);
            FD_CLR(tlink.ListenFD, &allset);
            tlink.ListenFD = -1;
        }
    } else if (tlink.mode == LINK_MODE_CLIENT) {
        if (tlink.ListenFD > 0) {
            close(tlink.ListenFD);
            FD_CLR(tlink.ListenFD, &allset);
            tlink.ListenFD = -1;
        }
    }

    for (i = 0; i < MAX_PTASK; i++) {
        if (ThreadFlags[i] != TRUE) {
            continue;
        }
        ThreadAlive[i] = FALSE;

        pthread_cancel(ThreadArray[i]);
        nanoSleep(10);
    }

    pthread_mutex_destroy(&TcpMutex);
    pthread_mutex_destroy(&BypassMutex);
    pthread_mutex_destroy(&StateMutex);

#ifdef __PGSQL__
    PQ_CloseDatabase();
#else
    OrcheCloseDatabase();
#endif

    if (hostConf.cloud_mode == 1) {
        CloseDatabase();
    }

    FreeVariable();

    terminateLog();
    close_xprint();

    exit(0);
}

/* Signal handler */
void host_signal_handler(int signum)
{
    xprint(FMS_HST | FMS_LOOKS, "Received Signal(%d)\n", signum);

    host_shutdown();

    RunState = 0;
}

void InitDebug(char *path, unsigned int mask, unsigned char level, unsigned int trace)
{
    unsigned int  DbgMask  = mask;
    unsigned int  DbgTrace = trace;
    unsigned char DbgLevel = level;

    if (init_xprint(DbgMask, DbgLevel, DbgTrace) < 0) {
        fprintf(stderr, "init_xprint error, So exit\n");
        exit(0);
    }

    /* 로그 디렉토리 및 로그파일의 초기화 */
    memset(&LogPath, 0x00, sizeof(LogPath));
    sprintf(LogPath, "%s/%s", path, "log/HOST");

    if (initializeLog(LogPath) < 0) {
        xprint(FMS_HST | FMS_FATAL, "initializeLog error, So exit\n");
        close_xprint();
        exit(0);
    }
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
    setupSignal(host_signal_handler);

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
    InitDebug(pEnv, FMS_HST | FMS_CLB | FMS_DBM | FMS_PQM, FMS_INFO1, DBG_TERMINAL | DBG_FILE | DBG_THIS);
#else
    InitDebug(pEnv, FMS_HST | FMS_CLB | FMS_DBM, FMS_INFO1, DBG_FILE | DBG_THIS);
#endif

    xprint(FMS_HST | FMS_LOOKS, "\n");
    xprint(FMS_HST | FMS_LOOKS, "=========================================================\n");
    xprint(FMS_HST | FMS_LOOKS, " BPAPP-Host : START\n");
    xprint(FMS_HST | FMS_LOOKS, "=========================================================\n");
    xprint(FMS_HST | FMS_LOOKS, "\n");

    /*--------------------------------------------------------------------------
      Read configration & Init Global Variable
      ---------------------------------------------------------------------------*/
    if (InitVariable(pEnv) < 0) {
        xprint(FMS_HST | FMS_FATAL, "Initialize failed, So exit\n");
        terminateLog();
        close_xprint();
        exit(0);
    }

#if 1
#ifdef __DEBUG__
    DebugSizeofStruct();
#endif
#endif
    DebugHostInfo();

    pthread_mutex_init(&TcpMutex, NULL);
    pthread_mutex_init(&BypassMutex, NULL);
    pthread_mutex_init(&StateMutex, NULL);

    /*--------------------------------------------------------------------------
      pthread Mutex & Create
      ---------------------------------------------------------------------------*/
    if (ThreadFlags[PTASK0] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK0]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK0], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK0], &Thread_Attr[PTASK0], tcp_rcvTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK0]);
        ThreadFlags[PTASK0] = TRUE;
    }

    if (ThreadFlags[PTASK1] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK1]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK1], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK1], &Thread_Attr[PTASK1], fifo_getTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK1]);
        ThreadFlags[PTASK1] = TRUE;
    }

    if (ThreadFlags[PTASK2] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK2]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK2], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK2], &Thread_Attr[PTASK2], CheckVmStatusTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK2]);
        ThreadFlags[PTASK2] = TRUE;
    }

    if (ThreadFlags[PTASK3] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK3]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK3], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK3], &Thread_Attr[PTASK3], CheckTcpStatusTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK3]);
        ThreadFlags[PTASK3] = TRUE;
    }

    if (ThreadFlags[PTASK4] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK4]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK4], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK4], &Thread_Attr[PTASK4], ResetWatchdogTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK4]);
        ThreadFlags[PTASK4] = TRUE;
    }

    if (ThreadFlags[PTASK5] == FALSE) {
        pthread_attr_init(&Thread_Attr[PTASK5]);
        pthread_attr_setdetachstate(&Thread_Attr[PTASK5], PTHREAD_CREATE_DETACHED);
        pthread_create(&ThreadArray[PTASK5], &Thread_Attr[PTASK5], ProcessVmStateTask, NULL);
        pthread_attr_destroy(&Thread_Attr[PTASK5]);
        ThreadFlags[PTASK5] = TRUE;
    }


    RunState = 1;

    while(RunState != 0) {

#if 0
        system_get_usageMYL(60);
#else
        nanoSleep(1000);
#endif
    }

    return(0);
}
