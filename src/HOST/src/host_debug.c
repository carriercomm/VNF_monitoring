/* -------------------------------------------------------------------------
 * File Name   : host_debug.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2013 by KBell Inc.
 * Description : CFM(Configuration Management)
 * History     : 13/07/02    Initial Coded.
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
#include <pthread.h>
#include <sys/un.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#include "host_conf.h"
#if 0
#include "host_sock.h"
#endif
#include "host_debug.h"


#ifdef __DEBUG__

static char *str2powerstatus(int status);
static char *str2vmstatus(int status);


void DebugSizeofStruct(void)
{
    xprint(FMS_HST | FMS_INFO1, "****************************************************************\n\n");
    xprint(FMS_HST | FMS_INFO1, "Sizeof    MAX_STR_LENGTH       : [%d]\n", MAX_STR_LENGTH);
    xprint(FMS_HST | FMS_INFO1, "Sizeof    MAX_DATA_LENGTH      : [%d]\n", MAX_DATA_LENGTH);
    xprint(FMS_HST | FMS_INFO1, "****************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "Sizeof    (unsigned short)     : [%d]\n", sizeof(unsigned short));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    (unsigned int)       : [%d]\n", sizeof(unsigned int));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    (unsigned long)      : [%d]\n", sizeof(unsigned long));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    (unsigned *int)      : [%d]\n", sizeof(unsigned int *));
    xprint(FMS_HST | FMS_INFO1, "****************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "Sizeof    common_tcpmsg_t      : [%d]\n", sizeof(common_tcpmsg_t));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    vm_proc_state_msg    : [%d]\n", sizeof(vm_proc_state_msg));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    vm_process_t         : [%d]\n", sizeof(vm_process_t));
#if 0
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_ccs_rsp_msg     : [%d]\n", sizeof(nsgp_ccs_rsp_msg));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_traffic_stat_t  : [%d]\n", sizeof(nsgp_traffic_stat_t));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_flow_stat_t     : [%d]\n", sizeof(nsgp_flow_stat_t));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_event_info_t    : [%d]\n", sizeof(nsgp_event_info_t));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_lineid_msg      : [%d]\n", sizeof(nsgp_lineid_msg));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_dsg_msg         : [%d]\n", sizeof(nsgp_dsg_msg));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsgp_dsg_auto_thrd   : [%d]\n", sizeof(nsgp_dsg_auto_threshold_t));
    xprint(FMS_HST | FMS_INFO1, "Sizeof    nsg_gui_sinkcmd_msg  : [%d]\n", sizeof(nsg_gui_sinkcmd_msg));
    xprint(FMS_HST | FMS_INFO1, "****************************************************************\n\n");
#endif

    xprint(FMS_HST | FMS_INFO1, "========================================================\n");
    xprint(FMS_HST | FMS_INFO1, "Nova DB \n");
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbAddr   = %s\n", hostConf.nova_DbAddr);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbUserid = %s\n", hostConf.nova_DbUserid);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbPass   = %s\n", hostConf.nova_DbPass);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbName   = %s\n", hostConf.nova_DbName);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbPort   = %d\n", hostConf.nova_DbPort);

    xprint(FMS_HST | FMS_INFO1, "Orchestrator DB \n");
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbAddr   = %s\n", hostConf.orche_DbAddr);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbUserid = %s\n", hostConf.orche_DbUserid);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbPass   = %s\n", hostConf.orche_DbPass);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbName   = %s\n", hostConf.orche_DbName);
    xprint(FMS_HST | FMS_INFO1, "hostConf.DbPort   = %d\n", hostConf.orche_DbPort);
    xprint(FMS_HST | FMS_INFO1, "========================================================\n");
    return;
}

void DebugHostInfo(void)
{
    xprint(FMS_HST | FMS_INFO1, "========================================================\n");
    xprint(FMS_HST | FMS_INFO1, "hostConf.host_ip   = %s\n", addr2ntoa(hostConf.host_ip));
    xprint(FMS_HST | FMS_INFO1, "hostConf.host_port = %u\n", hostConf.host_port);
    xprint(FMS_HST | FMS_INFO1, "hostConf.host_name = %s\n", hostConf.host_name);
    xprint(FMS_HST | FMS_INFO1, "hostConf.host_id   = %u(0x%x)\n", hostConf.host_id, hostConf.host_id);
    xprint(FMS_HST | FMS_INFO1, "========================================================\n");
    return;
}

void DebugVmStatus(void)
{
    int i;

    xprint(FMS_HST | FMS_INFO1, "\n");
    xprint(FMS_HST | FMS_INFO1, "**************************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "* CHANGE |-------->| VM STATUS (Count : %d)  \n", hostConf.vm_count);
    xprint(FMS_HST | FMS_INFO1, "**************************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "*   ID      VM Status      Power Status      VM IPAddr       VM Name      \n");
    xprint(FMS_HST | FMS_INFO1, "**************************************************************************\n");

    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        if (hostConf.vmc_status[i].vm_id == 0) {
            continue;
        }

        xprint(FMS_HST | FMS_INFO1, "\t%u\t %s\t %s\t %s\t %s\n",
               hostConf.vmc_status[i].vm_id, str2vmstatus(hostConf.vmc_status[i].vm_status),
               str2powerstatus(hostConf.vmc_status[i].vm_power_status), addr2ntoa(hostConf.vmc_status[i].vm_ipaddr),
               hostConf.vmc_status[i].vm_name);
    }

    xprint(FMS_HST | FMS_INFO1, "**************************************************************************\n\n");
    return;
}

void DebugVmProcessState(vm_proc_state_msg *s)
{
    int i, count, vm_type;

    if (s == NULL) {
        xprint(FMS_HST | FMS_INFO1, "VM Process State is NULL \n");
        return;
    }

    count   = NBO_NTOHL(s->pcount);
    vm_type = NBO_NTOHL(s->vm_type);

    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n");
#if 0
    xprint(FMS_HST | FMS_INFO1, "VM Time Stamp   : [%s]\n", s->TimeStamp);
#endif
    xprint(FMS_HST | FMS_INFO1, "VM IP Address   : [%s]\n", addr2ntoa(NBO_NTOHL(s->vm_ipaddress)));
    xprint(FMS_HST | FMS_INFO1, "VM Type         : [%d]\n", vm_type);
    xprint(FMS_HST | FMS_INFO1, "VM Proc Count   : [%d]\n", count);
    xprint(FMS_HST | FMS_INFO1, "VM ID           : [%s]\n", s->vm_id);
    xprint(FMS_HST | FMS_INFO1, "VM NAME         : [%s]\n", s->vm_name);
    xprint(FMS_HST | FMS_INFO1, "VM MANAGE IP    : [%s]\n", s->manage_ip);
    xprint(FMS_HST | FMS_INFO1, "=====================================================================\n");
    for (i = 0; i < count; i++) {
        xprint(FMS_HST | FMS_INFO1, "VM Proc Name    : [%s]\n", s->pstate[i].process_name);
        xprint(FMS_HST | FMS_INFO1, "VM Proc State   : [%s]\n", 
                                     (NBO_NTOHL((s->pstate[i].process_state)) == PROCESS_STATE_UP) ? "Up" : "Down");
        xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    }
    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n\n");

    return;
}

static char *str2powerstatus(int status)
{
    static char str[MAX_NAME_LENGTH];

    memset(str, 0x00, sizeof(str));

    switch (status) {
        case PW_STATUS_NOSTATUS:
            strcpy(str, "NOSTATUS ");
            break;

        case PW_STATUS_RUNNING:
            strcpy(str, "RUNNING ");
            break;

        case PW_STATUS_BLOCKED:
            strcpy(str, "BLOCKED ");
            break;

        case PW_STATUS_PAUSED:
            strcpy(str, " PAUSED ");
            break;

        case PW_STATUS_SHUTDOWN:
            strcpy(str, "SHUTDOWN");
            break;

        default:
            strcpy(str, "UNKNOWN ");
    }

    return str;
}

static char *str2vmstatus(int status)
{
    static char str[MAX_NAME_LENGTH];

    memset(str, 0x00, sizeof(str));

    switch (status) {
        case STATUS_ACTIVE:
            strcpy(str, "ACTIVE ");
            break;

        case STATUS_BUILD:
            strcpy(str, "BUILD  ");
            break;

        case STATUS_DELETED:
            strcpy(str, "   -   ");
            break;

        case STATUS_STOPPED:
            strcpy(str, "SHUTOFF");
            break;

        case STATUS_PAUSED:
            strcpy(str, "PAUSED");
            break;

        case STATUS_SUSPENDED:
            strcpy(str, "SUSPENDED");
            break;

        default:
            strcpy(str, "UNKNOWN");
    }

    return str;
}

void DebugVmcStateInfo(void)
{
    int i;

    for (i = 0; i < MAX_SYSTEM_NUM; i++) {
        if (stateinfo[i].vm_ip == 0) continue;
	xprint(FMS_HST | FMS_INFO1, "%s :: i(%d) / vm ip : %s\n", __func__, i, addr2ntoa(stateinfo[i].vm_ip));
    }

    return;
}

#if 0
void DebugSystemInfo(int total_system)
{
    int idx;

    /* NEXT_DEL, debug print by icecom */
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : System Total Num = %d\n", __func__, __LINE__, total_system);
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : alist   \n", __func__, __LINE__);
    for (idx = 0; idx < total_system; idx++) {
        if (alist[idx].sysinfo.systemidx != 0) {
            xprint(FMS_HST | FMS_INFO1, "%s(%d) : %d. %s \n", __func__, __LINE__,
                    alist[idx].sysinfo.systemidx, addr2ntoa(alist[idx].sysinfo.systemaddr));
        }
    }
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : no. sid, addr, systype, name, desc\n", __func__, __LINE__);
    for (idx = 0; idx < total_system; idx++) {
        if (alist[idx].sysinfo.systemidx != 0) {
            xprint(FMS_HST | FMS_INFO1, "%s(%d) : %d. %d, %s, %X, %s, %s\n", __func__, __LINE__,
                    hostConf.host_system[idx].systemidx,
                    hostConf.host_system[idx].systemid,
                    addr2ntoa(hostConf.host_system[idx].systemaddr),
                    hostConf.host_system[idx].systemtype,
                    hostConf.host_system[idx].systemname,
                    hostConf.host_system[idx].description);
        }
    }
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);

    return;
}

