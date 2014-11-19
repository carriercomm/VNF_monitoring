/* -------------------------------------------------------------------------
 * File Name   : host_task.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2013 by KBell Inc.
 * Description : CFM(Configuration Management)
 * History     : 13/07/02    Initial Coded.
 * -------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/time.h>

#include "bpapp.h"

#ifdef __PGSQL__
#include "pqmb.h"
#endif
#include "dbmb.h"

#include "xprint.h"
#include "kutil.h"

#include "host_main.h"
#include "host_conf.h"
#include "host_sock.h"
#include "host_fifo.h"
#if 0
#include "host_msg.h"
#endif
#include "host_task.h"
#include "host_debug.h"


static void ThreadCleanUp(void *arg);
static void TcpdSock_Event(int alist_idx, unsigned char *buf);
static void FifoGet_Event(block_t *fifomsg);
static int CheckVmcStatus(void);
static int CheckTcpStatus(void);
#if 0
static int CheckServerSystemStatus(void);
static int CheckLineSystemStatus(int year, int month, int day);
#endif

static int SendVmProcStateRsp(int alist_idx, unsigned int systemid);
static int SetVmProcessStateInfo(vm_proc_state_msg *s);
static int ProcessVmProcStateInfo(char *ttime);

static void ThreadCleanUp(void *arg)
{
    int i, task;
    pthread_t mytid = pthread_self();

    for (task = 0; task < MAX_PTASK; task++) {
        if (ThreadFlags[task] != TRUE) {
            continue;
        }

        if (mytid == ThreadArray[task]) {
            ThreadFlags[task] = FALSE;
            ThreadAlive[task] = FALSE;
            break;
        }
    }

    if (task == MAX_PTASK) {
        return;
    }

    if (task == PTASK0) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK0 (TCP Rcv Task)\n");

        /* socket fd close */ /* 모든 fd를 clear한다. */
        pthread_mutex_lock(&TcpMutex);
        if (tlink.mode == LINK_MODE_SERVER) {
            for (i = 0; i < MAX_LISTENQ; i++) {
                if (alist[i].AcceptFD > 0) {
                    FD_CLR(alist[i].AcceptFD, &allset);
                    close(alist[i].AcceptFD);
                    alist[i].sysinfo.port = 0;
                    alist[i].AcceptFD = -1;
                }
            }

            if (tlink.ListenFD > 0) {
                close(tlink.ListenFD);
                FD_CLR(tlink.ListenFD, &allset);
                tlink.ListenFD = -1;
            }
        } else if (tlink.mode == LINK_MODE_CLIENT) {
        }
        pthread_mutex_unlock(&TcpMutex);

    } else if (task == PTASK1) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK1 (TcpFifo Get Task)\n");

    } else if (task == PTASK2) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK2 (Check VmStatus Task)\n");

    } else if (task == PTASK3) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK3 (Check TcpStatus Task)\n");

    } else if (task == PTASK4) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK4 (Reset WatchDog Task)\n");

    } else if (task == PTASK5) {
        xprint(FMS_HST | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK4 (Process VM State Task)\n");

    }

    pthread_mutex_unlock(&TcpMutex);
}

