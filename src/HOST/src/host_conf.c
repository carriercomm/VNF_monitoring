/* -------------------------------------------------------------------------
 * File Name   : host_conf.c
 * Author      : Hyeong-Ik Jeon 
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : Host configure fn.
 * History     : 14/07/03    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <sys/un.h>
#include <pthread.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#ifdef __PGSQL__
#include "pqmb.h"
#endif

#include "dbmb.h"

#include "host_main.h"
#include "host_conf.h"
#include "host_sock.h"
#include "host_task.h"
#include "host_fifo.h"
#include "host_debug.h"


static unsigned int Isdigit(char *s, unsigned int *c);
static void Gettoken(char *name, char *value);
static void Lineparser(char *buf);
static int GetVmStatusInfoDB(void);


host_config_t hostConf;
process_state_t basePState[MAX_VM_PROC_NUM];
vmc_stateinfo_t stateinfo[MAX_SYSTEM_NUM];


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

    /* TCPD TCP sock Information, main link config */
    if (!strcmp(name, "HOST_ADDRESS")) {
        strcpy(tlink.addr, value);
        hostConf.host_ip = inet_addr(tlink.addr);
    }

    if (!strcmp(name, "HOST_PORT")) {
        retval = Isdigit(value, &d);
        if (retval) { 
            tlink.port = d;
            hostConf.host_port = d;
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(sock port) File!\n", __func__, __LINE__);
        }
    }

    /* Openstack NOVA MySQL DB Information */
    if (!strcmp(name, "NOVA_DB_ADDRESS")) {
        memset(hostConf.nova_DbAddr, 0x00, sizeof(hostConf.nova_DbAddr));
        strcpy(hostConf.nova_DbAddr, value);
    }

    /* Openstack NOVA MySQL Passwd */
    if (!strcmp(name, "NOVA_DB_PASSWORD")) {
        memset(hostConf.nova_DbPass, 0x00, sizeof(hostConf.nova_DbPass));
        strcpy(hostConf.nova_DbPass, value);
    }

    /* Openstack NOVA MySQL DB Information */
    if (!strcmp(name, "NOVA_DB_USERNAME")) {
        memset(hostConf.nova_DbUserid, 0x00, sizeof(hostConf.nova_DbUserid));
        strcpy(hostConf.nova_DbUserid, value);
    }

    /* Openstack NOVA MySQL DB Schema */
    if (!strcmp(name, "NOVA_DB_SCHEMA")) {
        memset(hostConf.nova_DbName, 0x00, sizeof(hostConf.nova_DbName));
        strcpy(hostConf.nova_DbName, value);
    }

    /* Orchestrator DB Information */
    if (!strcmp(name, "ORCHE_DB_ADDRESS")) {
        memset(hostConf.orche_DbAddr, 0x00, sizeof(hostConf.orche_DbAddr));
        strcpy(hostConf.orche_DbAddr, value);
    }

    /* Orchestrator DB Information */
    if (!strcmp(name, "ORCHE_DB_USERNAME")) {
        memset(hostConf.orche_DbUserid, 0x00, sizeof(hostConf.orche_DbUserid));
        strcpy(hostConf.orche_DbUserid, value);
    }

    /* Orchestrator DB Passwd */
    if (!strcmp(name, "ORCHE_DB_PASSWORD")) {
        memset(hostConf.orche_DbPass, 0x00, sizeof(hostConf.orche_DbPass));
        strcpy(hostConf.orche_DbPass, value);
    }

    /* Orchestrator DB Name */
    if (!strcmp(name, "ORCHE_DB_NAME")) {
        memset(hostConf.orche_DbName, 0x00, sizeof(hostConf.orche_DbName));
        strcpy(hostConf.orche_DbName, value);
    }

    /* Orchestrator DB Schema */
    if (!strcmp(name, "ORCHE_DB_SCHEMA")) {
        memset(hostConf.orche_DbSchema, 0x00, sizeof(hostConf.orche_DbSchema));
        strcpy(hostConf.orche_DbSchema, value);
    }

    /* Orchestrator DB Port */
    if (!strcmp(name, "ORCHE_DB_PORT")) {
        retval = Isdigit(value, &d);
        if (retval) {
            hostConf.orche_DbPort = d;
        }
    }

    /* Check VMC Status Time */
    if (!strcmp(name, "CHECK_VMC_STATUS_SEC")) {
        retval = Isdigit(value, &d);
        if (retval) {
            if (d <= 5) {
                CHECK_VMCSTATUS_SEC = 5;
            } else if (d <= 10) {
                CHECK_VMCSTATUS_SEC = 10;
            } else if (d <= 20) {
                CHECK_VMCSTATUS_SEC = 20;
            } else if (d <= 30) {
                CHECK_VMCSTATUS_SEC = 30;
            } else {
                CHECK_VMCSTATUS_SEC = 60;
            }
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(VM Status) File!\n", __func__, __LINE__);
        }
    }

    /* Check TCP Status Time */
    if (!strcmp(name, "CHECK_TCP_STATUS_MIN")) {
        retval = Isdigit(value, &d);
        if (retval) {
            CHECK_TCPSTATUS_MIN = d;
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(TCP Status) File!\n", __func__, __LINE__);
        }
    }

    /* Check Bypass WatchDog Time */
    if (!strcmp(name, "BYPASS_WATCHDOG_TIME")) {
        retval = Isdigit(value, &d);
        if (retval) {
            CHECK_WATCHDOG_MSEC = d * 1000;
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(WatchDog Time) File!\n", __func__, __LINE__);
        }
    }

    /* Check baremetal mode */
    if (!strcmp(name, "HOST_BAREMETAL_MODE")) {
        retval = Isdigit(value, &d);
        if (retval) {
            if (d > 0) {
                hostConf.baremetal_mode = 1;
            } else {
                hostConf.baremetal_mode = 0;
            }
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(baremetal mode) File!\n", __func__, __LINE__);
        }
    }

    /* Check cloud mode */
    if (!strcmp(name, "HOST_CLOUDE_MODE")) {
        retval = Isdigit(value, &d);
        if (retval) {
            if (d > 0) {
                hostConf.cloud_mode = 1;
            } else {
                hostConf.cloud_mode = 0;
            }
        } else {
            xprint(FMS_HST | FMS_ERROR, "%s(%d) : Check Config(cloud mode) File!\n", __func__, __LINE__);
        }
    }

    return;
}