void DebugGuiAction(nsgd_gui_action_msg *action)
{
    int i, count;

    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : action      : %u \n", __func__, __LINE__, NBO_NTOHL(action->action));
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : index_count : %u \n", __func__, __LINE__, NBO_NTOHL(action->index_count));

    count = NBO_NTOHL(action->index_count);
    for (i = 0; i < count; i++) {
        xprint(FMS_HST | FMS_INFO1, "%s(%d) : table_index : %u \n", __func__, __LINE__, NBO_NTOHL(action->table_index[i]));
    }
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);

    return;
}

void DebugCCSDsgInfo(nsgd_dsg_msg *dsg)
{
    int i, count;

    count = NBO_NTOHS(dsg->DsgCount);
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);
    for (i = 0; i < count; i++) {
        xprint(FMS_HST | FMS_INFO1, "%s(%d) : dsg id[%d]   : %u \n", __func__, __LINE__, i, NBO_NTOHL(dsg->dsg[i].dsg_id));
        xprint(FMS_HST | FMS_INFO1, "%s(%d) : dsg addr[%d] : %s \n", __func__, __LINE__, i, addr2ntoa(NBO_NTOHL(dsg->dsg[i].dsg_ipaddr)));
    }
    xprint(FMS_HST | FMS_INFO1, "%s(%d) : ----------------------------\n", __func__, __LINE__);
    
    return;
}