void *tcp_rcvTask(void *argc)
{
    int  ret, i;
    int  numEvents;
    unsigned char buf[MAX_DATA_LENGTH*1000]; /* NEXT_CHECK by icecom 110823 */

    ThreadAlive[PTASK0] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    memset(buf, 0x0, sizeof(buf));

    FD_ZERO(&allset);

    tlink.ListenFD = TcpOpen(&tlink);

    if (tlink.ListenFD <= 0) {
        if (tlink.mode == LINK_MODE_SERVER) {
            xprint(FMS_HST | FMS_ERROR, "%s : S-TCP TcpOpen failed\n", __func__);
        } else if (tlink.mode == LINK_MODE_CLIENT) {
            xprint(FMS_HST | FMS_ERROR, "%s : client mode\n", __func__);
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s : unknown socket mode\n", __func__);
        }
        /* NEXT_CHECK error processing, by icecom */
    } else {
        FD_SET(tlink.ListenFD, &allset);
        compareMaxfd(tlink.ListenFD);
        if (tlink.mode == LINK_MODE_CLIENT) {
        } else {
            TcpInfo();
        }
    }

    for (;;) {
        if (ThreadAlive[PTASK0] != TRUE) {
            break;
        }

        rset = allset;
        if (0 > (numEvents = recv_extendedPoll(200, &rset))) {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : recv_extendedPoll (reason=%s)\n",
                    __func__, __LINE__, strerror(errno));
            if (errno == EINTR) {
                continue;
            } else if (errno == EBADF || errno == EPIPE || errno == EIO) {
                xprint(FMS_HST | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            } else {
                xprint(FMS_HST | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            }
        } else if (numEvents == 0) {
            /* Time out 인 경우 정상처리 한다. */
            continue;
        } else {
            if (tlink.mode == LINK_MODE_SERVER) {
                if (tlink.ListenFD > 0) {
                    if (FD_ISSET(tlink.ListenFD, &rset)) {
                        xprint(FMS_HST | FMS_INFO1, "%s(%d) :: TcpAccept Start\n", __func__, __LINE__);

                        pthread_mutex_lock(&TcpMutex);
                        i = TcpAccept(tlink.ListenFD);
                        pthread_mutex_unlock(&TcpMutex);

                        if (i == MAX_LISTENQ) {
                            xprint(FMS_HST | FMS_ERROR, "%s(%d) :: TcpAccept error\n", __func__, __LINE__);
                            continue;
                        } else {
                            pthread_mutex_lock(&TcpMutex);
                            if (alist[i].AcceptFD > 0) {
                                FD_SET(alist[i].AcceptFD, &allset);
                                compareMaxfd(alist[i].AcceptFD);
#if 0
                                /* 20140723, by jeon */
                                TcpInfo();
#endif
                            } else {
                                xprint(FMS_HST | FMS_ERROR, "TcpAccept fail(fd=%d)\n", alist[i].AcceptFD);
                                pthread_mutex_unlock(&TcpMutex);
                                continue;
                            }
                            pthread_mutex_unlock(&TcpMutex);

                        }
                    }
                }

                pthread_mutex_lock(&TcpMutex);
                for (i = 0; i < MAX_LISTENQ; i++) {
                    if (alist[i].AcceptFD > 0) {
                        if (FD_ISSET(alist[i].AcceptFD, &rset)) {
                            if ((ret = TcpRecv(i, alist[i].AcceptFD, buf)) > 0) {

                                /* 20140723, by jeon */
                                if (hostConf.vmc_system[i].systemport == 0) {
                                    vm_proc_state_msg *hb  = (vm_proc_state_msg *)&buf[DEFAULT_TCP_COMHDR_LENGTH];

                                    alist[i].sysinfo.systemaddr = NBO_NTOHL(hb->vm_ipaddress);
                                    sprintf(alist[i].sysinfo.systemname, "VM-%s", addr2ntoa(alist[i].sysinfo.systemaddr));

                                    hostConf.vmc_system[i].systemaddr = alist[i].sysinfo.systemaddr;
                                    sprintf(hostConf.vmc_system[i].systemname, "%s", alist[i].sysinfo.systemname);

                                    hostConf.vmc_system[i].systemport = alist[i].sysinfo.port;

                                    TcpInfo();
                                }

                                TcpdSock_Event(i, buf);
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&TcpMutex);
            } else if (tlink.mode == LINK_MODE_CLIENT) {
            }
        }
    }

    ThreadAlive[PTASK0] = FALSE;
    ThreadFlags[PTASK0] = FALSE;

    /* pthread mgnt */
    pthread_cleanup_pop(1);

    return(NULL);
}

void *fifo_getTask(void *argc)
{
    block_t *p_block;
    int sleep_flag;

    ThreadAlive[PTASK1] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    sleep_flag = 0;

    for (;;) {
        if (ThreadAlive[PTASK1] != TRUE) {
            break;
        }

        /* Check DB handler1, 10s */
        sleep_flag++;
        if (sleep_flag > 50000) {
#if 0
            CheckTimeHLR1();
#endif
            sleep_flag = 0;
        }

        if ((p_block = block_FifoGet(TcpFifo)) == NULL) {
            continue;
        }

        FifoGet_Event(p_block);

        block_Release(p_block);
    }

    ThreadAlive[PTASK1] = FALSE;
    ThreadFlags[PTASK1] = FALSE;

    pthread_cleanup_pop(1);

    return(NULL);
}

void *CheckVmStatusTask(void *argc)
{
    int FiveSecCheck = FALSE;
    struct tm ctime;
    struct timeval tv;

    ThreadAlive[PTASK2] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    for (;;) {
        if (ThreadAlive[PTASK2] != TRUE) {
            break;
        }

        nanoSleep(100);

        gettimeofday(&tv, NULL);
        localtime_r((time_t *)&(tv.tv_sec), &ctime);

        /* check  5sec */
        if ((ctime.tm_sec % CHECK_VMCSTATUS_SEC) == 0) {
            if (FiveSecCheck == TRUE) {
                continue;
            }

            xprint(FMS_HST | FMS_INFO1, "Check VM Status %dsec (Bypass Mode : %s [vms(%s) / tcp(%s)]) )\n",
                            CHECK_VMCSTATUS_SEC, (hostConf.bp_mode == BYPASS_MODE_ON) ? "ON" : "OFF",
                            (hostConf.vmc_mode == BYPASS_MODE_ON) ? "ON" : "OFF", 
                            (hostConf.tcp_mode == BYPASS_MODE_ON) ? "ON" : "OFF");

            CheckVmcStatus();

            FiveSecCheck = TRUE;

        } else {
            FiveSecCheck = FALSE;
        }
    }

    ThreadAlive[PTASK2] = FALSE;
    ThreadFlags[PTASK2] = FALSE;

    pthread_cleanup_pop(1);

    return(NULL);
}

void *CheckTcpStatusTask(void *argc)
{
    int check1min;
    struct timeval tv;
    struct tm ctime;

    ThreadAlive[PTASK3] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    check1min = FALSE;

    /* tmphandler */
    for (;;) {
        if (ThreadAlive[PTASK3] != TRUE) {
            break;
        }

        gettimeofday(&tv, NULL);
        localtime_r((time_t *)&(tv.tv_sec), &ctime);

        /* every minute */
        if (ctime.tm_sec == 15) {
            if (check1min != TRUE) {
                xprint(FMS_HST | FMS_INFO1, "%s : Check TCP Status START\n", __func__);

                /* TCP status */
                CheckTcpStatus();

                check1min = TRUE;
                xprint(FMS_HST | FMS_INFO1, "%s : Check TCP Status End\n", __func__);
             }

        } else {
            check1min = FALSE;
        }

        nanoSleep(100);
    }

    ThreadAlive[PTASK3] = FALSE;
    ThreadFlags[PTASK3] = FALSE;

    pthread_cleanup_pop(1);

    return(NULL);
}

void *ResetWatchdogTask(void *argc)
{
    struct timeval tv;
    struct tm ctime;
    int sectime, check1sec = FALSE;

    ThreadAlive[PTASK4] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    gettimeofday(&tv, NULL);
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    sectime = ctime.tm_sec;

    for (;;) {
        if (ThreadAlive[PTASK4] != TRUE) {
            break;
        }

        gettimeofday(&tv, NULL);
        localtime_r((time_t *)&(tv.tv_sec), &ctime);

        /* check for every sec */
        if (sectime != ctime.tm_sec) {
            if (check1sec != TRUE && CHECK_WATCHDOG_MSEC != 0) {
                xprint(FMS_HST | FMS_INFO5, "%s :: Rest Bypass WatchDog Start\n", __func__);
                /* re-set bypass watchdog */
                ResetBypassWatchdog(); 

                sectime = ctime.tm_sec;
                check1sec = TRUE;
                xprint(FMS_HST | FMS_INFO5, "%s :: Rest Bypass WatchDog End\n", __func__);
            }
        } else {
            check1sec = FALSE;
        }

        nanoSleep(100);


    }

    ThreadAlive[PTASK4] = FALSE;
    ThreadFlags[PTASK4] = FALSE;

    pthread_cleanup_pop(1);

    return(NULL);
}

void *ProcessVmStateTask(void *argc)
{
    struct timeval tv;
    struct tm ctime;
    int check5sec = FALSE;
    char ttime[MAX_TIME_LENGTH];
    int sleep_flag;

    ThreadAlive[PTASK5] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    sleep_flag = 0;

    gettimeofday(&tv, NULL);
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    for (;;) {
        if (ThreadAlive[PTASK5] != TRUE) {
            break;
        }

        /* Check DB orchehandler, 10s */
        sleep_flag++;
        if (sleep_flag > 50000) {
#if 0
            CheckTimeOrcheHLR();
#endif
            sleep_flag = 0;
        }

        gettimeofday(&tv, NULL);
        localtime_r((time_t *)&(tv.tv_sec), &ctime);

        /* check 5sec */
        if ((ctime.tm_sec % 5) == 2) {
            if (check5sec == TRUE) {
                continue;
            }

            xprint(FMS_VMC | FMS_LOOKS, "%s :: Now Time[= %01d:%02d:%02d]\n", __func__,
                                        ctime.tm_hour, ctime.tm_min, ctime.tm_sec);

            memset(&ttime, 0x00, sizeof(ttime));
            sprintf(ttime, "%04d-%02d-%02d, %02d:%02d:%02d",
                   ctime.tm_year + 1900, ctime.tm_mon + 1, ctime.tm_mday,
                   ctime.tm_hour, ctime.tm_min, ctime.tm_sec-2);

            ProcessVmProcStateInfo(ttime);

            check5sec = TRUE;

        } else {
            check5sec = FALSE;
        }

        nanoSleep(100);
    }

    ThreadAlive[PTASK5] = FALSE;
    ThreadFlags[PTASK5] = FALSE;

    pthread_cleanup_pop(1);

    return(NULL);
}


int SetBypassWatchdog(void)
{
    int ret = 0;
    char cmd[MAX_STR_LENGTH];

    memset(cmd, 0x00, sizeof(cmd));
    sprintf(cmd, CTRL_BYPASS_WATCHDOG_SET, hostConf.EnvPath, CHECK_WATCHDOG_MSEC);

#if 0
#ifdef __BPCTL__
    ret = system(cmd);
#endif
#endif

    if (ret < 0) {
        xprint(FMS_HST | FMS_ERROR, "Failed Set Bypass WatchDog Mode = [ %s ]\n", cmd);
    }

    return ret;
}

int ResetBypassWatchdog(void)
{
    int ret = 0;
    char cmd[MAX_STR_LENGTH];

    memset(cmd, 0x00, sizeof(cmd));
    sprintf(cmd, CTRL_BYPASS_WATCHDOG_RESET, hostConf.EnvPath);

#if 0
#ifdef __BPCTL__
    ret = system(cmd);
#endif
#endif

    if (ret < 0) {
        xprint(FMS_HST | FMS_ERROR, "Failed Re-Set Bypass WatchDog Mode = [ %s ]\n", cmd);
    }

    return ret;
}

static void TcpdSock_Event(int alist_idx, unsigned char *buf)
{
#if 0
    int retval;
    common_tcpmsg_t *mpmsg = (common_tcpmsg_t *)buf;
#if 0
    xdump(FMS_HST | FMS_LOOKS, (unsigned char *)mpmsg, 20, "1111 TcpRecv ");
    xdump(FMS_HST | FMS_LOOKS, (unsigned char *)buf, NBO_NTOHS(mpmsg->MsgSize) + DEFAULT_TCP_COMHDR_LENGTH, "GUI RECV-1");
#endif

    TCP_MSG_NTOH(mpmsg);

    if (((mpmsg->MsgType > GUI_COMMAND_BASE) && (mpmsg->MsgType < GUI_REPORT_BASE)) ||
        (mpmsg->MsgType == SYSTEM_INFO_REQ) ||
        (mpmsg->MsgType == CCS_REGISTER_REQ) ||
        (mpmsg->MsgType == DSG_REGISTER_REQ) ||
        (mpmsg->MsgType == SDM_REGISTER_REQ) ||
        (mpmsg->MsgType == CTS_REGISTER_REQ) ||
        (mpmsg->MsgType == DBS_REGISTER_REQ) ||
        (mpmsg->MsgType == SFM_REGISTER_REQ)) {

        retval = tcp_PutFifo(FIFO_TYPE_REGCTRL, mpmsg, alist_idx, (unsigned char *)(buf + DEFAULT_TCP_COMHDR_LENGTH));

    } else {
        retval = tcp_PutFifo(FIFO_TYPE_ACTION, mpmsg, alist_idx, (unsigned char *)(buf + DEFAULT_TCP_COMHDR_LENGTH));
    }

    if (retval < 0) {
        xprint(FMS_HST | FMS_ERROR, "%s(%d) : tcp_PutFifo failed.\n", __func__, __LINE__);
    }
#else
    int retval;
    common_tcpmsg_t *mpmsg = (common_tcpmsg_t *)buf;

    TCP_MSG_NTOH(mpmsg);

    retval = tcp_PutFifo(mpmsg, alist_idx, (unsigned char *)(buf + DEFAULT_TCP_COMHDR_LENGTH));
    if (retval < 0) {
        xprint(FMS_HST | FMS_ERROR, "%s(%d) : tcp_PutFifo failed.\n", __func__, __LINE__);
    }
#endif

    return;
}

static void FifoGet_Event(block_t *fifomsg)
{
    /* vm proce state msg */
    if (fifomsg->msgtype == VM_PROC_STATE_REQ) {
        vm_proc_state_msg *pstate  = (vm_proc_state_msg *)fifomsg->message;
        xprint(FMS_HST | FMS_INFO1, "|<--------| VM_PROC_STATE_REQ msg recv. SystemID(%02u : %s / %s)\n", 
                fifomsg->systemid, addr2ntoa(NBO_NTOHL(pstate->vm_ipaddress)), alist[fifomsg->alist_idx].sysinfo.systemname);

#ifdef __DEBUG__
        DebugVmProcessState(pstate);
#endif

        SendVmProcStateRsp(fifomsg->alist_idx, fifomsg->systemid);

        /* 20140811, by jeon */
        SetVmProcessStateInfo(pstate);

    } else {
        xprint(FMS_HST | FMS_WARNING, "|<--------| recv Unknown msgtype = %x, SystemID(%u / %s)\n",
                fifomsg->msgtype, fifomsg->systemid, alist[fifomsg->alist_idx].sysinfo.systemname);
    }

    return;
}

static int CheckVmcStatus(void)
{
    int i, j, ret = 0, vmcount = 0;
    int s_flag = FALSE, bpmode = BYPASS_MODE_OFF;
    char cmd[MAX_STR_LENGTH];
    vmc_status_t vm[MAX_SYSTEM_NUM];

    memset(&vm[0], 0x00, sizeof(vmc_status_t) * MAX_SYSTEM_NUM);

    if (hostConf.cloud_mode == 1) {
        vmcount = SelectAllVMStatus(&vm[0]);
    }

    if (vmcount < 0) {
        xprint(FMS_HST | FMS_FATAL, "SelectAllVMStatus Error\n");
        return -1;
    }

    /* check to change vm status */
    if (vmcount != hostConf.vm_count) {
        s_flag = TRUE;

    } else {
        /* compare to vm status (old & new) */
        for (i = 0; i < vmcount; i++) {
            for (j = 0; j < hostConf.vm_count; j++) {
                if (vm[i].vm_id == hostConf.vmc_status[j].vm_id) {
                    if (vm[i].vm_status != hostConf.vmc_status[j].vm_status ||
                        vm[i].vm_power_status != hostConf.vmc_status[j].vm_power_status) {
                        s_flag = TRUE;
                    }
                    break;
                }
            }

            /* not found --> add new vm */
            if (j == hostConf.vm_count) {
                s_flag = TRUE;
            }

            if (s_flag == TRUE) {
                break;
            }

        }
    }

    /* check vm status for bypass mode ctrl */
    if (vmcount == 0) {
        bpmode = BYPASS_MODE_ON;
    } else {
        for (i = 0; i < vmcount; i++) {
            if ((vm[i].vm_status != STATUS_ACTIVE && vm[i].vm_status != STATUS_BUILD) ||
                (vm[i].vm_power_status != PW_STATUS_RUNNING && vm[i].vm_power_status != PW_STATUS_NOSTATUS)) {
                /* 20140814, by jeon */
                if (vm[i].vm_power_status != PW_STATUS_SHUTDOWN) {
                    xprint(FMS_HST | FMS_INFO3, "vm_id = %u :: Bypass Mode (ON)\n", vm[i].vm_id);
                    bpmode = BYPASS_MODE_ON;
                }
            }
        }
    }

    /* memcpy vm status */
    pthread_mutex_lock(&BypassMutex);
    memcpy(&hostConf.vmc_status[0], &vm[0], sizeof(vmc_status_t) * MAX_SYSTEM_NUM);
    hostConf.vm_count = vmcount;

    /* change vm status */
    if (s_flag == TRUE) {
#ifdef __DEBUG__
        DebugVmStatus();
#endif
    }

    xprint(FMS_HST | FMS_INFO3, "BP MDOE : %d / %d\n", bpmode, hostConf.bp_mode);

    /* Change Bypass Mode */
    if (bpmode != hostConf.bp_mode) {
        if (bpmode == BYPASS_MODE_ON) {
            /* Bypass Mode ON */
            memset(cmd, 0x00, sizeof(cmd));
            sprintf(cmd, CTRL_BYPASS_MODE_ON, hostConf.EnvPath);

#if 0
#ifdef __BPCTL__
            ret = system(cmd);
#endif
#endif
            if (ret < 0) {
                xprint(FMS_HST | FMS_ERROR, "Failed Set Bypass Mode [ ON ] = [ %s ]\n", cmd);
            }

            xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ ON ] = [ %s ]\n", cmd);

            hostConf.bp_mode  = bpmode;

        } else {
            if (hostConf.tcp_mode == BYPASS_MODE_OFF) {
                /* Bypass Mode OFF */
                memset(cmd, 0x00, sizeof(cmd));
                sprintf(cmd, CTRL_BYPASS_MODE_OFF, hostConf.EnvPath);

#if 0
#ifdef __BPCTL__
                ret = system(cmd);
#endif
#endif
                if (ret < 0) {
                    xprint(FMS_HST | FMS_ERROR, "Failed Set Bypass Mode [ OFF ] = [ %s ]\n", cmd);
                }

                xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ OFF ] = [ %s ]\n", cmd);

                hostConf.bp_mode  = bpmode;

#if 0
                /* Set Bypass WatchDog */
                SetBypassWatchdog();
#endif
            }
        }
    }

    hostConf.vmc_mode = bpmode;
    pthread_mutex_unlock(&BypassMutex);

    return 0;
}

static int CheckTcpStatus(void)
{
    int i, j, vmcount, ret = 0, bpmode = BYPASS_MODE_OFF;
    int v;
#if 0
    int found;
#endif
    char cmd[MAX_STR_LENGTH];
    vmc_status_t vstatus[MAX_SYSTEM_NUM];

    pthread_mutex_lock(&BypassMutex);
    vmcount = hostConf.vm_count;
    memcpy(&vstatus[0], &hostConf.vmc_status[0], sizeof(vmc_status_t) * MAX_SYSTEM_NUM);
    pthread_mutex_unlock(&BypassMutex);

    /* check tcp status for vmc */
    for (i = 0; i < vmcount; i++) {
#if 0
        found = 0;
#endif

        for (j = 0; j < MAX_SYSTEM_NUM; j++) {
            if (hostConf.vmc_system[j].systemaddr == 0) {
                continue;
            }

            if (vstatus[i].vm_ipaddr == hostConf.vmc_system[j].systemaddr) {
                if (hostConf.vmc_system[j].link_status < CHECK_TCPSTATUS_MIN) {
                    hostConf.vmc_system[j].link_status ++;
                } else {
                    bpmode = BYPASS_MODE_ON;

                    /* 20140818, by jeon */
                    for (v = 0; v < MAX_SYSTEM_NUM; v++) {
                        if (stateinfo[v].vm_ip == vstatus[i].vm_ipaddr) {
                            xprint(FMS_HST | FMS_WARNING, "VM is Hang [= %s]\n", addr2ntoa(stateinfo[v].vm_ip));
                            pthread_mutex_lock(&StateMutex);
                            memset(&stateinfo[i], 0x00, sizeof(vmc_stateinfo_t));
                            memcpy(&stateinfo[i].pstate[i], &basePState[i], sizeof(process_state_t) * MAX_VM_PROC_NUM);
                            pthread_mutex_unlock(&StateMutex);
                            break;
                        }
                    }
                }
#if 0
                found = 1;
#endif
                break;
            }
        }

#if 0
        /* Not Found TCP Session */
        if (found == 0) {
            bpmode = BYPASS_MODE_ON;
        }
#endif

    }

    /* Control Bypass Mode */
    pthread_mutex_lock(&BypassMutex);
    if (bpmode != hostConf.bp_mode) {
        if (bpmode == BYPASS_MODE_ON) {
            /* Bypass Mode ON */
            memset(cmd, 0x00, sizeof(cmd));
            sprintf(cmd, CTRL_BYPASS_MODE_ON, hostConf.EnvPath);

#if 0
#ifdef __BPCTL__
            ret = system(cmd);
#endif
#endif
            if (ret < 0) {
                xprint(FMS_HST | FMS_ERROR, "Failed Set Bypass Mode [ ON ] = [ %s ]\n", cmd);
            }

            xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ ON ] = [ %s ]\n", cmd);

            hostConf.bp_mode = bpmode;
            TcpInfo();

        } else {
            if (hostConf.vmc_mode == BYPASS_MODE_OFF) {
                /* Bypass Mode OFF */
                memset(cmd, 0x00, sizeof(cmd));
                sprintf(cmd, CTRL_BYPASS_MODE_OFF, hostConf.EnvPath);

#if 0
#ifdef __BPCTL__
                ret = system(cmd);
#endif
#endif
                if (ret < 0) {
                    xprint(FMS_HST | FMS_ERROR, "Failed Set Bypass Mode [ OFF ] = [ %s ]\n", cmd);
                }

                xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ OFF ] = [ %s ]\n", cmd);

                hostConf.bp_mode = bpmode;
                TcpInfo();

                /* Set Bypass WatchDog */
                SetBypassWatchdog();
            }
        }

    }

    hostConf.tcp_mode = bpmode;
    pthread_mutex_unlock(&BypassMutex);

    return 0;
}

