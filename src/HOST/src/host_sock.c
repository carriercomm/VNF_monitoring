/* -------------------------------------------------------------------------
 * File Name   : host_sock.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2013 by KBell Inc.
 * Description : Host Tcp Fn.
 * History     : 14/07/03    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#include "host_main.h"
#include "host_conf.h"
#include "host_sock.h"


int MaxFD;
fd_set rset, allset;
LinkInfo_t     tlink;                  /* Tcp Link Info */
AcceptList_t   alist[MAX_LISTENQ];     /* Accept List */


/* TCP NON-BLOCK MODE SETTING */
static int setNonblocking(int fd, int mode)
{
    int n, flags;

    if (mode == SET_NONBLOCKING) {
        if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
            xprint(FMS_HST | FMS_ERROR, "fcntl(F_GETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
                    fd, mode, strerror(errno), __func__, __LINE__);
            return(-1);
        }

        if (-1 == (n = fcntl(fd, F_SETFL, flags | O_NONBLOCK))) {
            xprint(FMS_HST | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
                    fd, mode, strerror(errno), __func__, __LINE__);
            return(-1);
        }
    } else {
        if (-1 == (flags = fcntl(fd, F_SETFL, 0))) {
            xprint(FMS_HST | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
                    fd, mode, strerror(errno), __func__, __LINE__);
            return(-1);
        }
    }

    return(0);
}

/* TCP CONNECTION FAST CHECK NO-BLOCK */
static int connect_timeout(int fd, struct sockaddr *addr, socklen_t len, int msec)
{
    int n, error=0, flags;
    struct timeval tv;
    fd_set readSet, writeSet;

    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        xprint(FMS_HST | FMS_ERROR, "fcntl(F_GETFL) : fd=%d, reason=%s [%s(%d)]\n",
                fd, strerror(errno), __func__, __LINE__);
        return(-1);
    }

    if (-1 == (n = fcntl(fd, F_SETFL, flags | O_NONBLOCK))) {
        xprint(FMS_HST | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, reason=%s [%s(%d)]\n",
                fd, strerror(errno), __func__, __LINE__);
        return(-1);
    }

    if (0 > (n = connect(fd, (struct sockaddr *)addr, len))) {
        if (errno != EINPROGRESS) {
            return(-1);
        }
    }

    if (n == 0) {
        goto done;
    }

    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_SET(fd, &readSet);
    FD_SET(fd, &writeSet);
    tv.tv_sec  = msec / 1000;
    tv.tv_usec = 1000 * (msec % 1000);

    n = select(fd + 1, &readSet, &writeSet, NULL, &tv);
    if (n == 0) {
        errno = ETIMEDOUT;
        return(-1);
    }

    if (n == -1) {
        return(-1);
    }

    if (FD_ISSET(fd, &readSet) || FD_ISSET(fd, &writeSet)) {
        if (FD_ISSET(fd, &readSet) && FD_ISSET(fd, &writeSet)) {
            unsigned int errlen;
            errlen = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errlen) < 0) {
                errno = ETIMEDOUT;
                return(-1);
            }

            if (error == 0) {
                goto done;
            } else {
                errno = error;
                return(-1);
            }
        }
    } else {
        return(-1);
    }

done:
    if (-1 == (n = fcntl(fd, F_SETFL, flags))) {
        xprint(FMS_HST | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, reason=%s [%s(%d)]\n",
                fd, strerror(errno), __func__, __LINE__);
        return(-1);
    }

    return(0);
}

/* TCP SELECT MAX_FD FIND */
int compareMaxfd(int compareFD)
{
    if (compareFD > 0) {
        if (MaxFD < compareFD) {
            MaxFD = compareFD;
        }
    }

    return MaxFD;
}

