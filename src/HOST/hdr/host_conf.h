/* -------------------------------------------------------------------------
 * File Name   : host_conf.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : Host configure header.
 * History     : 14/07/03    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_CONF_H__
#define __HOST_CONF_H__



#define MAX_ADDR_LENGTH    32            /* ipv4 addr */
#define MAX_ID_LENGTH      32
#define MAX_DB_SCHEMA      32

#define IS_CHECK_OFF       0x00
#define IS_CHECK_ON        0x01


typedef struct {
    unsigned int   systemidx;
    unsigned int   systemid;
    unsigned int   systemaddr;
    unsigned short systemport;
    unsigned short is_check;
    unsigned short link_status;       /* 3m */
    char           systemname[MAX_STR_LENGTH];
} vmc_system_t;

typedef struct {
    char            EnvPath[MAX_STR_LENGTH * 2];

    char            nova_DbAddr[MAX_ADDR_LENGTH];
    char            nova_DbUserid[MAX_ID_LENGTH];
    char            nova_DbPass[MAX_ID_LENGTH];
    char            nova_DbName[MAX_DB_SCHEMA];
    unsigned int    nova_DbPort;

    char            orche_DbAddr[MAX_ADDR_LENGTH];
    char            orche_DbUserid[MAX_ID_LENGTH];
    char            orche_DbPass[MAX_ID_LENGTH];
    char            orche_DbName[MAX_DB_SCHEMA];
    char            orche_DbSchema[MAX_DB_SCHEMA];
    unsigned int    orche_DbPort;

    unsigned int    host_ip;
    unsigned short  host_port;
    char            host_name[MAX_NAME_LENGTH];
    unsigned int    host_id;

    int             bp_mode;
    int             vm_count;

    int             vmc_mode;
    int             tcp_mode;

    int             baremetal_mode;
    int             cloud_mode;

    vmc_system_t    vmc_system[MAX_SYSTEM_NUM];
    vmc_status_t    vmc_status[MAX_SYSTEM_NUM];

} host_config_t;

extern int read_config(char *path);
extern void FreeVariable(void);
extern int InitVariable(char *path);

extern host_config_t hostConf;
extern process_state_t basePState[MAX_VM_PROC_NUM];
extern vmc_stateinfo_t stateinfo[MAX_SYSTEM_NUM];


#endif /* __HOST_CONF_H__ */