static int SendVmProcStateRsp(int alist_idx, unsigned int systemid)
{
    int             i, TcpMsgLength;
    unsigned int    system_addr;
    common_tcpmsg_t sndmsg;

    system_addr = alist[alist_idx].sysinfo.systemaddr;
    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        if (hostConf.vmc_system[i].systemaddr == system_addr) {
            hostConf.vmc_system[i].is_check = IS_CHECK_ON;
            hostConf.vmc_system[i].link_status = 0;
            break;
        }
    }
    if (i == MAX_SYSTEM_NUM) {
        xprint(FMS_HST | FMS_ERROR, "%s(%d) : System is Unknown.(= %s)\n", __func__, __LINE__, addr2ntoa(system_addr));
        return -1;
    }

    sndmsg.TpktHdr  = 0x00;
    sndmsg.SystemID = systemid;
    sndmsg.MsgType  = VM_PROC_STATE_RSP;
    sndmsg.MsgSize  = 0;

    TcpMsgLength = DEFAULT_TCP_COMHDR_LENGTH + sndmsg.MsgSize;
    TCP_MSG_HTON((common_tcpmsg_t *)&sndmsg);

    i = alist_idx;
    pthread_mutex_lock(&TcpMutex);
    if (alist[i].AcceptFD < 0) {
        xprint(FMS_HST | FMS_ERROR, "%s(%d) : recv fd lost.\n", __func__, __LINE__);
    } else {
        if (TcpSend(i, alist[i].AcceptFD, (unsigned char *)&sndmsg, TcpMsgLength) == TcpMsgLength) {
            xprint(FMS_HST | FMS_INFO3,
                    "|-------->| VM_PROC_STATE_RSP msg send(len:%d) OK. systemid(%d : %s)\n",
                    TcpMsgLength, systemid, alist[i].sysinfo.systemname);
        } else {
            xprint(FMS_HST | FMS_ERROR,
                    "|---XXX-->| VM_PROC_STATE_RSP msg send Fail. systemid(%d : %s)\n", systemid, alist[i].sysinfo.systemname);
        }
    }
    pthread_mutex_unlock(&TcpMutex);

    return i;
}

