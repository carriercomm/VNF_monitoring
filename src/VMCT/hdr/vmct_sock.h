/* -------------------------------------------------------------------------
 * File Name   : vmct_sock.h
 * Author      : Hyeong-Ik Jeon 
 * Copyright   : Copyright (C) 2005-2014 by KBell Inc.
 * Description : BPAPP-VMCT tcp fn.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __VMCT_SOCK_H__
#define __VMCT_SOCK_H__

/** used */
#define TMR_NOT_USED            0
#define TMR_USED                1

/** mode */
#define LINK_MODE_CLIENT        1
#define LINK_MODE_SERVER        2

/** version */
#define LINK_IP_V4_TYPE         4
#define LINK_IP_V6_TYPE         6

#define TCP_RECONNTIME          1000
#define TCP_HEARTBEATTIME       2000

#define TCP_VERSION             0x04

#define MAX_TMR_NUM             1024
#define MAX_DATA_LEN            4096 * 100

#if 0
#define MAX_LISTENQ             ((MAX_DSG_LIST + MAX_CCS_LIST) * 10) /* listen */
#else
#define MAX_LISTENQ             32 /* listen */
#endif

#define TPKT_HEADER_SIZE        4
#define TCP_SND_RETRY           3       /* send retry */

#define SET_BLOCKING            0       /* set blocking */
#define SET_NONBLOCKING         1       /* set non-blocking */

#define POLL_FD_UNUSED          -1

#define SOCK_LINK_TEST          0x0010

typedef struct {
    int tmrId;
    int tmrVal;
    int sendFlag; /* 일정 시간동안 데이터가 socket을 통해 받았는지 확인하는 flag */
    int recvFlag; /* 일정 시간동안 데이터가 socket을 통해 받았는지 확인하는 flag */
    int retryCnt; /* 연속 3번이상 테이타를 못받으면 해당 socket를 끊어 버린다. */
} tmrInfo_t;

typedef union {
    struct sockaddr_storage ss;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
    struct sockaddr sa;
} sockaddr_storage_t;

typedef struct {
    int  mode;   
    int  version;
    char addr[128];
    int  port;
    int  ListenFD;
    tmrInfo_t tmr_hb;
    tmrInfo_t tmr_reconn;
    sockaddr_storage_t sockaddr_storage;
} LinkInfo_t;

typedef struct {
    int             used;
    unsigned int    primId;
    int             argc1;
    int             argc2;
} TmrSyncStack_t;

typedef struct {
    unsigned int    systemid;
    unsigned int    systemaddr;
    unsigned short  systemport;
    char            info[MAX_INFO_LENGTH];
} link_system_t;

typedef struct {
    int             AcceptFD;
    tmrInfo_t       tmr_hb;
    link_system_t   sysinfo;
    pthread_mutex_t linkmutex;
} AcceptList_t;


extern int MaxFD;
extern fd_set rsetHOST, allsetHOST;
extern LinkInfo_t tlinkHOST;
extern TmrSyncStack_t tmrsync[MAX_TMR_NUM+1];
extern AcceptList_t alist[MAX_LISTENQ];

extern int compareMaxfd(int compareFD);
extern inline int send_extendedPoll(int fd, int time, fd_set *sendFdSet);
extern inline int recv_extendedPoll(int time, fd_set *readFdSet);

extern int TcpOpen(LinkInfo_t *link);
extern int TcpSend(int accept_idx, int fd, unsigned char *uiBuf, int nbytes, int tcpmode, int dsgidx);
extern int TcpRecv(int accept_idx, int fd, unsigned char *rBuf, int tcpmode, int dsgidx);
extern int TcpAccept(int listenFd, int dsgidx);
extern void TcpReconnect(int timerId, int param1, int param2, int param3);
extern int TcptmrStart(unsigned int interval, unsigned int mType, int argc1, int argc2);
extern int TcptmrStop(int timer_id);
extern void TcpInfo(void);

extern int UdpOpen(int port);
extern int UdpRecv(int fd, int size, struct sockaddr_in *saddr, unsigned char *rBuf);

#endif