/* TCP SOCKET OPEN AND optional Settting */
int TcpOpen(LinkInfo_t *link)
{
    int           ret, newfd=0;
    int           error;
    unsigned char reuse;
    struct linger lngr;

    if (link->version == LINK_IP_V4_TYPE) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&link->sockaddr_storage.v4;
        sa4->sin_family = AF_INET;
        sa4->sin_port = NBO_HTONS(link->port);
        if (!(inet_pton(AF_INET, link->addr, &sa4->sin_addr))) {
            return(-1);
        }
    } else if (link->version == LINK_IP_V6_TYPE) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&link->sockaddr_storage.v6;
        sa6->sin6_family = AF_INET6;
        sa6->sin6_port = NBO_HTONS(link->port);
        if (!(inet_pton(AF_INET6, link->addr, &sa6->sin6_addr))) {
            return(-1);
        }
    }

    if (link->mode == LINK_MODE_CLIENT) {
        if ((newfd = socket(link->sockaddr_storage.sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            xprint(FMS_HST | FMS_ERROR, "C-TCP socket() ip=%s:%d, reason=%s\n",
                    link->addr, link->port, strerror(errno));
            return(newfd);
        }

        /** SO_SNDBUF : socket snd buf */
        {
            int sndbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int))) == -1) {
                xprint(FMS_HST | FMS_ERROR,
                        "C-TCP setsockopt(SO_SNDBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s\n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-1);
            }
        }

        /** SO_RCVBUF : socket rcv buf */
        {
            int rcvbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int))) == -1) {
                xprint(FMS_HST | FMS_ERROR,
                        "C-TCP setsockopt(SO_RCVBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s \n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-1);
            }
        }

        /** connect request */
        ret = connect_timeout(newfd, (struct sockaddr *)&link->sockaddr_storage,
                sizeof(link->sockaddr_storage), 50);

        /** Setting Non-Block Mode Socket */
        setNonblocking(newfd, SET_NONBLOCKING);

        if (ret < 0) {
            close(newfd);
            newfd = -1;
            xprint(FMS_HST | FMS_WARNING, "|---XXX-->| connection ip=%s:%d(%s)\n",
                    link->addr, link->port, strerror(errno));
        } else if (ret == 0) {
            xprint(FMS_HST | FMS_LOOKS, "|-------->| connection OK ip=%s:%d(fd=%d).\n",
                    link->addr, link->port, newfd);
        }
    } else if (link->mode == LINK_MODE_SERVER) {
        if ((newfd = socket(link->sockaddr_storage.sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
            xprint(FMS_HST | FMS_ERROR, "S-TCP socket() ip=%s:%d, reason=%s\n",
                    link->addr, link->port, strerror(errno));
            return(newfd);
        }

        /* Set SO_LINGER on socket */
        lngr.l_onoff  = 1;
        lngr.l_linger = 0;
        if (setsockopt(newfd, SOL_SOCKET, SO_LINGER, (char *)&lngr, sizeof(lngr)) < 0) {
            xprint(FMS_HST | FMS_ERROR, "S-TCP setsockopt(SO_LINGER) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-2);
        }

        /* Set SO_REUSEADDR on socket */
        reuse = 1;
        if (setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1) {
            xprint(FMS_HST | FMS_ERROR, "S-TCP setsockopt(SO_REUSEADDR) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-3);
        }

        /** SO_SNDBUF : socket snd buf */
        {
            int sndbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int))) == -1) {
                xprint(FMS_HST | FMS_ERROR,
                        "S-TCP setsockopt(SO_SNDBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s \n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-3);
            }
        }

        /** SO_RCVBUF : socket rcv buf */
        {
            int rcvbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int))) == -1) {
                xprint(FMS_HST | FMS_ERROR,
                        "S-TCP setsockopt(SO_RCVBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s \n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-3);
            }
        }

        /** binding */
        if (bind(newfd, (struct sockaddr *)&link->sockaddr_storage, sizeof(link->sockaddr_storage)) < 0) {
            xprint(FMS_HST | FMS_ERROR, "S-TCP bind() ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-5);
        }

        /** listen */
        if (listen(newfd, MAX_LISTENQ) < 0) {
            xprint(FMS_HST | FMS_ERROR, "S-TCP listen() ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-6);
        }
        xprint(FMS_HST | FMS_LOOKS, "|-------->| listen ip=%s:%d(fd=%d)\n", link->addr, link->port, newfd);
        setNonblocking(newfd, SET_NONBLOCKING);
    }

    return(newfd);
}

