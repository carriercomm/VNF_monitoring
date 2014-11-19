/* -------------------------------------------------------------------------
 * File Name   : vmct_debug.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2013 by KBell Inc.
 * Description : VMCT Debug fn.
 * History     : 14/07/04    Initial Coded.
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

#include "vmct_conf.h"
#if 0
#include "vmct_sock.h"
#endif
#include "vmct_debug.h"


#ifdef __DEBUG__

void DebugSizeofStruct(void)
{
    xprint(FMS_VMC | FMS_INFO1, "****************************************************************\n\n");
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    MAX_STR_LENGTH       : [%d]\n", MAX_STR_LENGTH);
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    MAX_DATA_LENGTH      : [%d]\n", MAX_DATA_LENGTH);
    xprint(FMS_VMC | FMS_INFO1, "****************************************************************\n");
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    (unsigned short)     : [%d]\n", sizeof(unsigned short));
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    (unsigned int)       : [%d]\n", sizeof(unsigned int));
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    (unsigned long)      : [%d]\n", sizeof(unsigned long));
    xprint(FMS_VMC | FMS_INFO1, "Sizeof    (unsigned *int)      : [%d]\n", sizeof(unsigned int *));
    xprint(FMS_VMC | FMS_INFO1, "****************************************************************\n");
    return;
}

void DebugVmctInfo(void)
{
    xprint(FMS_VMC | FMS_INFO1, "========================================================\n");
    xprint(FMS_VMC | FMS_INFO1, "VmctConf.host_ip   = %s\n", addr2ntoa(VmctConf.host_ip));
    xprint(FMS_VMC | FMS_INFO1, "VmctConf.manage_ip = %s\n", VmctConf.manage_hostip);
    xprint(FMS_VMC | FMS_INFO1, "VmctConf.vmct_name = %s\n", VmctConf.vmct_name);
    xprint(FMS_VMC | FMS_INFO1, "VmctConf.vmct_id   = %u(0x%x)\n", VmctConf.vmct_id, VmctConf.vmct_id);
    xprint(FMS_VMC | FMS_INFO1, "========================================================\n");
    return;
}

void DebugVmProcessInfo(void)
{
    int i;
    xprint(FMS_VMC | FMS_INFO1, "*********************************************************************\n");
    xprint(FMS_VMC | FMS_INFO1, "  VM Type : 0x%u \n", VmctConf.vm_type); 
    xprint(FMS_VMC | FMS_INFO1, "  VM State Send Time : %u sec\n", VmctConf.vmstate_sendtime);
    xprint(FMS_VMC | FMS_INFO1, "---------------------------------------------------------------------\n");
    for (i = 0; i < VmctConf.vmct_proc.pcount; i++) {
        xprint(FMS_VMC | FMS_INFO1, "Process Name   = %s\n", VmctConf.vmct_proc.pstate[i].process_name);
        xprint(FMS_VMC | FMS_INFO1, "Process State  = %s\n", 
                                (VmctConf.vmct_proc.pstate[i].process_state == PROCESS_STATE_UP) ? "Up" : "Down");
        xprint(FMS_VMC | FMS_INFO1, "========================================================\n");
    }
    xprint(FMS_VMC | FMS_INFO1, "*********************************************************************\n\n");
    return;
}


#endif /* __DEBUG__ */
