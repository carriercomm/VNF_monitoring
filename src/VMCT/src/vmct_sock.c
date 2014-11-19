/* -------------------------------------------------------------------------
 * File Name   : vmct_sock.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP-VMCT tcp fn.
 * History     : 13/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"
#include "timer.h"

#include "vmct_main.h"
#include "vmct_conf.h"
#include "vmct_task.h"
#include "vmct_sock.h"


int MaxFD;
fd_set rsetHOST, allsetHOST;
LinkInfo_t tlinkHOST;              /* Tcp Link Info */
AcceptList_t alist[MAX_LISTENQ];  /* Accept List (Not Used) */
TmrSyncStack_t tmrsync[MAX_TMR_NUM+1];

/* TCP NON-BLOCK MODE SETTING */
static int setNonblocking(int fd, int mode)
{
    int n, flags;

    if (mode == SET_NONBLOCKING) {
        if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
            xprint(FMS_VMC | FMS_ERROR, "fcntl(F_GETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
                    fd, mode, strerror(errno), __func__, __LINE__);
            return(-1);
        }

        if (-1 == (n = fcntl(fd, F_SETFL, flags | O_NONBLOCK))) {
            xprint(FMS_VMC | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
                    fd, mode, strerror(errno), __func__, __LINE__);
            return(-1);
        }
    } else {
        if (-1 == (flags = fcntl(fd, F_SETFL, 0))) {
            xprint(FMS_VMC | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, mode=%d, reason=%s [%s(%d)]\n",
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
        xprint(FMS_VMC | FMS_ERROR, "fcntl(F_GETFL) : fd=%d, reason=%s [%s(%d)]\n",
                fd, strerror(errno), __func__, __LINE__);
        return(-1);
    }

    if (-1 == (n = fcntl(fd, F_SETFL, flags | O_NONBLOCK))) {
        xprint(FMS_VMC | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, reason=%s [%s(%d)]\n",
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
                xprint(FMS_VMC | FMS_ERROR, "connect error :: (%s)\n", strerror(errno));
                return(-1);
            }
        }
    } else {
        return(-1);
    }

done:
    if (-1 == (n = fcntl(fd, F_SETFL, flags))) {
        xprint(FMS_VMC | FMS_ERROR, "fcntl(F_SETFL) : fd=%d, reason=%s [%s(%d)]\n",
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
            xprint(FMS_VMC | FMS_ERROR, "send select error \n");

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
            xprint(FMS_VMC | FMS_ERROR, "recv select error \n");

        }
    }

    return(ret);
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
            xprint(FMS_VMC | FMS_ERROR, "C-TCP socket() ip=%s:%d, reason=%s\n",
                    link->addr, link->port, strerror(errno));
            return(newfd);
        }

        /* Set SO_LINGER on socket */
        lngr.l_onoff  = 1;
        lngr.l_linger = 0;
        if (setsockopt(newfd, SOL_SOCKET, SO_LINGER, (char *)&lngr, sizeof(lngr)) < 0) {
            xprint(FMS_VMC | FMS_ERROR, "C-TCP setsockopt(SO_LINGER) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-2);
        }

        /* Set SO_REUSEADDR on socket */
        reuse = 1;
        if (setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1) {
            xprint(FMS_VMC | FMS_ERROR, "C-TCP setsockopt(SO_REUSEADDR) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-3);
        }

        /** SO_SNDBUF : socket snd buf */
        {
            int sndbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int))) == -1) {
                xprint(FMS_VMC | FMS_ERROR,
                        "C-TCP setsockopt(SO_SNDBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s\n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-1);
            }
        }

        /** SO_RCVBUF : socket rcv buf */
        {
            int rcvbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int))) == -1) {
                xprint(FMS_VMC | FMS_ERROR,
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
            xprint(FMS_VMC | FMS_WARNING, "|---XXX-->| connection ip=%s:%d(%s)\n",
                    link->addr, link->port, strerror(errno));

        } else if (ret == 0) {
            xprint(FMS_VMC | FMS_LOOKS, "|-------->| connection OK ip=%s:%d(fd=%d).\n",
                    link->addr, link->port, newfd);
        }

    } else if (link->mode == LINK_MODE_SERVER) {
        if ((newfd = socket(link->sockaddr_storage.sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            xprint(FMS_VMC | FMS_ERROR, "S-TCP socket() ip=%s:%d, reason=%s\n",
                    link->addr, link->port, strerror(errno));
            return(newfd);
        }

        /* Set SO_LINGER on socket */
        lngr.l_onoff  = 1;
        lngr.l_linger = 0;
        if (setsockopt(newfd, SOL_SOCKET, SO_LINGER, (char *)&lngr, sizeof(lngr)) < 0) {
            xprint(FMS_VMC | FMS_ERROR, "S-TCP setsockopt(SO_LINGER) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-2);
        }

        /* Set SO_REUSEADDR on socket */
        reuse = 1;
        if (setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1) {
            xprint(FMS_VMC | FMS_ERROR, "S-TCP setsockopt(SO_REUSEADDR) ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-3);
        }

        /** SO_SNDBUF : socket snd buf */
        {
            int sndbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int))) == -1) {
                xprint(FMS_VMC | FMS_ERROR,
                        "S-TCP setsockopt(SO_SNDBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s \n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-3);
            }
        }

        /** SO_RCVBUF : socket rcv buf */
        {
            int rcvbuf = 1024 * 40;
            if ((error = setsockopt(newfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int))) == -1) {
                xprint(FMS_VMC | FMS_ERROR,
                        "S-TCP setsockopt(SO_RCVBUF=1024 X 40) ip=%s:%d, fd=%d, reason=%s \n",
                        link->addr, link->port, newfd, strerror(errno));
                return(-3);
            }
        }

        /** binding */
        if (bind(newfd, (struct sockaddr *)&link->sockaddr_storage, sizeof(link->sockaddr_storage)) < 0) {
            xprint(FMS_VMC | FMS_ERROR, "S-TCP bind() ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-5);
        }

        /** listen */
        if (listen(newfd, MAX_LISTENQ) < 0) {
            xprint(FMS_VMC | FMS_ERROR, "S-TCP listen() ip=%s:%d, fd=%d, reason=%s \n",
                    link->addr, link->port, newfd, strerror(errno));
            return(-6);
        }
        xprint(FMS_VMC | FMS_LOOKS, "|-------->| listen ip=%s:%d(fd=%d)\n", link->addr, link->port, newfd);
        setNonblocking(newfd, SET_NONBLOCKING);
    }

    return(newfd);
}

