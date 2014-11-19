/* -------------------------------------------------------------------------
 * File Name   : vmct_conf.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP-VMCT Configuration fn.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/msg.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"
#include "mqipc.h"
#include "timer.h"


#include "vmct_main.h"
#include "vmct_conf.h"
#include "vmct_sock.h"


int           TmrCmdQid;
vmct_config_t VmctConf;


/* 16진수, 10진수의 스트링값을 읽어 변환하는 함수 */
static unsigned int Isdigit(char *s, unsigned int *c)
{
    size_t i, size;

    size = strlen(s);

    if (strncasecmp(s, "0x", 2) == 0 || strncasecmp(s, "0X", 2) == 0) {    /* 16진수 확인 */
        for (i = 2; i < size; i++) {
            if (!isxdigit((int)s[i])) {
                return(0);
            }
        }
        sscanf(s, "%x", c);
    } else {                                /* 10진수 확인 */
        for (i = 0; i < size; i++) {
            if (!isdigit((int)s[i])) {
                return(0);
            }
        }
        sscanf(s, "%u", c);
    }

    return(1);
}

static void Gettoken(char *name, char *value)
{
    unsigned int d, retval;

    /* HOST TCP Cinfig sock Information, main link config */
    if (!strcmp(name, "HOST_ADDRESS")) {
        VmctConf.host_ip = inet_addr(value);
        strcpy(tlinkHOST.addr, value);
    }

    if (!strcmp(name, "HOST_PORT")) {
        retval = Isdigit(value, &d);
        if (retval) {
            VmctConf.host_port = d;
            tlinkHOST.port = VmctConf.host_port;
        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s(%d) : Check HOST TCP(sock port) File!\n", __func__, __LINE__);
        }
    }

    /* VMCT TCP Cinfig sock Information, main link config */
    if (!strcmp(name, "VMCT_ADDRESS")) {
        VmctConf.vmct_ip = inet_addr(value);
    }

    /* 20140903, by jeon */
    if (!strcmp(name, "VM_STATE_SEND_TIME")) {
        retval = Isdigit(value, &d);
        if (retval) {
            if (d <= 5) {
                VmctConf.vmstate_sendtime = 5;
            } else if (d <= 10) {
                VmctConf.vmstate_sendtime = 10;
            } else if (d <= 20) {
                VmctConf.vmstate_sendtime = 20;
            } else if (d <= 30) {
                VmctConf.vmstate_sendtime = 30;
            } else {
                VmctConf.vmstate_sendtime = 60;
            }
        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s(%d) : Check Config(VM State Send Time) File!\n", __func__, __LINE__);
        }
    }

    if (!strcmp(name, "VMCT_TYPE")) {
        retval = Isdigit(value, &d);
        if (retval) {
            if (d == 1) {
                VmctConf.vm_type = VM_TYPE_UTM;
            } else if (d == 2) {
                VmctConf.vm_type = VM_TYPE_IPS;
            } else if (d == 3) {
                VmctConf.vm_type = VM_TYPE_HOST;
            } else {
                VmctConf.vm_type = VM_TYPE_UTM;
            }
        } else {
            xprint(FMS_VMC | FMS_ERROR, "%s(%d) : Check Config(VM Type) File!\n", __func__, __LINE__);
        }
    }

    /* MANAGEMENT HOST IP Information, */
    if (!strcmp(name, "MANAGEMENT_HOST_IP")) {
        strcpy(VmctConf.manage_hostip, value);
    }

    return;
}

/* 라인별로 스트링을 읽어 파싱한다. */
static void Lineparser(char *buf)
{
    int  r, i = 0;
    char c, str[1024], token[5][256];

    while (1) {
        c = *(buf + i);
        str[i] = c;
        if (c=='#' || c==0) {
            str[i] = 0;
            break;
        }
        i++;
    }

    r = sscanf(str, "%s%s%s%s%s", token[0], token[1], token[2], token[3], token[4]);
    if (r == 3) {
        Gettoken(token[0], token[2]);
    }
}

