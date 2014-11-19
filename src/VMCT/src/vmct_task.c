/* -------------------------------------------------------------------------
 * File Name   : vmct_task.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : VMCT task fn.
 * History     : 14/07/01    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"
#include "timer.h"
#include "mqipc.h"


#include "vmct_main.h"
#include "vmct_conf.h"
#include "vmct_task.h"
#include "vmct_sock.h"

static int SendProcStateReq(void);

static void ThreadCleanUp(void *arg)
{
    int task;
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
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK0 (TcpRcv Task)\n");
    } else if (task == PTASK1) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK1 (TcpSession Task)\n");
#if 0
        /* mutex close */
        if (&TcpHostMutex != NULL) {
            pthread_mutex_unlock(&TcpHostMutex);
        }
#else
        pthread_mutex_unlock(&TcpHostMutex);
#endif
    } else if (task == PTASK2) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK2 (TmrCmd Task)\n");
#if 0
    } else if (task == PTASK3) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK3 (CheckUserAccountDB Task)\n");
    } else if (task == PTASK4) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK4 (DeleteAutoProfile Task)\n");

    } else if (task == PTASK5) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK5 (TcpStat[DSG - 0] Task)\n");
    } else if (task == PTASK6) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK6 (TcpStat[DSG - 1] Task)\n");
    } else if (task == PTASK7) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK7 (TcpStat[DSG - 3] Task)\n");
    } else if (task == PTASK8) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK8 (TcpStat[DSG - 3] Task)\n");
    } else if (task == PTASK9) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK9 (TcpStat[DSG - 4] Task)\n");
    } else if (task == PTASK10) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK10 (TcpStat[DSG - 5] Task)\n");
    } else if (task == PTASK11) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK11 (TcpStat[DSG - 6] Task)\n");
    } else if (task == PTASK12) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK12 (TcpStat[DSG - 7] Task)\n");
    } else if (task == PTASK13) {
        xprint(FMS_VMC | FMS_LOOKS, "|<--------| ThreadCleanUp : PTASK13 (AlarmRelease Task)\n");
#endif

    }

    return;
}

