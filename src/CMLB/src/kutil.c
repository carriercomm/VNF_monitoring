/*
 -------------------------------------------------------------------------
  File Name       : kutil.c
  Author          : Hyo-Seok Kang
  Copyright       : Copyright (C) 2005 - 2007 KBell Inc.
  Descriptions    : 유틸리티 함수 

  History         : 05/08/17       Initial Coded
 -------------------------------------------------------------------------
*/

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>


#include "xprint.h"
#include "kutil.h"

/*
 밀리세컨드 단위의 sleep
*/
void nanoSleep(int msecs)
{
    struct timespec ts;

    ts.tv_sec  = (msecs / 1000);
    ts.tv_nsec = (msecs * 1000) * 1000 - 2000000;

    if ((msecs/1000) > 0) {
        /*usleep((msecs * 1000) - 9000);*/
        usleep((msecs * 1000) - 900);
    } else {
        nanosleep(&ts, NULL);
    }
}

/*
 현재 시간을 나타낸다.
*/
char *time_stamp(void)
{
    static char buf[64];
    struct tm ctime;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    /* 멀티 쓰레드인 경우 localtime을 사용하면 SIGSEV가 발생할 수 있어 localtime_r을 사용한다. */
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    sprintf(buf, "%d-%02d-%02d, %02d:%02d:%02d.%03d", ctime.tm_year + 1900,
                  ctime.tm_mon + 1, ctime.tm_mday, ctime.tm_hour, ctime.tm_min,
                  ctime.tm_sec, (int)(tv.tv_usec / 1000));

    return(buf);
}

/* DB insert를 위한 현재 시간, by jeon */
char *GetTimeStamp(void)
{
    static char buf[16];
    struct tm ctime;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    localtime_r((time_t *)&(tv.tv_sec), &ctime);
    sprintf(buf, "%04d%02d%02d%02d%02d%02d", ctime.tm_year + 1900,
                  ctime.tm_mon + 1, ctime.tm_mday, ctime.tm_hour, ctime.tm_min,
                  ctime.tm_sec);

    return(buf);
}

/* DB insert를 위한 시간, by jeon */
char *get_dateTime(unsigned long unix_usec, char *date)
{
    struct timeval tv;
    struct tm      ctime;

    tv.tv_sec = unix_usec;
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    sprintf(date, "%04d%02d%02d%02d%02d%02d",
                   ctime.tm_year + 1900, ctime.tm_mon + 1, ctime.tm_mday,
                   ctime.tm_hour, ctime.tm_min, ctime.tm_sec);

    return(date);
}

/* DB insert를 위한 현재 시간의 -5초 이전 시간, by jeon */
void init_insertTime(char *fivesec, char *fivemin) 
{
    int remain = 0;
    struct timeval tv;
    struct tm ctime;

    gettimeofday(&tv, NULL);
    localtime_r((time_t *)&(tv.tv_sec), &ctime);
    if (fivesec != NULL) {
        remain = ctime.tm_sec % 5;
        if (remain) {
            ctime.tm_sec -= remain;
        }
        sprintf(&fivesec[0], "%04d%02d%02d%02d%02d%02d", ctime.tm_year + 1900,
                          ctime.tm_mon + 1, ctime.tm_mday, ctime.tm_hour, ctime.tm_min,
                          ctime.tm_sec);
    }

    if (fivemin != NULL) {
        remain = ctime.tm_min % 5;
        if (remain) {
            ctime.tm_min -= remain;
        }
        sprintf(&fivemin[0], "%04d%02d%02d%02d%02d%02d", ctime.tm_year + 1900,
                          ctime.tm_mon + 1, ctime.tm_mday, ctime.tm_hour, ctime.tm_min,
                          ctime.tm_sec);
    }
    
    return;
}

int Get1MinTimeStamp(unsigned char *time)
{
    struct tm ctime;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    /* 멀티 쓰레드인 경우 localtime을 사용하면 SIGSEV가 발생할 수 있어 localtime_r을 사용한다. */
    /* 1분전 시간을 timestamp으로 입력한다, 20101214, by jeon */
#if 0
    tv.tv_sec -= 60;
#endif

    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    ctime.tm_sec = 0;
    tv.tv_usec   = 0;

    if (time != NULL) {
        sprintf((char *)time, "%d%02d%02d%02d%02d%02d", ctime.tm_year + 1900,
                      ctime.tm_mon + 1, ctime.tm_mday, ctime.tm_hour, ctime.tm_min,
                      ctime.tm_sec);
    }

    return(ctime.tm_min);
}

