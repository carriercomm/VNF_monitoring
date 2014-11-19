/* -------------------------------------------------------------------------
 *  File Name   : mqipc.h
 *  Author      : Hyo-Seok Kang
 *  Copyright   : Copyright (C) 2003-2008 by KBell Inc.
 *  Descriptions: 시스템의 메시지 큐 통신을 위한 함수와 키 값을 정의하였다.
 *
 *  History     : 03/12/29        Initial Coded
 * ------------------------------------------------------------------------*/

#ifndef __MQIPC_H__
#define __MQIPC_H__

#define IPC_MAX_LENGTH         4098            /* drop max data size */
#define IPC_DATA_LENGTH        2048            /* real max data size */
#define IPC_HEAD_LENGTH        16              /* common header size */

/* common message struct */
typedef struct {
    long    msgtype;                /* message type         */
    int     src_qid;                /* source queue id      */
    int     dst_qid;                /* destination queue id */
    int     datalen;                /* user data length     */
    char userdata[IPC_DATA_LENGTH - IPC_HEAD_LENGTH];    /* user data */
} ipc_msg_struct;

extern int ipcClean(int msg_qid);
extern int ipcOpen(key_t msgKey, int msgflag);
extern int ipcSend(int msg_qid, unsigned int mtype, int src_qid, int msgsize, void *sndBuf);
extern int ipcRecv(int msg_qid, ipc_msg_struct *msgBuf, int msgflg);

#endif