int TcpAccept(int listenFd)
{
    int                 i, acceptfd;
    char                addrbuf[INET6_ADDRSTRLEN];
    short               from_port = 0;
    const char          *from_addr = NULL;    
    unsigned int        clntlen;
    sockaddr_storage_t  from;
    struct sockaddr_in  *from_sin;
    struct sockaddr_in6 *from_sin6;

    clntlen = sizeof(sockaddr_storage_t);

    if (listenFd <= 0) {
        xprint(FMS_HST | FMS_ERROR, "S-TCP TcpAccept(listenFd=%d) listenFd <= 0 \n", listenFd);
        return(-1);
    }

    if ((acceptfd = accept(listenFd, (struct sockaddr *)&from, &clntlen)) < 0) {
        xprint(FMS_HST | FMS_ERROR, "S-TCP accept (listenFd=%d, reason=%s) \n", listenFd, strerror(errno));
        return(-1);
    }

    switch (from.sa.sa_family) {
        case AF_INET:
            from_sin = (struct sockaddr_in *)&from.sa;
            from_addr = inet_ntop(AF_INET, &from_sin->sin_addr, addrbuf, INET_ADDRSTRLEN);
            from_port = from_sin->sin_port;        
            xprint(FMS_HST | FMS_LOOKS,
                    "|-------->| accept ipv4=%s:(c-port=%d)(listenfd=%d,acceptfd=%d)\n",
                    from_addr, NBO_NTOHS(from_port), listenFd, acceptfd);
            break;
        case AF_INET6:
            from_sin6 = (struct sockaddr_in6 *)&from;
            from_addr = inet_ntop(AF_INET6, &from_sin6->sin6_addr, addrbuf, INET6_ADDRSTRLEN);
            from_port = from_sin6->sin6_port;
            xprint(FMS_HST | FMS_LOOKS,
                    "|-------->| accept ipv6=%s:(c-port=%d)(listenfd=%d,acceptfd=%d)\n",
                    from_addr, NBO_NTOHS(from_port), listenFd, acceptfd);
            break;
    }

#if 0
    for (i = 0; i < MAX_LISTENQ; i++) {
        if (inet_addr(from_addr) == alist[i].sysinfo.systemaddr) {
            if (alist[i].AcceptFD > 0) {

#if 0
                if ((alist[i].sysinfo.port > 0) && 
                    ((alist[i].sysinfo.systemaddr == inet_addr("192.168.10.28")) || 
                     (alist[i].sysinfo.systemaddr == inet_addr("10.12.220.28")))) {
                    continue;
                }
#endif
                FD_CLR(alist[i].AcceptFD, &allset);
                close(alist[i].AcceptFD);
                alist[i].AcceptFD = -1;
            }
            alist[i].sysinfo.port = NBO_NTOHS(from_port);
            alist[i].AcceptFD = acceptfd;
            xprint(FMS_HST | FMS_INFO1,
                    "+         | accept FD(%d) : idx(%d) \n", alist[i].AcceptFD, i);
            break;
        }
    }
#else
#if 0
    /* 20140707, by jeon */
    /* 1. check address & re-set alist */
    for (i = 0; i < MAX_LISTENQ; i++) {
        if (inet_addr(from_addr) == alist[i].sysinfo.systemaddr) {
            if (alist[i].AcceptFD > 0) {
                FD_CLR(alist[i].AcceptFD, &allset);
                close(alist[i].AcceptFD);
                alist[i].AcceptFD = -1;
            }
            alist[i].sysinfo.port = NBO_NTOHS(from_port);
            alist[i].AcceptFD = acceptfd;

            f_flag = 1;

            xprint(FMS_HST | FMS_INFO1,
                        "+1.         | accept FD(%d) : idx(%d) \n", alist[i].AcceptFD, i);
            break;
        }
    }
#endif

    /* 2. Not find address --> set alist */
    for (i = 0; i < MAX_LISTENQ; i++) {
        if (alist[i].sysinfo.port == 0) {

            /* 20140704, by jeon */
            alist[i].sysinfo.systemidx  = i + 1;
            alist[i].sysinfo.port       = NBO_NTOHS(from_port);
            alist[i].AcceptFD           = acceptfd;
#if 0
            /* 20140723, by jeon */
            alist[i].sysinfo.systemaddr = inet_addr(from_addr);
            sprintf(alist[i].sysinfo.systemname, "VM-%s", addr2ntoa(alist[i].sysinfo.systemaddr));
#endif

            /* Host Conf (vmc_system_t), 20140707, by jeon */
            hostConf.vmc_system[i].systemidx  = i + 1;
#if 0
            /* 20140723, by jeon */
            hostConf.vmc_system[i].systemaddr = inet_addr(from_addr);
            sprintf(hostConf.vmc_system[i].systemname, "%s", alist[i].sysinfo.systemname);
#endif

            xprint(FMS_HST | FMS_INFO1,
                        "+0.         | accept FD(%d) : idx(%d) \n", alist[i].AcceptFD, i);
            break;
        }
    }
#endif

    if (i == MAX_LISTENQ) { 
        xprint(FMS_HST | FMS_ERROR, "connect addr have not config list.(%s) \n", from_addr);
        close(acceptfd);
    }

    return(i);
}