/* 라인별로 스트링을 읽어 파싱한다. */
static void Lineparser(char *buf)
{
    int  r, i = 0;
    char c, str[MAX_STR_LENGTH*8], token[5][MAX_STR_LENGTH];

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

int read_config(char *path)
{
    FILE *fp;
    char *c;
    char CfgPath[512];
    char str[MAX_STR_LENGTH*8];

    if (path == NULL) {
        xprint(FMS_HST | FMS_FATAL, "configuration file path is null at %s\n", __func__);
        return(-1); 
    }

    sprintf(CfgPath, "%s/cfg/bpapp.cfg", path);

    if ((fp = fopen(CfgPath, "r")) == (FILE *)NULL) {
        xprint(FMS_HST | FMS_FATAL, "fopen error(%s) at %s\n", strerror(errno), __func__);
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

    return(0);
}

void FreeVariable(void)
{
    if (TcpFifo != NULL) {
        block_FifoEmpty(TcpFifo);
        block_FifoRelease(TcpFifo);
    }

    return;
}

int InitVariable(char *path)
{
    int i;

    memset(&hostConf, 0x00, sizeof(host_config_t));
    memset(&tlink, 0x00, sizeof(tlink));
    memset(alist, 0x00, sizeof(alist));
#if 0
    memset(host_ccsinfo, 0x00, sizeof(host_ccsinfo));
#endif

    sprintf(hostConf.EnvPath, "%s", path);

    /* Nova MySQL */
    sprintf(hostConf.nova_DbAddr, "127.0.0.1");
    hostConf.nova_DbPort          = 3306;
    sprintf(hostConf.nova_DbUserid, "root");
    sprintf(hostConf.nova_DbPass, "openstack");
    sprintf(hostConf.nova_DbName, "nova");

    /* Orchestartor MySQL */
    sprintf(hostConf.orche_DbAddr, "127.0.0.1");
    hostConf.orche_DbPort          = 5432;
    sprintf(hostConf.orche_DbUserid, "bypass");
    sprintf(hostConf.orche_DbPass, "by!234");
    sprintf(hostConf.orche_DbName, "sdi");

    hostConf.vmc_mode = BYPASS_MODE_OFF;
    hostConf.tcp_mode = BYPASS_MODE_OFF;

    gethostname(hostConf.host_name, MAX_NAME_LENGTH);
    hostConf.host_id = gethostid();

    /* Tcp link 정보 config로 설정 운용 */
    tlink.ListenFD          = -1;
    tlink.mode              = LINK_MODE_SERVER;
    tlink.version           = LINK_IP_V4_TYPE;
    tlink.tmr_reconn.tmrVal = 2000;
    tlink.port              = 17000;
    strcpy(tlink.addr, "127.0.0.1");

    MaxFD = 0;

    for (i = 0; i < MAX_LISTENQ; i++) {
        alist[i].AcceptFD = -1;
        pthread_mutex_init(&alist[i].linkmutex, NULL);
    }

    for (i = 0; i < MAX_PTASK; i++)  {
        ThreadFlags[i] = FALSE;
        ThreadAlive[i] = FALSE;
    }

    /* 20140812, by jein */
    memset(&basePState[0], 0x00, sizeof(process_state_t) * MAX_VM_PROC_NUM);
    for (i = 0; i < MAX_VM_PROC_NUM; i++) {
        strcpy((char *)basePState[i].process_name, "-");
    }

    /* 20140812, by jein */
    memset(&stateinfo[0], 0x00, sizeof(vmc_stateinfo_t) * MAX_SYSTEM_NUM);
    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        memcpy(&stateinfo[i].pstate[0], &basePState[0], sizeof(process_state_t) * MAX_VM_PROC_NUM);
    }

#if 0
    /* Init FIFO  */
    RegCtrlFifo = block_FifoNew();
    if (RegCtrlFifo == NULL) {
        xprint(FMS_HST | FMS_FATAL, "RegCtrlFifo : memory alloc failtd at block_FifoNew\n");
    }
#endif

    TcpFifo = block_FifoNew();
    if (TcpFifo == NULL) {
        xprint(FMS_HST | FMS_FATAL, "TcpFifo : memory alloc failtd at block_FifoNew\n");
    }

    /* config file check */
    if (read_config(path) < 0) {
        xprint(FMS_HST | FMS_FATAL, "Configuration error, So exit\n");
        return -1;
    }

    if (hostConf.cloud_mode == 1) {
        /* MySQL initialize */
        if (DatabaseInit(hostConf.nova_DbAddr, hostConf.nova_DbUserid, hostConf.nova_DbPass, hostConf.nova_DbName, hostConf.nova_DbPort) < 0) {
            xprint(FMS_HST | FMS_FATAL, "Nova MySQL DatabaseInit failed\n");
            return -1;
        }
    }

#ifdef __PGSQL__
    /* PGSQL initialize */
    if (PQ_DatabaseInit(hostConf.orche_DbAddr, hostConf.orche_DbUserid, hostConf.orche_DbPass, hostConf.orche_DbName, hostConf.orche_DbPort) < 0) {
        xprint(FMS_HST | FMS_FATAL, "Orchestrator PGSQL DatabaseInit failed\n");
        return -1;
    }
#else
    /* Orchestrator initialize */
    if (OrcheDatabaseInit(hostConf.orche_DbAddr, hostConf.orche_DbUserid, hostConf.orche_DbPass, hostConf.orche_DbName, hostConf.orche_DbPort) < 0) {
        xprint(FMS_HST | FMS_FATAL, "Orchestrator MySQL DatabaseInit failed\n");
        return -1;
    }
#endif

    if (GetVmStatusInfoDB() < 0) {
        xprint(FMS_HST | FMS_FATAL, "DB VM Status Info read error, So exit\n");
        return -1;
    }

    /* set bypass nic watchdog fn. */
    SetBypassWatchdog();

    return 0;
}

static int GetVmStatusInfoDB(void)
{
    int i, ret = 0, vmcount = 0, bpmode = BYPASS_MODE_OFF;
    char cmd[MAX_STR_LENGTH];

    if (hostConf.cloud_mode == 1) {
        vmcount = SelectAllVMStatus(&hostConf.vmc_status[0]);
    }

    if (vmcount < 0) {
        xprint(FMS_HST | FMS_FATAL, "SelectAllVMStatus Error\n");
        return -1;
    }

#ifdef __DEBUG__
    DebugVmStatus();
#endif

    if (vmcount == 0) {
         bpmode = BYPASS_MODE_ON;
    }

    /* check vm status for bypass mode ctrl */
    for (i = 0; i < vmcount; i++) {
#if 0
        if ((hostConf.vmc_status[i].vm_status != STATUS_ACTIVE || hostConf.vmc_status[i].vm_status != STATUS_BUILD) &&
            (hostConf.vmc_status[i].vm_power_status != PW_STATUS_RUNNING || hostConf.vmc_status[i].vm_power_status != PW_STATUS_NOSTATUS)) {
            bpmode = BYPASS_MODE_ON;
        }
#else
        if ((hostConf.vmc_status[i].vm_status != STATUS_ACTIVE && hostConf.vmc_status[i].vm_status != STATUS_BUILD) ||
            (hostConf.vmc_status[i].vm_power_status != PW_STATUS_RUNNING && hostConf.vmc_status[i].vm_power_status != PW_STATUS_NOSTATUS)) {
            /* 20140814, by jeon */
            if (hostConf.vmc_status[i].vm_power_status != PW_STATUS_SHUTDOWN) {
                xprint(FMS_HST | FMS_INFO3, "vm_id = %u :: Bypass Mode (ON)\n", hostConf.vmc_status[i].vm_id);
                bpmode = BYPASS_MODE_ON;
            }
        }
#endif
    }

    /* Set Bypass mode */
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

        hostConf.bp_mode = BYPASS_MODE_ON;

        xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ ON ] = [ %s ]\n", cmd);

    } else {
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

        hostConf.bp_mode = BYPASS_MODE_OFF;

        xprint(FMS_HST | FMS_INFO1, "Set Bypass Mode [ OFF ] = [ %s ]\n", cmd);
    }

    hostConf.vmc_mode = bpmode;
    hostConf.vm_count = vmcount;

    return (vmcount);
}



