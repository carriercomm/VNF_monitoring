/* -------------------------------------------------------------------------
 * File Name   : host_tcp.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : Host Tcp header
 * History     : 14/07/03    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_SOCK_H__
#define __HOST_SOCK_H__

/** mode */
#define LINK_MODE_CLIENT        1
#define LINK_MODE_SERVER        2

/** version */
#define LINK_IP_V4_TYPE         4
#define LINK_IP_V6_TYPE         6

#define SET_BLOCKING            0       /* set blocking */
#define SET_NONBLOCKING         1       /* set non-blocking */

#if 0
#define MAX_LISTENQ             (MAX_SYSTEM_NUM * 10)
#else
#define MAX_LISTENQ             (MAX_SYSTEM_NUM * 1)
#endif

#define TPKT_HEADER_SIZE        4

#define POLL_FD_UNUSED          -1
#define TCP_SND_RETRY           3       /* send retry */
#define TCP_VERSION             0x04

typedef struct {
    unsigned int   systemidx;
    unsigned int   systemid;
    unsigned int   systemaddr;

    unsigned short port;
    char           systemname[MAX_STR_LENGTH];
} link_system_t;

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
    tmrInfo_t tmr_reconn;
    sockaddr_storage_t sockaddr_storage;
} LinkInfo_t;

typedef struct {
    int             AcceptFD;
    link_system_t   sysinfo;
    pthread_mutex_t linkmutex;
} AcceptList_t;


extern int MaxFD;
extern fd_set rset, allset;
extern LinkInfo_t   tlink;
extern AcceptList_t alist[MAX_LISTENQ];


extern int compareMaxfd(int compareFD);
extern inline int recv_extendedPoll(int time, fd_set *readFdSet);
extern inline int send_extendedPoll(int fd, int time, fd_set *sendFdSet);

extern int TcpOpen(LinkInfo_t *link);
extern int TcpAccept(int listenFd);
extern int TcpSend(int accept_idx, int fd, unsigned char *SndMsg, int nbytes);
extern int TcpRecv(int accept_idx, int fd, unsigned char *rBuf);

extern void WriteProcInfo(void);
extern void TcpInfo(void);


#endif /* __HOST_SOCK_H__ */