/* TCP BEFORE SEND  CHECK AVAILABLE */
inline int send_extendedPoll(int fd, int time, fd_set *sendFdSet)
{
    int            ret;
    struct timeval timeout;
    struct timeval *to;

    if (time < 0) {
        to = NULL;
    } else {
        to = &timeout;
        timeout.tv_sec  = time / 1000;
        timeout.tv_usec = (time % 1000) * 1000;
    }

    FD_ZERO(sendFdSet);
    if (fd == POLL_FD_UNUSED) {
        errno = EBADF;
        ret = -1;
    } else  {
        FD_SET(fd, sendFdSet);
        ret = select(fd + 1, (fd_set *)NULL, sendFdSet, (fd_set *)NULL, to);
        if (ret < 0) {
            xprint(FMS_HST | FMS_ERROR, "send select error \n");

        }
    }

    return(ret);
}

/* TCP BEFORE RECV  CHECK AVAILABLE */
inline int recv_extendedPoll(int time, fd_set *readFdSet)
{
    struct timeval timeout;
    struct timeval *to;
    struct timespec ts;

    int maxfd=0;
    int ret;

    if (time < 0) {
        to = NULL;
    } else {
        to = &timeout;
        timeout.tv_sec  = time / 1000;
        timeout.tv_usec = (time % 1000) * 1000;
    }

    maxfd = compareMaxfd(0);
    if (0 >= maxfd) { 
        ts.tv_sec  = 0;
        ts.tv_nsec = 10000000;
        nanosleep(&ts, NULL); 
        ret = 0;
    } else {
        ret = select(maxfd + 1, readFdSet, (fd_set *)NULL, (fd_set *)NULL, to);
        if (ret < 0) {
            xprint(FMS_HST | FMS_ERROR, "recv select error \n");
        }
    }

    return(ret);
}