/* Search 5M STAT DB, 20111208, by jeon */
unsigned short GetDateDayWeek(int day, char *date)
{
    unsigned char ucL;
    unsigned short usMJD;
    unsigned short ucDayOfWeek;

    struct tm ctime;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    tv.tv_sec -= (86400 * day);

    localtime_r((time_t *)&(tv.tv_sec), &ctime);
    sprintf(date, "%04d%02d%02d", ctime.tm_year + 1900, ctime.tm_mon + 1, ctime.tm_mday);

    ctime.tm_year += 1900;
    ctime.tm_mon += 1;

    ucL = ((ctime.tm_mon == 1) || (ctime.tm_mon == 2))? 1: 0;
    usMJD = 14956
        + ctime.tm_mday
        + (unsigned short)(((ctime.tm_year-1900) - ucL) * 365.25)
        + (unsigned short)((ctime.tm_mon + 1 + ucL * 12) * 30.6001);

    /* 1 : 월요일, 2 : 화요일, ... 6 : 토요일, 7 : 일요일 */
    ucDayOfWeek = (unsigned short)((usMJD + 2) % 7 + 1);

    return(ucDayOfWeek);
}

/*
 현재 시간을 나타낸다.
*/
char *directory_time(void)
{
    static char buf[64];
    struct tm ctime;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    /* 멀티 쓰레드인 경우 localtime을 사용하면 SIGSEV가 발생할 수 있어 localtime_r을 사용한다. */
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    sprintf(buf, "%d.%02d.%02d", ctime.tm_year + 1900, ctime.tm_mon + 1, ctime.tm_mday);

    return(buf);
}

/*
 프로세스의 중복을 체크한다.
*/
int dupcheckbyname(char *name)
{
    FILE *fp;
    char cmd[512];
    int pid, ppid;

    if (name == NULL) {
        fprintf(stderr, "process name is null at check_process\n");
        return(-1);
    }

    ppid = getpid();

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "ps -ef | grep %s$ | grep -v grep | grep -v vi | awk '{print $2}'", name);

    if ((fp = popen(cmd, "r")) == NULL) {
        fprintf(stderr, "popen failed at check_process\n");
        return(-1);
    }

    while(fscanf(fp, "%d", &pid) == 1) {
        if (pid != ppid) {
            pclose(fp);
            return(-1);
        }
    }

    pclose(fp);

    return(0);
}

/*
 필요한 시그날을 세팅하고 불필요한 시그날은 무시한다.
*/
void setupSignal(void(*sigcb)(int signum))
{
    int i, sigAry[]={SIGQUIT, SIGINT, SIGTERM, SIGKILL};  

    for(i = 0; i < (sizeof(sigAry)/sizeof(int)); i++) {
        signal(sigAry[i], sigcb);
    }

    signal(SIGALRM, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);
}

/*
// 시그널을 무시한다.
*/
void ignoreSignal(int signum)
{
    signal(signum, SIG_IGN);
}

/*
// Returns the difference between gmt and local time in seconds.
// Use gmtime() and localtime() to keep things simple.
*/
int gmt2local(time_t t)
{
    register int dt, dir;
    register struct tm *gmt, *loc;
    struct tm sgmt;

    if (t == 0) {
        t = time(NULL);
    }
    gmt = &sgmt;
    *gmt = *gmtime(&t);
    loc = localtime(&t);
    dt = (loc->tm_hour - gmt->tm_hour) * 60 * 60 + (loc->tm_min - gmt->tm_min) * 60;

    /*
    // If the year or julian day is different, we span 00:00 GMT
    // and must add or subtract a day. Check the year first to
    // avoid problems when the julian day wraps.
    */
    dir = loc->tm_year - gmt->tm_year;
    if (dir == 0) {
        dir = loc->tm_yday - gmt->tm_yday;
    }
    dt += dir * 24 * 60 * 60;

    return(dt);
}


/*
   현재 날짜를 입력 받아 요일을 찾는 프로그램, by jeon
   윤년은 4년마다 +1, 100년마다-1 , 400년마다 +1
   요일은 총일수

    1 : 월요일
    2 : 화요일
    ...
    6 : 토요일
    7 : 일요일
*/
unsigned short FindDayOfWeek()
{
    struct tm ctime;
    struct timeval tv;

    unsigned char ucL;
    unsigned short usMJD;
    unsigned short ucDayOfWeek;

    gettimeofday(&tv, NULL);
    localtime_r((time_t *)&(tv.tv_sec), &ctime);

    ctime.tm_year += 1900;
    ctime.tm_mon += 1;
#if 0
    printf("year : %d \n", ctime.tm_year);
    printf("mon  : %02d \n", ctime.tm_mon);
    printf("day  : %0d \n", ctime.tm_mday);
#endif
    ucL = ((ctime.tm_mon == 1) || (ctime.tm_mon == 2))? 1: 0;
    usMJD = 14956
        + ctime.tm_mday
        + (unsigned short)(((ctime.tm_year-1900) - ucL) * 365.25)
        + (unsigned short)((ctime.tm_mon + 1 + ucL * 12) * 30.6001);

    ucDayOfWeek = (unsigned short)((usMJD + 2) % 7 + 1);

    return (ucDayOfWeek);
}

