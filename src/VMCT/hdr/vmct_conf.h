/* -------------------------------------------------------------------------
 * File Name   : vmct_conf.h
 * Author      : Hyenog-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : BPAPP-VMCT configuration header.
 * History     : 14/07/04    Initial Coded.
 * -------------------------------------------------------------------------*/


#ifndef __VMCT_CONF_H__
#define __VMCT_CONF_H__


typedef struct {
    unsigned int     pcount;
    process_state_t  pstate[MAX_VM_PROC_NUM];
} vmct_proc_t;

typedef struct {
    /* 20140903, by jeon */
    unsigned short   vm_type;
    unsigned short   vmstate_sendtime;

    unsigned int     host_ip;
    unsigned short   host_port;

    unsigned int     vmct_id;
    unsigned int     vmct_ip;
    unsigned short   vmct_clientport;

    char             manage_hostip[MAX_NAME_LENGTH];
    char             vmct_name[MAX_NAME_LENGTH];

    vmct_proc_t      vmct_proc;

} vmct_config_t;



extern int           TmrCmdQid;
extern vmct_config_t VmctConf;


extern int InitVariable(void);
extern int read_config(char *path);
extern void InitMutexCmdTmr(void);


#endif /* __VMCT_CONF_H__ */