static int SetVmProcessStateInfo(vm_proc_state_msg *s)
{
    int i, f_flag = 0;
    unsigned int vm_ip, pcount, vm_type;
    vmc_stateinfo_t vmstate[MAX_SYSTEM_NUM];

    pthread_mutex_lock(&StateMutex);
    memcpy(&vmstate[0], &stateinfo[0], sizeof(vmc_stateinfo_t) * MAX_SYSTEM_NUM);
    pthread_mutex_unlock(&StateMutex);

    vm_ip   = NBO_NTOHL(s->vm_ipaddress);
    vm_type = NBO_NTOHL(s->vm_type);
    pcount  = NBO_NTOHL(s->pcount);
    for (i = 0; i < MAX_VM_PROC_NUM; i++) {
        s->pstate[i].process_state = NBO_NTOHL(s->pstate[i].process_state);
    }

    /* search vm */
    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        if (vmstate[i].vm_ip == vm_ip) {
            vmstate[i].vm_type = vm_type;
            vmstate[i].pcount  = pcount;
            memcpy(&vmstate[i].pstate[0], &s->pstate[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);

            /*20140926, by jeon */
            strcpy((char *)vmstate[i].vm_id, (char *)s->vm_id);
            strcpy((char *)vmstate[i].vm_name, (char *)s->vm_name);
            strcpy((char *)vmstate[i].manage_ip, (char *)s->manage_ip);

            f_flag = 1;
            break;
        }
    }

    /* set vmstate (for not search) */
    if (f_flag != 1) {
        for (i = 0; i < MAX_SYSTEM_NUM; i++) {
            if (vmstate[i].vm_ip == 0) {
                vmstate[i].vm_ip   = vm_ip;
                vmstate[i].vm_type = vm_type;
                vmstate[i].pcount  = pcount;
                memcpy(&vmstate[i].pstate[0], &s->pstate[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);

                /*20140926, by jeon */
                strcpy((char *)vmstate[i].vm_id, (char *)s->vm_id);
                strcpy((char *)vmstate[i].vm_name, (char *)s->vm_name);
                strcpy((char *)vmstate[i].manage_ip, (char *)s->manage_ip);

                break;
            }
        }
    }

    if (i == MAX_SYSTEM_NUM) {
        xprint(FMS_HST | FMS_INFO1, "Not Found VM (%s)\n", addr2ntoa(vm_ip));
        return -1;
    }  

    pthread_mutex_lock(&StateMutex);
    memcpy(&stateinfo[0], &vmstate[0], sizeof(vmc_stateinfo_t) * MAX_SYSTEM_NUM);
    pthread_mutex_unlock(&StateMutex);

    return i;
}

static int ProcessVmProcStateInfo(char *ttime)
{
    int i, j, vmcount, v_flag = 0, pq_retval;
    vmc_status_t vstatus[MAX_SYSTEM_NUM];
    vmc_stateinfo_t vminfo[MAX_SYSTEM_NUM];
    vm_process_t vmstate;

    pthread_mutex_lock(&BypassMutex);
    vmcount = hostConf.vm_count;
    memcpy(&vstatus[0], &hostConf.vmc_status[0], sizeof(vmc_status_t) * MAX_SYSTEM_NUM);
    pthread_mutex_unlock(&BypassMutex);

    pthread_mutex_lock(&StateMutex);
    memcpy(&vminfo[0], &stateinfo[0], sizeof(vmc_stateinfo_t) * MAX_SYSTEM_NUM);
    pthread_mutex_unlock(&StateMutex);

    DebugVmcStateInfo();

    /* Insert VM State */
    for (i = 0; i < vmcount; i++) {
        /* set vmstate */
        memset(&vmstate, 0x00, sizeof(vm_process_t));

        strcpy((char *)vmstate.TimeStamp, ttime);
        strcpy((char *)vmstate.vm_id, vstatus[i].vm_uuid);
        strcpy((char *)vmstate.vm_name, vstatus[i].vm_name);
        strcpy((char *)vmstate.host_id, vstatus[i].vm_host_name);
        strcpy((char *)vmstate.host_ip, "-");

        vmstate.vm_type          = VM_TYPE_UTM;
        vmstate.vm_state         = vstatus[i].vm_status;
        vmstate.vm_power_state   = vstatus[i].vm_power_status;

        for (j = 0; j < MAX_SYSTEM_NUM; j++) {
            if (vstatus[i].vm_ipaddr == vminfo[j].vm_ip) {
                vmstate.vm_process_count = vminfo[j].pcount;
                memcpy(&vmstate.pstate[0], &vminfo[j].pstate[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);
                strcpy((char *)vmstate.host_ip, (char *)vminfo[j].manage_ip);
                break;
            }
        }

#if 0
        if (vmstate.vm_process_count == 0) {
            continue;
        }
#endif

        if (j == MAX_SYSTEM_NUM) {
            memcpy(&vmstate.pstate[0], &basePState[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);
        }

        vmstate.bp_state = hostConf.bp_mode;

        /* Insert DB (Cloud Mode) */
        if (hostConf.cloud_mode == 1) {
#ifdef __PGSQL__
            pq_retval = PQ_SelectCountVMStateInfo(&vmstate);

            if (pq_retval == 0) {
                PQ_InsertVMStateInfo(&vmstate);
            } else if (pq_retval >= 1) {
                if (PQ_CheckVMStateInfo(&vmstate) == 0) {
                    PQ_UpdateVMStateInfo(&vmstate);
                }
            }
#endif
        }
    }

    /* Insert Baremetal State */
    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        if (vminfo[i].vm_ip == 0) {
            continue;
        }

        /* baremetal */
        for (j = 0; j < vmcount; j ++) {
            /* search baremetal */
            if (vminfo[i].vm_ip == vstatus[j].vm_ipaddr) {
                v_flag = 1;
                break;
            }
        }

        if (v_flag == 1) {
            continue;
        }

        /* set vmstate */
        memset(&vmstate, 0x00, sizeof(vm_process_t));

        strcpy((char *)vmstate.TimeStamp, ttime);
        strcpy((char *)vmstate.vm_id, (char *)vminfo[i].vm_id);
        strcpy((char *)vmstate.vm_name, (char *)vminfo[i].vm_name);
        strcpy((char *)vmstate.host_id, "UTM");
        strcpy((char *)vmstate.host_ip, (char *)vminfo[i].manage_ip);

        vmstate.vm_type          = vminfo[i].vm_type;
        vmstate.vm_state         = STATUS_ACTIVE;
        vmstate.vm_power_state   = PW_STATUS_RUNNING;
        vmstate.vm_process_count = vminfo[i].pcount;
        memcpy(&vmstate.pstate[0], &vminfo[i].pstate[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);

        vmstate.bp_state = hostConf.bp_mode;

        /* Insert DB (baremetal_mode)*/
        if (hostConf.baremetal_mode == 1) {
#ifdef __PGSQL__
            PQ_InsertVMStateInfo(&vmstate);
#endif
        }
    }

    return 0;
}