int TcpSend(int accept_idx, int fd, unsigned char *SndMsg, int nbytes)
{
    int total_len = -1, result;
    int nSend=0, numCnt_0=0, numCnt_1=0, retry=TCP_SND_RETRY;
    int s_okFlag=0, p_okFlag=0;
    int l_error=0;
    fd_set sendSet;
    unsigned char *g_sBuf = SndMsg;

    int tnSend = 0;
    int nByte  = nbytes;
    unsigned short pkt_len = nbytes;

    if (fd <= 0 || nbytes <= 0) {
        xprint(FMS_HST | FMS_ERROR, "|---XXX-->| TcpSend (fd=%d, nbytes=%d) \n",
                fd, nbytes);
        return(-1);
    }

    /* tpkt hdr */
    pkt_len = NBO_HTONS(pkt_len);
    g_sBuf[0] = TCP_VERSION;
    g_sBuf[1] = 0x00;
    g_sBuf[2] = pkt_len >> 8;
    g_sBuf[3] = pkt_len & 0xFF;

#if 0 /* icecom_print */
    fprintf(stderr, " ############# TcpSend : len = %d\n", nbytes);
    fprintf(stderr, " ############# TcpSend : tpkt = %x %x %x %x\n",
            g_sBuf[0], g_sBuf[1], g_sBuf[2], g_sBuf[3]);
    fprintf(stderr, " ############# TcpSend : tpkt = %x %x %x %x\n",
            g_sBuf[4], g_sBuf[5], g_sBuf[6], g_sBuf[7]);
    fprintf(stderr, " ############# TcpSend : tpkt = %x %x %x %x\n",
            g_sBuf[8], g_sBuf[9], g_sBuf[10], g_sBuf[11]);
    fprintf(stderr, " ############# TcpSend : tpkt = %x %x %x %x\n",
            g_sBuf[12], g_sBuf[13], g_sBuf[14], g_sBuf[15]);
#endif

    /* select send fd buf available */
s_again :
    result = send_extendedPoll(fd, 100, &sendSet);
    l_error = errno;

    if (result == -1) {
        if (l_error == EINTR) {
            goto s_again;
        } else if (l_error == EBADF || l_error == EPIPE || l_error == EIO) {
            s_okFlag = 0;
        } else {
            xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, reason=%s) return \n",
                    fd, strerror(l_error));
            s_okFlag = -1;
            return(total_len);
        }
        xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, reason=%s) \n",
                fd, strerror(l_error));
    } else if (result == 0) {
        retry--;
        xprint(FMS_HST | FMS_WARNING, "|---WWW-->| send_extendedPoll (fd=%02d, retry=%d/%d) timeout \n",
                fd, retry, TCP_SND_RETRY); 

        if (retry) {
            goto s_again;
        }

        s_okFlag = -1;
        xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, retry=%d/%d) timeout \n",
                fd, retry, TCP_SND_RETRY);
        return(total_len);
    } else {
        s_okFlag = 1;
        if (!(FD_ISSET(fd, &sendSet))) {
            xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send (fd=%02d) !(FD_ISSET(fd, &sendSet) return \n", fd);
            s_okFlag = -1;
            return(total_len);
        }

        /*send fd */
        while (tnSend < nByte) {
p_again:
            if ((nByte - tnSend) <= 0) {
                xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send (fd=%02d) (nbytes - tnSend) <= 0 \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte);
                break;
            }

            if ((nSend = send(fd, (void *)((unsigned char*)&g_sBuf[0]+tnSend), nByte-tnSend, 0)) < 0) {
                l_error = errno;
                if (errno == EINTR || errno == EAGAIN) {
                    numCnt_0++;
                    goto p_again;
                }
                xprint(FMS_HST | FMS_ERROR, "E: |---XXX-->| send (fd=%02d) !(EINTR || EAGAIN) \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:             nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
                break;
            } else if(nSend == 0) {
                l_error = errno;
                xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send (fd=%02d) nSend == 0 \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
                break;
            }

#if 0 /* icecom_print */
            fprintf(stderr, " ############# TcpSend : nSend = %d\n", nSend);
            xprint(FMS_HST | FMS_LOOKS, "+--------------------------------------- \n");
            xprint(FMS_HST | FMS_LOOKS, "|=====>| real TcpSend : nSend = %d\n", nSend);
            xprint(FMS_HST | FMS_LOOKS, "+--------------------------------------- \n");
#endif

            l_error = errno;
            tnSend += nSend;
            numCnt_1++;
            p_okFlag = 1;
        }

        if (tnSend != nByte || nSend <= 0) {
            if (tnSend != nByte) {
                p_okFlag = -2;
            } else {
                p_okFlag = nSend;
            }
            xprint(FMS_HST | FMS_ERROR, "|---XXX-->| send (fd=%02d) abnormal \n", fd);
            xprint(FMS_HST | FMS_ERROR,
                    "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                    nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
        }
    }

    if (s_okFlag == 1 && p_okFlag == 1) {
        total_len = tnSend ;
        if ( total_len < -1) {
            total_len = -1;
        }

#if 0 /* for debuging....*/
        xprint(FMS_HST | FMS_INFO3, "|-------->| send (fd=%02d) total_len=%d \n", fd, total_len);
        xdump(FMS_HST | FMS_LOOKS, (unsigned char *)g_sBuf, total_len, "TcpSend ");
#endif
    } else {
        xprint(FMS_HST | FMS_ERROR,
                "|---XXX-->| send disconnection!! (ip=%s:%d, fd=%4d(%d), reason=%s, len=%d) [fd=%s(System_ID%d)]\n",
                tlink.addr, tlink.port,
                (tlink.mode == LINK_MODE_SERVER) ? alist[accept_idx].AcceptFD : tlink.ListenFD,
                accept_idx + 1, strerror(l_error), nSend, __func__, __LINE__);

        if (tlink.mode == LINK_MODE_SERVER) {
            if (alist[accept_idx].AcceptFD > 0) {
                FD_CLR(alist[accept_idx].AcceptFD, &allset);
                close(alist[accept_idx].AcceptFD);
                alist[accept_idx].sysinfo.port = 0;
                alist[accept_idx].AcceptFD = -1;

                /* 20140718, by jeon */
                memset(&hostConf.vmc_system[accept_idx], 0x00, sizeof(vmc_system_t));
            }
        } else if (tlink.mode == LINK_MODE_CLIENT) {
        }
        TcpInfo();
    }

    return(total_len);
}

int TcpRecv(int accept_idx, int fd, unsigned char *rBuf)
{
    int nRecv=0, tnRecv=0;
    unsigned char tpktHeader[TPKT_HEADER_SIZE];
    int numCnt_0=0, numCnt_1=0;
    int h_okFlag=0, b_okFlag=0;
    int l_error=0;

    int nByte = 0;
    unsigned short datasize = 0;

    /* TPKT Header message receive */
    while (tnRecv < TPKT_HEADER_SIZE) {
h_again:
        if ((TPKT_HEADER_SIZE - tnRecv) <= 0) {
#if 0 /* by icecom */
            xprint(FMS_HST | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) (TPKT_HEADER_SIZE - tnRecv) <= 0\n", fd);
            xprint(FMS_HST | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, TPKT_HEADER_SIZE=%02d \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, TPKT_HEADER_SIZE);
#else
            xprint(FMS_HST | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) (TPKT_HEADER_SIZE - tnRecv) <= 0 (line:%d)\n", fd, __LINE__);
#endif
            break;
        }

        if ((nRecv = recv(fd, (void *)((unsigned char *)tpktHeader+tnRecv), TPKT_HEADER_SIZE - tnRecv, 0)) < 0) {
            l_error = errno;
            if (errno == EINTR || errno == EAGAIN ) {
                numCnt_0++;
                goto h_again;
            }

#if 0 /* by icecom */
            xprint(FMS_HST | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) !(EINTR || EAGAIN) \n", fd);
            xprint(FMS_HST | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
#else
            xprint(FMS_HST | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) !(EINTR || EAGAIN) (line:%d)\n", fd, __LINE__);
#endif
            break;
        } else if(nRecv == 0) {
            l_error = errno;
#if 0 /* by icecom */
            xprint(FMS_HST | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) nRecv == %d \n", fd, nRecv);
            xprint(FMS_HST | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
#else
            xprint(FMS_HST | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) nRecv == %d (line:%d)\n", fd, nRecv, __LINE__);
#endif
            break;
        }

        l_error = errno;
        tnRecv += nRecv;
        numCnt_1++;
        h_okFlag = 1;
    }

#if 0 /* icecom_print */
        fprintf(stderr, "1. recv =================== tnRecv = %d, nByte = %d\n", tnRecv, nByte);
        fprintf(stderr, "2. recv +=================== tpktHeader = %x %x %x %x\n", tpktHeader[0], tpktHeader[1], tpktHeader[2], tpktHeader[3]);
#endif
        
    if ((tnRecv != TPKT_HEADER_SIZE) || (nRecv <= 0)) {
        if (tnRecv != TPKT_HEADER_SIZE) {
            h_okFlag = -2;
        } else {
            h_okFlag = nRecv;
        }
#if 0 /* by icecom */
        xprint(FMS_HST | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) abnormal \n", fd);
        xprint(FMS_HST | FMS_ERROR,
                "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
#else
        xprint(FMS_HST | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) abnormal (line:%d) reason=%s\n", 
                                    fd, __LINE__, strerror(l_error));
#endif
    } else {
        /* PAYLOAD message receive */
        nRecv = numCnt_0 = numCnt_1 = 0;
        tnRecv = TPKT_HEADER_SIZE;
        datasize = (tpktHeader[2] << 8) + tpktHeader[3];
        datasize = NBO_NTOHS(datasize);
        nByte = datasize;
#if 0 /* icecom_print */
        fprintf(stderr, "3. recv  =================== tnRecv = %d, nByte = %d\n", tnRecv, nByte);
        fprintf(stderr, "4. recv  =================== tnRecv = %d, nByte = %d\n", tnRecv, nByte);
#endif
        while (tnRecv < nByte) {
b_again:
            if ((nByte - tnRecv) <= 0) {
#if 0 /* by icecom */
                xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) (nByte - tnRecv) <= 0 \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte);
#else
                xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) (nByte - tnRecv) <= 0 (line:%d)\n", fd, __LINE__);
#endif
                break;
            }

            if ((nRecv = recv(fd, (void *)(rBuf+tnRecv), nByte - tnRecv, 0)) < 0) {
                l_error = errno;
                if ((errno == EINTR) || (errno == EAGAIN)) {
                    numCnt_0++;
                    goto b_again;
                }
#if 0 /* by icecom */
                xprint(FMS_HST | FMS_ERROR,
                        "|<--XXX---| B-recv (fd=%02d) !(EINTR || EAGAIN) \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
#else
                xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) !(EINTR || EAGAIN) (line:%d)\n", fd, __LINE__);
#endif
                break;
            } else if (nRecv == 0) {
                l_error = errno;
#if 0 /* by icecom */
                xprint(FMS_HST | FMS_ERROR,
                        "|<--XXX---| B-recv (fd=%02d) nRecv == 0 \n", fd);
                xprint(FMS_HST | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
#else
                xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) nRecv == 0 (line:%d)\n", fd, __LINE__);
#endif
                break;
            }
            l_error = errno;
            tnRecv += nRecv;
            numCnt_1++;
            b_okFlag = 1;
        }

        if ((tnRecv != nByte) || (nRecv <= 0)) {
            if (tnRecv != nByte) {
                b_okFlag = -2;
            } else {
                b_okFlag = nRecv;
            }
#if 0 /* by icecom */
            xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) abnormal \n", fd);
            xprint(FMS_HST | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
#else
            xprint(FMS_HST | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) abnormal (line:%d)\n", fd, __LINE__);
#endif
        }
    }
#if 0 /* icecom_print */
    fprintf(stderr, "5. recv  =================== tnRecv = %d, nByte = %d\n", tnRecv, nByte);
#endif

    if (!((h_okFlag == 1) && (b_okFlag == 1))) {
#if 0 /* by icecom */
        xprint(FMS_HST | FMS_ERROR,
                "|<--XXX---| recv disconnection !! (ip=%s:%d, idx=%d, fd=%4d(%d), reason=%s, len=%d) [%s(%d)] \n",
                tlink.addr, tlink.port, accept_idx, 
                (tlink.mode == LINK_MODE_SERVER) ? alist[accept_idx].AcceptFD : tlink.ListenFD,
                accept_idx, strerror(l_error), nRecv, __func__, __LINE__);
#else
        xprint(FMS_HST | FMS_ERROR,
                "|<--XXX---| recv disconnection !! (ip=%s:%d, fd=%d)\n",
                tlink.addr, tlink.port,
                (tlink.mode == LINK_MODE_SERVER) ? alist[accept_idx].AcceptFD : tlink.ListenFD);
#endif
        if (tlink.mode == LINK_MODE_SERVER) {
            int i;
            if (alist[accept_idx].AcceptFD > 0) {
                FD_CLR(alist[accept_idx].AcceptFD, &allset);
                close(alist[accept_idx].AcceptFD);
                alist[accept_idx].sysinfo.port = 0;
                alist[accept_idx].AcceptFD = -1;

                /* 20140814, by jeon */
                for (i = 0; i < MAX_SYSTEM_NUM; i++) {
                    if (stateinfo[i].vm_ip == hostConf.vmc_system[accept_idx].systemaddr) {
                        xprint(FMS_HST | FMS_INFO5, "1. i : %d :: vm ip : %s\n", i, addr2ntoa(stateinfo[i].vm_ip));
                        pthread_mutex_lock(&StateMutex);
                        memset(&stateinfo[i], 0x00, sizeof(vmc_stateinfo_t));
                        memcpy(&stateinfo[i].pstate[0], &basePState[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);
                        xprint(FMS_HST | FMS_INFO5, "2. i : %d :: vm ip : %s\n", i, addr2ntoa(stateinfo[i].vm_ip));
                        pthread_mutex_unlock(&StateMutex);
                        break;
                    }
                }

                /* 20140718, by jeon */
                memset(&hostConf.vmc_system[accept_idx], 0x00, sizeof(vmc_system_t));
            }
        } else if (tlink.mode == LINK_MODE_CLIENT) {
        }
        TcpInfo();
    } else {
#if 0 /* for debuging */
        xprint(FMS_HST | FMS_INFO3, "|<--------| recv (fd=%02d) total_len=%d \n", fd, nRecv);
        xdump(FMS_HST | FMS_LOOKS, rBuf, nRecv, "TcpRecv ");
#endif
    }

    return(nRecv);
}

void WriteProcInfo(void)
{
    FILE *fp;
    int i, count = 0, real_pp=0;
    char path[512];
    struct in_addr in;

    sprintf(path, "%s/TcpdInfo", LogPath);
    if ((fp =fopen(path, "w")) == (FILE *)NULL) {
        xprint(FMS_HST | FMS_FATAL, "fopen error(%s) at %s\n", strerror(errno), __func__);
        return;
    }

    fprintf(fp, "\n");
    fprintf(fp, "==========================================================\n");
    fprintf(fp, "            CFM TCPD Process Information              \n");
    fprintf(fp, "----------------------------------------------------------\n");
    fprintf(fp, " Link Info  Family      : Link IP Version 4\n");
    fprintf(fp, "            listen IP   : %s\n", tlink.addr);
    fprintf(fp, "            listen port : %d\n", tlink.port);
    fprintf(fp, "            listen FD   : %d\n", tlink.ListenFD);
    fprintf(fp, "\n");
    fprintf(fp, "Index  SystemID    Client IP        port    FD  Info\n");
    fprintf(fp, "----------------------------------------------------------\n");
    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
#if 0
        if (alist[i].sysinfo.systemaddr == 0) {
#else
        /* 20140723, by jeon */
        if (alist[i].sysinfo.port == 0) {
#endif
            continue;
        } else {
            real_pp++;
        }

        in.s_addr = alist[i].sysinfo.systemaddr;
        fprintf(fp, " %3d      %4u    %-15s %6hu   %2d   %s\n",
                alist[i].sysinfo.systemidx,
                alist[i].sysinfo.systemid,
                inet_ntoa(in),
                alist[i].sysinfo.port,
                alist[i].AcceptFD, alist[i].sysinfo.systemname);
        if (alist[i].AcceptFD > 0) {
            count++;
        }
    }
    fprintf(fp, "==========================================================\n");
    fprintf(fp, " * Last Update Time : %s\n", time_stamp());
    fprintf(fp, " * Client System Running(%d), Disconnect(%d)\n", count, real_pp - count);
    fprintf(fp, "\n");
    fclose(fp);

    return;
}

void TcpInfo(void)
{
    int i, run = 0, down = 0;
    struct in_addr in;
    int real_pp=0;

    xprint(FMS_HST | FMS_INFO1, "\n");
    xprint(FMS_HST | FMS_INFO1, "*------------------------------------------------------------\n");
    xprint(FMS_HST | FMS_INFO1, "* CHANGE |-------->| TCP SOCKET STATUS                       \n");
    xprint(FMS_HST | FMS_INFO1, "*------------------------------------------------------------\n");
    xprint(FMS_HST | FMS_INFO1, "* Index   IP Address      lport / rport    AcceptFD   alias\n");
    xprint(FMS_HST | FMS_INFO1, "*------------------------------------------------------------\n");

    if (tlink.mode == LINK_MODE_SERVER) {
        for (i = 0; i < MAX_LISTENQ; i++) {
#if 0
            if (alist[i].sysinfo.systemaddr == 0) {
#else
            /* 20140723, by jeon */
            if (alist[i].sysinfo.port == 0) {
#endif
                continue;
            } else {
                real_pp++;
            }

            in.s_addr = alist[i].sysinfo.systemaddr;
            xprint(FMS_HST | FMS_INFO1, "*  %3d   %-15s  %d / %5d       %2d     %s\n",
                    (i+1), inet_ntoa(in), tlink.port, alist[i].sysinfo.port, alist[i].AcceptFD, alist[i].sysinfo.systemname);

            /* add by icecom 080905 */
            if (alist[i].AcceptFD < 0) {
                down++;
            } else {
                run++;
            }
        }
    } else if (tlink.mode == LINK_MODE_CLIENT) {
        xprint(FMS_HST | FMS_INFO1, "* (ip=%s) (port=%d) (fd=%d) \n",
                tlink.addr, tlink.port, tlink.ListenFD);
    }

    xprint(FMS_HST | FMS_INFO1, "*------------------------------------------------------------\n");
    xprint(FMS_HST | FMS_INFO1, " * Client System Running(%d), Disconnect(%d)\n", run, down);

    WriteProcInfo();
}