/* Binding 시 FD값을 이용하여 자신의 local ip를 가져오는 방법 */
unsigned int GetLocalIP(int fd)
{
    struct sockaddr_in sockAddr;
    int    nSockAddrLen, bResult;
    unsigned short socketPort;
    unsigned int LocalIPAddr;

    memset(&sockAddr, 0, sizeof(sockAddr));

    nSockAddrLen = sizeof(sockAddr);
    bResult = (getsockname(fd,(struct sockaddr*)&sockAddr,(socklen_t *) &nSockAddrLen) == 0);
    if(bResult)
    {
        socketPort  = ntohs(sockAddr.sin_port);
        LocalIPAddr = sockAddr.sin_addr.s_addr;
#if 0
        printf("port : %d \n", socketPort);
        printf("1. ip   : %u \n", sockAddr.sin_addr.s_addr);  /* 같은 값, 이것으로 전송함.*/
        printf("2. ip   : %u \n", LocalIPAddr);               /* 같은 값, 이것으로 전송함.*/
        printf("3. ip   : %s \n", inet_ntoa(sockAddr.sin_addr));
        printf("4. ip   : %d \n", inet_addr("192.168.1.224"));
        printf("5. ip   : %u \n", inet_addr("192.168.1.224"));
#endif
        xprint(FMS_CLB | FMS_INFO1, "%s :: ip= %s:%u (fd=%d) \n", 
                                    __func__, addr2ntoa(LocalIPAddr), socketPort, fd);

    } else {
        xprint(FMS_CLB | FMS_INFO5, "Not Find local IP address \n");
        return -1;
    }
    return LocalIPAddr;
}


/* unsigned long (8 byte) byte ordering, by jeon */
unsigned long NBO_HTONLL(unsigned long x)
{
    return (((unsigned long) htonl(x)) << 32) + htonl(x >> 32);
}

/* unsigned long (8 byte) byte ordering, by jeon */
unsigned long NBO_NTOHLL(unsigned long x)
{
    return (((unsigned long) ntohl(x)) << 32) + ntohl(x >> 32);
}

char *addr2str(unsigned int addr)
{
    static char address[32];

    sprintf(address, "%d.%d.%d.%d", (int)((addr >> 24) & 0xff),
                                    (int)((addr >> 16) & 0xff),
                                    (int)((addr >>  8) & 0xff),
                                    (int)((addr >>  0) & 0xff));

    return(address);
}

char *ipStr(unsigned int addr)
{
        static char address[32];

        sprintf(address, "%d.%d.%d.%d", (int)((addr >> 24) & 0xff),
                (int)((addr >> 16) & 0xff),
                (int)((addr >>  8) & 0xff),
                (int)((addr >>  0) & 0xff));

        return(address);
}

char *addr2ntoa(unsigned int addr)
{
    struct in_addr in;
    in.s_addr = addr;

    return(inet_ntoa(in));
}

int CmpNetmaskIp(unsigned int addr1, unsigned int addr2, unsigned int netmask)
{
    int ret = 0;
    unsigned int cmp1 = 0, cmp2 = 1;

    xprint(FMS_CLB | FMS_INFO1, "addr1  [= %s] \n", addr2str(addr1));
    xprint(FMS_CLB | FMS_INFO1, "addr2  [= %s] \n", addr2str(addr2));
    xprint(FMS_CLB | FMS_INFO1, "netmask[= %s] \n", addr2str(netmask));

    cmp1 = addr1 & netmask;
    cmp2 = addr2 & netmask;

    xprint(FMS_CLB | FMS_INFO1, "%s :: cmp1[= %u], cmp2[= %u]\n", __func__, cmp1, cmp2);

    if (cmp1 == cmp2) {
        ret = 1;
    }

    return (ret);
}

int proccheckbyname(char *name)
{
    int i = 0, c;
    FILE *fp;
    char cmd[512] , line[65535];

    if (name == NULL) {
        fprintf(stderr, "process name is null at check_process\n");
        return(-1);
    }

     memset(cmd, 0x00, sizeof(cmd));

     sprintf(cmd, "ps -ef | grep %s | grep -v grep | grep -v vi", name);

#if 0
     if (fp == NULL) {
         xprint(FMS_CLB | FMS_INFO1, "1. fp is NULL \n");
     }
#endif

     if ((fp = popen(cmd, "r")) == NULL) {
         fprintf(stderr, "popen failed at check_process\n");
         return(-1);
     }

     memset(line, 0x00, sizeof(line));
     while ((c = getc(fp)) != EOF) {
         line[i++] = c;
     }

     xprint(FMS_CLB | FMS_INFO5, "i = %d / %s \n", i, &line[0]);

#if 0
     while(fscanf(fp, "%d", &pid) == 1) {
         if (pid != ppid) {
             pclose(fp);
            return(-1);
        }
    }
#endif

    pclose(fp);
    return(i);
}