#if 0
/* Receive Configuration Info from HOST */
int RegisterCFM(void)
{
    int ret, numEvents, reSendReg = 0;
    unsigned char buf[MAX_DATA_LEN];

    memset(buf, 0x0, sizeof(buf));

    FD_ZERO(&allsetHOST);
    tlinkHOST.ListenFD = TcpOpen(&tlinkHOST);

    pthread_mutex_lock(&TcpHostMutex);
    if (tlinkHOST.ListenFD <= 0) {
        if (tlinkHOST.mode == LINK_MODE_SERVER) {
            xprint(FMS_VMC | FMS_ERROR, "%s : S-TCP TcpOpen failed\n", __func__);
        } else if (tlinkHOST.mode == LINK_MODE_CLIENT) {
            pthread_mutex_unlock(&TcpHostMutex);
            return (-1);
        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s : unknown socket mode\n", __func__);
        }
    } else {
        FD_SET(tlinkHOST.ListenFD, &allsetHOST);
        compareMaxfd(tlinkHOST.ListenFD);
        if (tlinkHOST.mode == LINK_MODE_CLIENT) {
            TcpInfo();
        }
    }
    pthread_mutex_unlock(&TcpHostMutex);

    /* Send Register Msg */
    RegisterMsgSend(tlinkHOST.ListenFD);

    for (;;) {
        rsetHOST = allsetHOST;

        /* Re-Send Register Msg */
        if (reSendReg > 25) {
            reSendReg = 0;
            RegisterMsgSend(tlinkHOST.ListenFD);
        }

        if (0 > (numEvents = recv_extendedPoll(200, &rsetHOST))) {
            xprint(FMS_VMC | FMS_ERROR, "recv_extendedPoll (reason=%s) [%s(%d)] \n", strerror(errno), __func__, __LINE__);
            if (errno == EINTR) {
                continue;
            } else if (errno == EBADF || errno == EPIPE || errno == EIO) {
                xprint(FMS_VMC | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            } else {
                xprint(FMS_VMC | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            }
        } else if (numEvents == 0) {
            /* Time out 인 경우 정상처리 한다. */
            reSendReg++;
            continue;
        } else {
            if (tlinkHOST.mode == LINK_MODE_CLIENT) {
                pthread_mutex_lock(&TcpHostMutex);
                if (tlinkHOST.ListenFD > 0) {
                    if (FD_ISSET(tlinkHOST.ListenFD, &rsetHOST)) {
                        if ((ret = TcpRecv(0, tlinkHOST.ListenFD, buf, LINK_MODE_CLIENT, 0)) > 0) {
                            if (NBO_NTOHS(((common_tcpmsg_t *)buf)->MsgType) == DBS_REGISTER_RSP) {
                                xprint(FMS_VMC | FMS_INFO1, "|<--------| DBS_REGISTER_RSP : (fd=%2d) \n", tlinkHOST.ListenFD);

                                /* Process Collector Config Msg */
                                RecvConfigInfo(buf);

                                pthread_mutex_unlock(&TcpHostMutex);
                                break;
                            }

                        } else {
                            xprint(FMS_VMC | FMS_ERROR, "MsgType is Unknown %s(%d) \n", __func__, __LINE__);
                            pthread_mutex_unlock(&TcpHostMutex);
                        }
                    } else {
                        xprint(FMS_VMC | FMS_ERROR, "FD_ISSET is Unknown %s(%d) \n", __func__, __LINE__);
                        pthread_mutex_unlock(&TcpHostMutex);
                    }

                } else {
                    xprint(FMS_VMC | FMS_ERROR, "FD is Unknown %s(%d) \n", __func__, __LINE__);
                    pthread_mutex_unlock(&TcpHostMutex);
                }
            }
        }
    }

    /* Close FD */
    if (tlinkHOST.ListenFD > 0) {
        close(tlinkHOST.ListenFD);
        FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
        tlinkHOST.ListenFD = -1;
    }

    return (0);
}
#endif

void *TcpRcvTask(void *argc)
{
    int ret, numEvents, dbcount = 0;
    unsigned char buf[MAX_DATA_LEN];

    ThreadAlive[PTASK0] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    xprint(FMS_VMC | FMS_INFO1, "%s Start \n", __func__);

    memset(buf, 0x0, sizeof(buf));

    FD_ZERO(&allsetHOST);
    tlinkHOST.ListenFD = TcpOpen(&tlinkHOST);

    pthread_mutex_lock(&TcpHostMutex);
    if (tlinkHOST.ListenFD <= 0) {
        if (tlinkHOST.mode == LINK_MODE_SERVER) {
            xprint(FMS_VMC | FMS_ERROR, "%s : S-TCP TcpOpen failed\n", __func__);
        } else if (tlinkHOST.mode == LINK_MODE_CLIENT) {
            if (tlinkHOST.tmr_reconn.tmrId > 0) {
                TcptmrStop(tlinkHOST.tmr_reconn.tmrId);
            }
            tlinkHOST.tmr_reconn.tmrId = TcptmrStart(tlinkHOST.tmr_reconn.tmrVal, TMR_TCP_RECONNECT_MSG, 0, 0);
        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s : unknown socket mode\n", __func__);
        }
    } else {
        FD_SET(tlinkHOST.ListenFD, &allsetHOST);
        compareMaxfd(tlinkHOST.ListenFD);
    }
    pthread_mutex_unlock(&TcpHostMutex);

    SendProcStateReq();

    for (;;) {
        if (ThreadAlive[PTASK0] != TRUE) {
            break;
        }

        rsetHOST = allsetHOST;
        if (0 > (numEvents = recv_extendedPoll(200, &rsetHOST))) {
            xprint(FMS_VMC | FMS_ERROR, "recv_extendedPoll (reason=%s) [%s(%d)] \n",
                    strerror(errno), __func__, __LINE__);
            if (errno == EINTR) {
                continue;
            } else if (errno == EBADF || errno == EPIPE || errno == EIO) {
                xprint(FMS_VMC | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            } else {
                xprint(FMS_VMC | FMS_ERROR, "select at ProcessEvent(%s) EXIT !!\n", strerror(errno));
            }

#ifdef __UTM__
            nanoSleep(1000);
#endif

        } else if (numEvents == 0) {

            /* Check DB Handelr1 every 1min, by jeon */
            if (dbcount == 300) {
#if 0
                CheckTimeHLR1();
#endif
                dbcount = 0;
            } 

            dbcount ++;
            /* Time out 인 경우 정상처리 한다. */
            continue;

        } else {
            if (tlinkHOST.mode == LINK_MODE_SERVER) {
            }
            else if (tlinkHOST.mode == LINK_MODE_CLIENT) {
                pthread_mutex_lock(&TcpHostMutex);
                if (tlinkHOST.ListenFD > 0) {
                    if (FD_ISSET(tlinkHOST.ListenFD, &rsetHOST)) {
                        if ((ret = TcpRecv(0, tlinkHOST.ListenFD, buf, LINK_MODE_CLIENT, 0)) > 0) {
                            tlinkHOST.tmr_hb.retryCnt = 0;
                            tlinkHOST.tmr_hb.recvFlag = 1;

#if 0 
                            if (NBO_NTOHS(((common_tcpmsg_t *)buf)->MsgType) == SYSTEM_INFO_RSP) {
                                xprint(FMS_VMC | FMS_INFO1, "|<--------| SYSTEM_INFO_RSP : (fd=%2d) \n", tlinkHOST.ListenFD);

                                pthread_mutex_unlock(&TcpHostMutex);
                                continue;

                            } else {
                                xprint(FMS_VMC | FMS_ERROR, "MsgType is Unknown %s(%d) \n", __func__, __LINE__);
                                pthread_mutex_unlock(&TcpHostMutex);
                                continue;
                            }
#else
                            xprint(FMS_VMC | FMS_INFO1, "|<--------| HOST_RSP : (fd=%2d) \n", tlinkHOST.ListenFD);
                            pthread_mutex_unlock(&TcpHostMutex);
#endif

                        } else {
                            xprint(FMS_VMC | FMS_ERROR, "TCP Conf Recv Error %s(%d) \n", __func__, __LINE__);
                            pthread_mutex_unlock(&TcpHostMutex);
                        }
                    } else {
                        xprint(FMS_VMC | FMS_ERROR, "FD_ISSET is Unknown %s(%d) \n", __func__, __LINE__);
                        pthread_mutex_unlock(&TcpHostMutex);
                    }
                } else {
                    xprint(FMS_VMC | FMS_ERROR, "FD is Unknown %s(%d) \n", __func__, __LINE__);
                    pthread_mutex_unlock(&TcpHostMutex);
                }
            }
        }
    }

    ThreadAlive[PTASK0] = FALSE;
    ThreadFlags[PTASK0] = FALSE;

    /* pthread mgnt */
    pthread_cleanup_pop(1);

    xprint(FMS_VMC | FMS_INFO1, "%s Stop \n", __func__);
    return(NULL);
}

void *TcpSessionTask(void *argc)
{
    struct timeval tv;
    struct tm ctime;
    int TimeSecCheck = FALSE;
    char ttime[MAX_TIME_LENGTH];

    ThreadAlive[PTASK1] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    gettimeofday(&tv, NULL);
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

#if 0
    dftype = DATA_PARTITION;
    check_df_type();
#endif

    xprint(FMS_VMC | FMS_INFO1, "%s Start \n", __func__);

    for (;;) {
        if (ThreadAlive[PTASK1] != TRUE) {
            break;
        }

        nanoSleep(1000);

        gettimeofday(&tv, NULL);
        localtime_r((time_t *)&(tv.tv_sec), &ctime);

        if ((ctime.tm_sec % VmctConf.vmstate_sendtime) == 1) {
            if (TimeSecCheck == TRUE) {
                continue;
            }

            xprint(FMS_VMC | FMS_INFO1, "%s :: Now Time[= %02d:%02d:%02d]\n", __func__,
                                        ctime.tm_hour, ctime.tm_min, ctime.tm_sec);

#if 0
            /* Send System Info (1 min) to HOST */
            SendSystemInfo(ctime.tm_min);
#else
            memset(&ttime, 0x00, sizeof(ttime));
            sprintf(ttime, "%04d-%02d-%02d, %02d:%02d:%02d",
                   ctime.tm_year + 1900, ctime.tm_mon + 1, ctime.tm_mday,
                   ctime.tm_hour, ctime.tm_min, ctime.tm_sec-1);

            SendProcStateReq();
#endif

            if (tlinkHOST.mode == LINK_MODE_CLIENT) {
                pthread_mutex_lock(&TcpHostMutex);
                if (tlinkHOST.ListenFD > 0) {
                    if (tlinkHOST.tmr_hb.recvFlag == 0) {
                        tlinkHOST.tmr_hb.retryCnt++;
                    }

                    if (tlinkHOST.tmr_hb.retryCnt >= TCP_SND_RETRY) {

                        xprint(FMS_VMC | FMS_ERROR,
                                "|<---XX--->| HB Not Receive C-TCP (addrs=%s:%d)(FD=%2d), retryCnt=%d \n",
                                tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD, tlinkHOST.tmr_hb.retryCnt);

                        tlinkHOST.tmr_hb.retryCnt = 0;
                        tlinkHOST.tmr_hb.sendFlag = 0;
                        tlinkHOST.tmr_hb.recvFlag = 0;

#ifndef __UTM__
                        FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
#else
                        FD_ZERO(&allsetHOST);
#endif
                        close(tlinkHOST.ListenFD);
                        tlinkHOST.ListenFD = -1;

                        /*TcpInfo();*/

                        if (tlinkHOST.tmr_reconn.tmrId > 0) {
                            TcptmrStop(tlinkHOST.tmr_reconn.tmrId);
                        }

                        tlinkHOST.tmr_reconn.tmrId = TcptmrStart(tlinkHOST.tmr_reconn.tmrVal, TMR_TCP_RECONNECT_MSG, 0, 0);
                    }

                    tlinkHOST.tmr_hb.recvFlag = 0;
                }
                pthread_mutex_unlock(&TcpHostMutex);
            }

            TimeSecCheck = TRUE;

        } else {
            TimeSecCheck = FALSE;
        }

        xprint(FMS_VMC | FMS_INFO3, "%s :: sec(%ld), usec(%ld)\n", __func__, tv.tv_sec, tv.tv_usec);
        xprint(FMS_VMC | FMS_INFO3, "%s :: hour(%02d), min(%02d), sec(%02d)\n", __func__,
                                     ctime.tm_hour, ctime.tm_min, ctime.tm_sec);
    }

    ThreadAlive[PTASK1] = FALSE;
    ThreadFlags[PTASK1] = FALSE;

    pthread_cleanup_pop(1);

    xprint(FMS_VMC | FMS_INFO1, "%s Stop \n", __func__);
    return(NULL);
}

void *TmrCmdTask(void *argc)
{
    ipc_msg_struct ipcBuf;
    int l_src_qid, l_dst_qid;

    int flag = 0;
    int primId, tId_idx=0;
    tmr_expire_t *tmrMsg=NULL;

    ThreadAlive[PTASK2] = TRUE;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(ThreadCleanUp, (void *)NULL);

    xprint(FMS_VMC | FMS_INFO1, "%s Start \n", __func__);

    for (;;) {
        if (ThreadAlive[PTASK2] != TRUE) {
            break;
        }

        if ((primId = ipcRecv(TmrCmdQid, (void *)&ipcBuf, flag)) == -1) {
            xprint(FMS_VMC | FMS_ERROR,
                    "ipcRecv failed, error:%s (%d(%s)) \n", strerror(errno), __LINE__, __FILE__);
            continue;
        }

        /*pthread_mutex_lock(&ProcMutex);*/

        l_src_qid = ipcBuf.src_qid;
        l_dst_qid = ipcBuf.dst_qid;

        switch (primId) {
            case TMR_TCP_RECONNECT_MSG:
                tmrMsg = (tmr_expire_t *)ipcBuf.userdata;
                tId_idx = tmrMsg->timerId;
                if (tmrsync[tId_idx].used != TMR_USED) {
                    xprint(FMS_VMC | FMS_ERROR,
                            "EXP : TMR_TCP_RECONNECT_MSG |(timerId=%4d, argc1=%4d, argc2=%4d, argc3=%4d \n",
                            tmrMsg->timerId, tmrMsg->argc1, tmrMsg->argc2, tmrMsg->argc3);
                    break;
                }

                tmrsync[tId_idx].used = TMR_NOT_USED;
                xprint(FMS_VMC | FMS_INFO5,
                        "|----S--->| EXP : TMR_TCP_RECONNECT_MSG |(timerId=%4d, argc1=%4d, argc2=%4d, argc3=%4d \n",
                        tmrMsg->timerId, tmrMsg->argc1, tmrMsg->argc2, tmrMsg->argc3);
                TcpReconnect(tmrMsg->timerId, tmrMsg->argc1, tmrMsg->argc2, tmrMsg->argc3);
                break;

            case TMR_TEST_MSGQ_MSG:
                /* reserved */
                break;
            default :
                xprint(FMS_VMC | FMS_ERROR, "src_qid : %d , dst_qid : %d\n", l_src_qid, l_dst_qid);
                xprint(FMS_VMC | FMS_ERROR, "UNKNOWN         ----->|                | (0x%X)\n", primId);
                break;
        }
        /*pthread_mutex_unlock(&ProcMutex);*/
    }
    ThreadAlive[PTASK2] = FALSE;
    ThreadFlags[PTASK2] = FALSE;

    pthread_cleanup_pop(1);

    xprint(FMS_VMC | FMS_INFO1, "%s Stop \n", __func__);
    return(NULL);
}


static int SendProcStateReq(void)
{
    int TcpMsgLength, len = 0, i;
    common_tcpmsg_t sndmsg;
    vm_proc_state_msg pstatemsg;
    unsigned char   *sndbuf = NULL;

    /* Send to HOST */
    sndmsg.TpktHdr  = 0x00;
    sndmsg.SystemID = 0x01;
    sndmsg.MsgType  = VM_PROC_STATE_REQ;
    sndmsg.MsgSize  = sizeof(vm_proc_state_msg);

    /* set vm_proc_state_msg */
    memset(&pstatemsg, 0x00, sizeof(vm_proc_state_msg));
    for (i = 0; i < MAX_VM_PROC_NUM; i++) {
        strcpy((char *)pstatemsg.pstate[i].process_name, "-");
    }

    pstatemsg.vm_ipaddress = NBO_HTONL(VmctConf.vmct_ip); 
    pstatemsg.vm_type      = NBO_HTONL(VmctConf.vm_type);
    pstatemsg.pcount       = NBO_HTONL(VmctConf.vmct_proc.pcount);

    for (i = 0; i < VmctConf.vmct_proc.pcount; i++) {
        if (proccheckbyname((char *)VmctConf.vmct_proc.pstate[i].process_name) != 0) {
            VmctConf.vmct_proc.pstate[i].process_state = PROCESS_STATE_UP;
        } else {
            VmctConf.vmct_proc.pstate[i].process_state = PROCESS_STATE_DOWN;
        }
        strcpy((char *)pstatemsg.pstate[i].process_name, (char *)VmctConf.vmct_proc.pstate[i].process_name);
        pstatemsg.pstate[i].process_state = NBO_HTONL(VmctConf.vmct_proc.pstate[i].process_state);
    }

    /* 20140926, by jeon */
    strcpy((char *)pstatemsg.manage_ip, VmctConf.manage_hostip);
    if (VmctConf.vm_type == VM_TYPE_HOST) {
        sprintf((char *)pstatemsg.vm_id, "%s-%x", VmctConf.vmct_name, VmctConf.vmct_id);
        sprintf((char *)pstatemsg.vm_name, "vUTM_%s", addr2ntoa(VmctConf.vmct_ip));
    } else {
        strcpy((char *)pstatemsg.vm_id, "-");
        strcpy((char *)pstatemsg.vm_name, "-");
    }

    TcpMsgLength = DEFAULT_TCP_COMHDR_LENGTH + sndmsg.MsgSize;

    sndbuf = (unsigned char *)malloc(TcpMsgLength);
    if (sndbuf == NULL) {
        xprint(FMS_VMC | FMS_ERROR, "sndbuf is NULL\n");
        return(-1);
    }

    TCP_MSG_HTON((common_tcpmsg_t *)&sndmsg);

    memcpy(&sndbuf[len], (unsigned char *)&sndmsg, DEFAULT_TCP_COMHDR_LENGTH);
    len += DEFAULT_TCP_COMHDR_LENGTH;
    memcpy(&sndbuf[len], &pstatemsg, sizeof(vm_proc_state_msg));

    if (tlinkHOST.ListenFD > 0) {
        TcpSend(0, tlinkHOST.ListenFD, sndbuf, TcpMsgLength, LINK_MODE_CLIENT, 0);
        xprint(FMS_VMC | FMS_LOOKS, "|----T---->| VM_PROC_STATE_REQ SEND : VmIdx(%2d), (fd=%2d), len(%d)\n",
                                     VmctConf.vmct_id, tlinkHOST.ListenFD, TcpMsgLength);
    } else {
        xprint(FMS_VMC | FMS_WARNING, "|-------->| Not Send VM_PROC_STATE_REQ : VmIdx(%2d),(fd=%2d), len(%d) \n",
                                     VmctConf.vmct_id, tlinkHOST.ListenFD, TcpMsgLength);

        /* by jeon */
        pthread_mutex_lock(&TcpHostMutex);
        close(tlinkHOST.ListenFD);

#ifndef __UTM__
        FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
#endif
        tlinkHOST.ListenFD = -1;

        FD_ZERO(&allsetHOST);
        tlinkHOST.ListenFD = TcpOpen(&tlinkHOST);
        pthread_mutex_unlock(&TcpHostMutex);

        if (tlinkHOST.ListenFD > 0) {

            FD_SET(tlinkHOST.ListenFD, &allsetHOST);
            compareMaxfd(tlinkHOST.ListenFD);

            TcpSend(0, tlinkHOST.ListenFD, sndbuf, TcpMsgLength, LINK_MODE_CLIENT, 0);
            xprint(FMS_VMC | FMS_LOOKS, "|----T---->| VM_PROC_STATE_REQ SEND : VmIdx(%2d), (fd=%2d), len(%d)\n",
                                     VmctConf.vmct_id, tlinkHOST.ListenFD, TcpMsgLength);
        }
    }
    free(sndbuf);

    return(0);
}