/* 20140731, by jeon */
static int ReadUtmProcessConfig(char *path)
{
    int c, total = 0, i = 0;
    FILE *fp;
    char filePath[128], subline[200];

    if (path == NULL) {
        xprint(FMS_VMC | FMS_FATAL, "configuration file path is null at %s\n", __func__);
        return(-1);
    }

    sprintf(filePath, "%s/cfg/conf_utm_process.cfg", path);

    if ((fp = fopen(filePath, "r")) == (FILE *)NULL) {
        xprint(FMS_VMC | FMS_FATAL, "%s(%d) : path=%s, fopen is NULL : %s \n", __func__, __LINE__, filePath, strerror(errno));
        return(-1);
    }

    memset(subline, 0x00, sizeof(subline));
    while ((c = getc(fp)) != EOF) {
         if (c == '#') {
             while((c = getc(fp)) != '\n') ;
             continue;
         }

#if 0
         if (c == ' ' || c == '\t' || c == '\n') {
#else
         if ((c == '\n') && (strlen(&subline[0]) != 0)) {
#endif
             strcpy((char *)VmctConf.vmct_proc.pstate[total].process_name, &subline[0]);
             i = 0; 
             total ++;
             memset(subline, 0x00, sizeof(subline));
             xprint(FMS_VMC | FMS_INFO1, "id(%d) : %s \n", total, VmctConf.vmct_proc.pstate[total-1].process_name);

         } else {
             subline[i++] = (unsigned char)c;
         }

         if (total >= MAX_VM_PROC_NUM) {
             break;
         }
    }

    VmctConf.vmct_proc.pcount = total;
    xprint(FMS_VMC | FMS_INFO1, "proc count : %u \n", VmctConf.vmct_proc.pcount);

    fclose(fp);

    return(0);
}

int read_config(char *path)
{
    FILE *fp;
    char *c, fileName[512];
    char str[1024];

    if (path == NULL) {
        xprint(FMS_VMC | FMS_FATAL, "configuration file path is null at %s\n", __func__);
        return(-1);
    }

    sprintf(fileName, "%s/cfg/bpapp.cfg", path);

    if ((fp = fopen(fileName, "r")) == (FILE *)NULL) {
        xprint(FMS_VMC | FMS_FATAL, "fopen error(%s) at %s\n", strerror(errno), __func__);
        return(-1);
    }

    while (1) {
        c = fgets(str, sizeof(str), fp);
        if (c == NULL) {
            break;
        }
        Lineparser(str);
    }
    fclose(fp);

    xprint(FMS_VMC | FMS_LOOKS, "## ================================================================= ##\n");
    xprint(FMS_VMC | FMS_LOOKS, "      MODE          : %s(%d)\n", "Default MODE --> CLIENT ", tlinkHOST.mode);
    xprint(FMS_VMC | FMS_LOOKS, "      VERSION       : %s(%d)\n", "IPV4", tlinkHOST.version);
    xprint(FMS_VMC | FMS_LOOKS, "      RECONNTIME    :   (%d)\n", tlinkHOST.tmr_reconn.tmrVal);
    xprint(FMS_VMC | FMS_LOOKS, "      HBTIME        :   (%d)\n", tlinkHOST.tmr_hb.tmrVal);
    xprint(FMS_VMC | FMS_LOOKS, "      IP   (Server) : %s(%s)\n", addr2str(htonl(VmctConf.host_ip)), tlinkHOST.addr);
    xprint(FMS_VMC | FMS_LOOKS, "      PORT (Server) : %d(%d)\n", VmctConf.host_port, tlinkHOST.port);
    xprint(FMS_VMC | FMS_LOOKS, "      IP   (Client) : %s\n", addr2str(htonl(VmctConf.vmct_ip)));
    xprint(FMS_VMC | FMS_LOOKS, "      PORT (Client) : %d\n", VmctConf.vmct_clientport);
    xprint(FMS_VMC | FMS_LOOKS, "## ================================================================= ##\n\n");

    /* Read Check UTM Process Config */
    if (ReadUtmProcessConfig(path) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "UTM Process List Information Configuration error, So exit\n");
        return (-1);
    }

    return(0);
}

int InitVariable(void)
{
    int i;

    memset(&VmctConf, 0x00, sizeof(vmct_config_t));

    memset(&tlinkHOST, 0x00, sizeof(tlinkHOST));
    memset(&alist, 0x00, sizeof(alist));
    memset(tmrsync, 0x00, sizeof(tmrsync));

    TmrCmdQid = -1;
    MaxFD     = 0;

    /* Client Tcp link 정보 */
    tlinkHOST.ListenFD          = -1;
    tlinkHOST.mode              = LINK_MODE_CLIENT;
    tlinkHOST.version           = LINK_IP_V4_TYPE;
    tlinkHOST.tmr_hb.tmrVal     = TCP_HEARTBEATTIME;
    tlinkHOST.tmr_reconn.tmrVal = TCP_RECONNTIME;
    tlinkHOST.port              = 0;
    strcpy(tlinkHOST.addr, "127.0.0.1");

    gethostname(VmctConf.vmct_name, MAX_NAME_LENGTH);
    VmctConf.vmct_id = gethostid();

    for (i = 0; i < MAX_PTASK; i++)  {
        ThreadFlags[i] = FALSE;
        ThreadAlive[i] = FALSE;
    }

    return (0);
}

void InitMutexCmdTmr(void)
{
    if ((TmrCmdQid = ipcOpen(VMC_TMRCMD_QUEUE_KEY, 0666 | IPC_CREAT)) < 0) {
        xprint(FMS_VMC | FMS_FATAL, "ipcOpen failed : KEY = 0x%x \n", VMC_TMRCMD_QUEUE_KEY);
        exit(1);
    }

    timer_init(MAX_TMR_NUM, VMC_TMRCMD_QUEUE_KEY);

    ipcClean(TmrCmdQid);

    xprint(FMS_VMC | FMS_INFO3, " %s : queue infomation \n", __func__);
    xprint(FMS_VMC | FMS_INFO3, "    +qid(TMRCMD) :: KEY=%4x, QID=%5d) \n", VMC_TMRCMD_QUEUE_KEY, TmrCmdQid);

    /* Mutex */
    pthread_mutex_init(&TcpHostMutex, NULL);

    return;
}