void DebugGUISinkholeCmd(nsgd_gui_sinkcmd_msg *s)
{
    int i;
    if (s == NULL) {
        xprint(FMS_HST | FMS_INFO1, "GUI SINKHOLE CMD is NULL \n");
        return;
    }

    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "action         : [%u]\n", NBO_NTOHL(s->action));
    xprint(FMS_HST | FMS_INFO1, "login_id       : [%s]\n", s->login_id);
    xprint(FMS_HST | FMS_INFO1, "login_pw       : [%s]\n", s->login_pw);
    xprint(FMS_HST | FMS_INFO1, "sink_count     : [%u]\n", NBO_NTOHS(s->sink_count));
    xprint(FMS_HST | FMS_INFO1, "cisco_count    : [%u]\n", NBO_NTOHS(s->cisco_count));
    xprint(FMS_HST | FMS_INFO1, "juniper_count  : [%u]\n", NBO_NTOHS(s->juniper_count));
    xprint(FMS_HST | FMS_INFO1, "antiddos_count : [%u]\n", NBO_NTOHS(s->antiddos_count));
    xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    for (i = 0; i < NBO_NTOHS(s->sink_count); i++) {
        xprint(FMS_HST | FMS_INFO1, "    his_id  (%d)        : [%u]\n", i, NBO_NTOHL(s->sinkhole[i].his_id));
        xprint(FMS_HST | FMS_INFO1, "    sink_id (%d)        : [%u]\n", i, NBO_NTOHS(s->sinkhole[i].sink_id));
    }
    xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    for (i = 0; i < NBO_NTOHS(s->cisco_count); i++) {
        xprint(FMS_HST | FMS_INFO1, "    cisco   cmd (%d)    : [%s]\n", i, s->cisco[i].cmd);
    }
    xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    for (i = 0; i < NBO_NTOHS(s->juniper_count); i++) {
        xprint(FMS_HST | FMS_INFO1, "    juniper cmd (%d)    : [%s]\n", i, s->juniper[i].cmd);
    }
    xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    for (i = 0; i < NBO_NTOHS(s->antiddos_count); i++) {
        if (NBO_NTOHL(s->action) == 3) {
            xprint(FMS_HST | FMS_INFO1, "    antiddos  cmd (%d)  : [%s]\n", i, s->antiddos[i].cmd);
        } else {
            switch(NBO_NTOHS(s->sink_count))
            {
                case 1:
                    xprint(FMS_HST | FMS_INFO1, "    cisco     cmd (%d)  : [%s]\n", i, s->cisco[i].cmd);
                    break;

                case 2:
                    xprint(FMS_HST | FMS_INFO1, "    cisco     cmd (%d)  : [%s]\n", i, s->cisco[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    juniper   cmd (%d)  : [%s]\n", i, s->juniper[i].cmd);
                    break;

                case 3:
                    xprint(FMS_HST | FMS_INFO1, "    cisco     cmd (%d)  : [%s]\n", i, s->cisco[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    juniper   cmd (%d)  : [%s]\n", i, s->juniper[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    antiddos  cmd (%d)  : [%s]\n", i, s->antiddos[i].cmd);
                    break;

                case 4:
                    xprint(FMS_HST | FMS_INFO1, "    cisco     cmd (%d)  : [%s]\n", i, s->cisco[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    juniper   cmd (%d)  : [%s]\n", i, s->juniper[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    antiddos  cmd (%d)  : [%s]\n", i, s->antiddos[i].cmd);
                    xprint(FMS_HST | FMS_INFO1, "    antiddos4 cmd (%d)  : [%s]\n", i, s->antiddos[i].cmd);
                    break;

                default:
                    xprint(FMS_CTS | FMS_ERROR, "%s(%d) : sink num is wrong (= %d)\n",__func__, __LINE__, NBO_NTOHS(s->sink_count));
            }
        }
    }

    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n\n");
    return;
}

void DebugDcsRowPakcet(nsg_dcs_rowpacket_msg *r)
{
    if (r == NULL) {
        xprint(FMS_HST | FMS_INFO1, "DCS DCS_MX960POSCLI_RSP is NULL \n");
        return;
    }

    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n");
    xprint(FMS_HST | FMS_INFO1, "line id        : [%u]\n", NBO_NTOHS(r->line_id));
    xprint(FMS_HST | FMS_INFO1, "filter_type    : [%u]\n", NBO_NTOHS(r->filter_type));
    xprint(FMS_HST | FMS_INFO1, "filter_value   : [%s]\n", r->filter_value);
    xprint(FMS_HST | FMS_INFO1, "rcv_second     : [%u]\n", NBO_NTOHL(r->rcv_second));
    xprint(FMS_HST | FMS_INFO1, "rcv_packets    : [%u]\n", NBO_NTOHL(r->rcv_packets));
    xprint(FMS_HST | FMS_INFO1, "---------------------------------------------------------------------\n");
    xprint(FMS_HST | FMS_INFO1, "start_time     : [%s]\n", r->start_time);
    xprint(FMS_HST | FMS_INFO1, "end_time       : [%s]\n", r->end_time);
    xprint(FMS_HST | FMS_INFO1, "pcap_name      : [%s]\n", r->pcap_name);
    xprint(FMS_HST | FMS_INFO1, "pcap_path      : [%s]\n", r->pcap_path);
    xprint(FMS_HST | FMS_INFO1, "pcap_size      : [%u]\n", NBO_NTOHL(r->pcap_size));
    xprint(FMS_HST | FMS_INFO1, "dump_status    : [%u]\n", NBO_NTOHS(r->dump_status));

    xprint(FMS_HST | FMS_INFO1, "*********************************************************************\n\n");
    return;
}

#endif


#endif /* __DEBUG__ */