int TcpAccept(int listenFd, int dsgidx)
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
        xprint(FMS_VMC | FMS_ERROR, "S-TCP TcpAccept(listenFd=%d) listenFd <= 0 \n", listenFd);
        return(-1);
    }

    if ((acceptfd = accept(listenFd, (struct sockaddr *)&from, &clntlen)) < 0) {
        xprint(FMS_VMC | FMS_ERROR, "S-TCP accept (listenFd=%d, reason=%s) \n", listenFd, strerror(errno));
        return(-1);
    }

    switch (from.sa.sa_family) {
        case AF_INET:
            from_sin = (struct sockaddr_in *)&from.sa;
            from_addr = inet_ntop(AF_INET, &from_sin->sin_addr, addrbuf, INET_ADDRSTRLEN);
            from_port = from_sin->sin_port;
            xprint(FMS_VMC | FMS_LOOKS,
                    "|-------->| accept ipv4=%s:(c-port=%d)(listenfd=%d,acceptfd=%d)\n",
                    from_addr, NBO_NTOHS(from_port), listenFd, acceptfd);
            break;
        case AF_INET6:
            from_sin6 = (struct sockaddr_in6 *)&from;
            from_addr = inet_ntop(AF_INET6, &from_sin6->sin6_addr, addrbuf, INET6_ADDRSTRLEN);
            from_port = from_sin6->sin6_port;
            xprint(FMS_VMC | FMS_LOOKS,
                    "|-------->| accept ipv6=%s:(c-port=%d)(listenfd=%d,acceptfd=%d)\n",
                    from_addr, NBO_NTOHS(from_port), listenFd, acceptfd);
            break;
    }

    for (i = 0; i < MAX_LISTENQ; i++) {
        if (inet_addr(from_addr) == alist[i].sysinfo.systemaddr) {
            alist[i].sysinfo.systemport = NBO_NTOHS(from_port);
            if (alist[i].AcceptFD > 0) {
                close(alist[i].AcceptFD);
            }
            alist[i].AcceptFD = acceptfd;
            break;
        }
    }

    if (i == MAX_LISTENQ) {
        xprint(FMS_VMC | FMS_ERROR, "connect addr have not config list.(%s) \n", from_addr);
        close(acceptfd);
    }

    return(i);

}

/* TCP SOCKET SEND  TPKT 4 Byte SUM SNDBUF */
int TcpSend(int accept_idx, int fd, unsigned char *SndMsg, int nbytes, int tcpmode, int dsgidx)
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

    /* Tcp Msg Size is not Check */
    if (fd <= 0 || nbytes <= 0) {
        xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| TcpSend (fd=%d, nbytes=%d) \n", fd, nbytes);
        return(-1);
    }

    /* tpkt hdr */
    pkt_len = NBO_HTONS(pkt_len);
    g_sBuf[0] = TCP_VERSION;
    g_sBuf[1] = 0x00;
    g_sBuf[2] = pkt_len >> 8;
    g_sBuf[3] = pkt_len & 0xFF;

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
            xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, reason=%s) return \n",
                    fd, strerror(l_error));
            s_okFlag = -1;
            return(total_len);
        }
        xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, reason=%s) \n",
                fd, strerror(l_error));
    } else if (result == 0) {
        retry--;
        xprint(FMS_VMC | FMS_WARNING, "|---WWW-->| send_extendedPoll (fd=%02d, retry=%d/%d) timeout \n",
                fd, retry, TCP_SND_RETRY);

        if (retry) {
            goto s_again;
        }

        s_okFlag = -1;
        xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send_extendedPoll (fd=%02d, retry=%d/%d) timeout \n",
                fd, retry, TCP_SND_RETRY);
        return(total_len);
    } else {
        s_okFlag = 1;
        if (!(FD_ISSET(fd, &sendSet))) {
            xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send (fd=%02d) !(FD_ISSET(fd, &sendSet) return \n", fd);
            s_okFlag = -1;
            return(total_len);
        }

        /*send fd */
        while (tnSend < nByte) {
p_again:
            if ((nByte - tnSend) <= 0) {
                xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send (fd=%02d) (nbytes - tnSend) <= 0 \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte);
                break;
            }
#if 0
            xdump(FMS_VMC | FMS_LOOKS, (unsigned char*)&g_sBuf[0]+tnSend, nByte-tnSend, "TCPSend");
#endif
            if ((nSend = send(fd, (void *)((unsigned char*)&g_sBuf[0]+tnSend), nByte-tnSend, 0)) < 0) {
                l_error = errno;
                if (errno == EINTR || errno == EAGAIN) {
                    numCnt_0++;
                    goto p_again;
                }
                xprint(FMS_VMC | FMS_ERROR, "E: |---XXX-->| send (fd=%02d) !(EINTR || EAGAIN) \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:             nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
                break;
            } else if(nSend == 0) {
                l_error = errno;
                xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send (fd=%02d) nSend == 0 \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                        nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
                break;
            }

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
            xprint(FMS_VMC | FMS_ERROR, "|---XXX-->| send (fd=%02d) abnormal \n", fd);
            xprint(FMS_VMC | FMS_ERROR,
                    "+:          nSend=%04d, tnSend=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%02d, reason=%s \n",
                    nSend, tnSend, numCnt_0, numCnt_1, nByte, strerror(l_error));
        }
    }

    if (s_okFlag == 1 && p_okFlag == 1) {
        total_len = tnSend ;
        if ( total_len < -1) {
            total_len = -1;
        }

    } else {

        if (tcpmode == LINK_MODE_CLIENT) {

            xprint(FMS_VMC | FMS_ERROR,
                    "|---XXX-->| send disconnection!! (ip=%s:%d, fd=%4d, reason=%s, len=%d) [%s(%d)]\n",
                    tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD, strerror(l_error), nSend, __func__, __LINE__);

            if (tlinkHOST.ListenFD > 0) {
#ifndef __UTM__
                FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
#else
                FD_ZERO(&allsetHOST);
#endif
                close(tlinkHOST.ListenFD);
                tlinkHOST.ListenFD = -1;

                if (tlinkHOST.tmr_reconn.tmrId > 0) {
                    TcptmrStop(tlinkHOST.tmr_reconn.tmrId);
                }

                tlinkHOST.tmr_reconn.tmrId = TcptmrStart(tlinkHOST.tmr_reconn.tmrVal, TMR_TCP_RECONNECT_MSG, 0, 0);
            }

        } else if (tcpmode == LINK_MODE_SERVER) {

            xprint(FMS_VMC | FMS_ERROR,
                    "|---XXX-->| send disconnection!! (ip=%s:%d, fd=%4d, reason=%s, len=%d) [%s(%d)]\n",
                    tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD, strerror(l_error), nSend, __func__, __LINE__);

            if (alist[accept_idx].AcceptFD > 0) {
                FD_CLR(alist[accept_idx].AcceptFD, &allsetHOST);
                close(alist[accept_idx].AcceptFD);
                alist[accept_idx].sysinfo.systemport = 0;
                alist[accept_idx].AcceptFD = -1;
            }

        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s(%d) :: TCP Mode is UNKNOW[= %d] \n", __func__, __LINE__, tcpmode);
        }

        TcpInfo();
    }

    return(total_len);
}

/* TCP SOCKET RECV  TPKT 4 Byte DEL RCVBUF */
int TcpRecv(int accept_idx, int fd, unsigned char *rBuf, int tcpmode, int dsgidx)
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
            xprint(FMS_VMC | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) (TPKT_HEADER_SIZE - tnRecv) <= 0\n", fd);
            xprint(FMS_VMC | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, TPKT_HEADER_SIZE=%02d \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, TPKT_HEADER_SIZE);
            break;
        }

        if ((nRecv = recv(fd, (void *)((unsigned char *)tpktHeader+tnRecv), TPKT_HEADER_SIZE - tnRecv, 0)) < 0) {
            l_error = errno;
            if (errno == EINTR || errno == EAGAIN ) {
                numCnt_0++;
                goto h_again;
            }

            xprint(FMS_VMC | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) !(EINTR || EAGAIN) \n", fd);
            xprint(FMS_VMC | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
            break;
        } else if(nRecv == 0) {
            l_error = errno;
            xprint(FMS_VMC | FMS_ERROR,
                    "|<--XXX---| H-recv (fd=%02d) nRecv == %d \n", fd, nRecv);
            xprint(FMS_VMC | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
            break;
        }

        l_error = errno;
        tnRecv += nRecv;
        numCnt_1++;
        h_okFlag = 1;
    }

    if ((tnRecv != TPKT_HEADER_SIZE) || (nRecv <= 0)) {
        if (tnRecv != TPKT_HEADER_SIZE) {
            h_okFlag = -2;
        } else {
            h_okFlag = nRecv;
        }
        xprint(FMS_VMC | FMS_ERROR, "|<--XXX---| H-recv (fd=%02d) abnormal \n", fd);
        xprint(FMS_VMC | FMS_ERROR,
                "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, reason=%s \n",
                nRecv, tnRecv, numCnt_0, numCnt_1, strerror(l_error));
    } else {
        /* PAYLOAD message receive */
        nRecv = numCnt_0 = numCnt_1 = 0;
        tnRecv = TPKT_HEADER_SIZE;
        datasize = (tpktHeader[2] << 8) + tpktHeader[3];
        datasize = NBO_NTOHS(datasize);
        nByte = datasize;
        while (tnRecv < nByte) {
b_again:
            if ((nByte - tnRecv) <= 0) {
                xprint(FMS_VMC | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) (nByte - tnRecv) <= 0 \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte);
                break;
            }

            if ((nRecv = recv(fd, (void *)(rBuf+tnRecv), nByte - tnRecv, 0)) < 0) {
                l_error = errno;
                if ((errno == EINTR) || (errno == EAGAIN)) {
                    numCnt_0++;
                    goto b_again;
                }
                xprint(FMS_VMC | FMS_ERROR,
                        "|<--XXX---| B-recv (fd=%02d) !(EINTR || EAGAIN) \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
                break;
            } else if (nRecv == 0) {
                l_error = errno;
                xprint(FMS_VMC | FMS_ERROR,
                        "|<--XXX---| B-recv (fd=%02d) nRecv == 0 \n", fd);
                xprint(FMS_VMC | FMS_ERROR,
                        "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                        nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
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
            xprint(FMS_VMC | FMS_ERROR, "|<--XXX---| B-recv (fd=%02d) abnormal \n", fd);
            xprint(FMS_VMC | FMS_ERROR,
                    "+:          nRecv=%04d, tnRecv=%04d : numCnt_0=%04d, numCnt_1=%04d, nByte=%04d, reason=%s \n",
                    nRecv, tnRecv, numCnt_0, numCnt_1, nByte, strerror(l_error));
        }
    }

    if (!((h_okFlag == 1) && (b_okFlag == 1))) {

        if (tcpmode == LINK_MODE_CLIENT) {
            xprint(FMS_VMC | FMS_ERROR,
                "|<--XXX---| recv disconnection !! (ip=%s:%d, fd=%4d, reason=%s, len=%d) [%s(%d)] \n",
                    tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD,
                    strerror(l_error), nRecv, __func__, __LINE__);

            if (tlinkHOST.ListenFD > 0) {
#ifndef __UTM__
                FD_CLR(tlinkHOST.ListenFD, &allsetHOST);
#else
                FD_ZERO(&allsetHOST);
#endif
                close(tlinkHOST.ListenFD);
                tlinkHOST.ListenFD = -1;
                if (tlinkHOST.tmr_reconn.tmrId > 0) {
                    TcptmrStop(tlinkHOST.tmr_reconn.tmrId);
                }

                tlinkHOST.tmr_reconn.tmrId =
                    TcptmrStart(tlinkHOST.tmr_reconn.tmrVal, TMR_TCP_RECONNECT_MSG, 0, 0);
            }

        } else if (tcpmode == LINK_MODE_SERVER) {
            xprint(FMS_VMC | FMS_ERROR,
                "|<--XXX---| recv disconnection !! (ip=%s:%d, fd=%4d, reason=%s, len=%d) [%s(%d)] \n",
                    tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD,
                    strerror(l_error), nRecv, __func__, __LINE__);

            if (alist[accept_idx].AcceptFD > 0) {
                FD_CLR(alist[accept_idx].AcceptFD, &allsetHOST);
                close(alist[accept_idx].AcceptFD);
                alist[accept_idx].sysinfo.systemport = 0;
                alist[accept_idx].AcceptFD = -1;
            }

        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s(%d) :: TCP Mode is UNKNOW[= %d] \n", __func__, __LINE__, tcpmode);
        }

        TcpInfo();

    } else {

    }

    return(nRecv);
}

/* Client TCP */
void TcpInfo(void)
{
    int i;

    xprint(FMS_VMC | FMS_LOOKS, "*----------------------------------------------------------------------\n");
    xprint(FMS_VMC | FMS_LOOKS, "* CHANGE |-------->| TCP SOCKET STATUS                                 \n");
    xprint(FMS_VMC | FMS_LOOKS, "*----------------------------------------------------------------------\n");

    /* Client Mode */
    if (tlinkHOST.mode == LINK_MODE_CLIENT) {
        xprint(FMS_VMC | FMS_LOOKS, "* (ip=%s) (port=%d) (fd=%d) \n", tlinkHOST.addr, tlinkHOST.port, tlinkHOST.ListenFD);

    /* Server Mode */
    } else if (tlinkHOST.mode == LINK_MODE_SERVER) {
        for (i = 0; i < MAX_LISTENQ; i++) {
            xprint(FMS_VMC | FMS_LOOKS, "* (ip=%s) (port=%d/%d) (fd=%d) \n",
                    alist[i].sysinfo.systemaddr, tlinkHOST.port, alist[i].sysinfo.systemport, alist[i].AcceptFD);
        }
    }

    xprint(FMS_VMC | FMS_LOOKS, "*----------------------------------------------------------------------\n");
}

int TcptmrStart(unsigned int interval, unsigned int mType, int argc1, int argc2)
{
    int timer_id;

    timer_id = timer_start(interval, NULL, mType, argc1, argc2, TMR_MSGQUEUE);
    if (0 >= timer_id) {
        xprint(FMS_VMC | FMS_ERROR, "timer_start not OK, [t_idx=%d], interval=%d, mType=%d\n",
                timer_id, interval, mType);
    } else {
        tmrsync[timer_id].used = TMR_USED;
        tmrsync[timer_id].primId = mType;
        tmrsync[timer_id].argc1 = argc1;
        tmrsync[timer_id].argc2 = argc2;
        xprint(FMS_VMC | FMS_INFO5, "timer_start OK, [t_idx=%d] interval=%d, mType=%d\n",
                timer_id, interval, mType);
    }

    return(timer_id);
}

int TcptmrStop(int timer_id)
{
    if (-1 == timer_stop(timer_id, TMR_STACK_USED)) {
        xprint(FMS_VMC | FMS_WARNING, "timer_stop NOK :: timer_id=%d \n", timer_id);
        tmrsync[timer_id].used = TMR_NOT_USED;
        tmrsync[timer_id].primId = 0;
        tmrsync[timer_id].argc1 = 0;
        tmrsync[timer_id].argc2 = 0;
        return(-1);
    } else {
        xprint(FMS_VMC | FMS_INFO5, "timer_stop OK_ :: timer_id=%d \n", timer_id);
        tmrsync[timer_id].used = TMR_NOT_USED;
        tmrsync[timer_id].primId = 0;
        tmrsync[timer_id].argc1 = 0;
        tmrsync[timer_id].argc2 = 0;
    }

    return(0);
}

void TcpReconnect(int timerId, int param1, int param2, int param3)
{
    tlinkHOST.tmr_reconn.tmrId = 0;

    pthread_mutex_lock(&TcpHostMutex);
    tlinkHOST.ListenFD = TcpOpen(&tlinkHOST);

    if (tlinkHOST.ListenFD <= 0) {
        tlinkHOST.ListenFD = -1;
        if (tlinkHOST.mode == LINK_MODE_SERVER) {
            xprint(FMS_VMC | FMS_ERROR, "%s: S-TCP TcpOpen failed\n", __func__);
        } else if (tlinkHOST.mode == LINK_MODE_CLIENT) {
            if (tlinkHOST.tmr_reconn.tmrId > 0) {
                TcptmrStop(tlinkHOST.tmr_reconn.tmrId);
            }
            tlinkHOST.tmr_reconn.tmrId =
                TcptmrStart(tlinkHOST.tmr_reconn.tmrVal, TMR_TCP_RECONNECT_MSG, 0, 0);
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

    return;
}

int UdpOpen(int port)
{
    int                 sock, buf=0x8000;
    struct sockaddr_in  bindaddr;

    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_family      = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port        = htons(port);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "socket error(%s) at %s\n", strerror(errno), __FUNCTION__);
        return(-1);
    }

    if (bind(sock, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "bind error(%s) at %s(port:%d)\n", strerror(errno), __FUNCTION__, port);
        close(sock);
        return(-1);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&buf, sizeof(buf)) == -1) {
        xprint(FMS_VMC | FMS_FATAL, "setsockopt(SO_RCVBUF) error(%s) at %s\n", strerror(errno), __FUNCTION__);
        close(sock);
        return(-1);
    }

    xprint(FMS_VMC | FMS_INFO1, "%s :: Success [sock(%d), port(%d)] \n", __FUNCTION__, sock, port);
    return(sock);
}

int UdpRecv(int fd, int size, struct sockaddr_in *saddr, unsigned char *rBuf)
{
    ssize_t     nRecv;
    socklen_t   ssize;

    ssize = sizeof(struct sockaddr_in);
    nRecv = recvfrom(fd, rBuf, (size_t)size, 0, (struct sockaddr *)saddr, &ssize);
    if (nRecv < 0) {
        xprint(FMS_VMC | FMS_ERROR, "recvfrom error(%s) at %s\n", strerror(errno), __FUNCTION__);
    }

    return(nRecv);
}
