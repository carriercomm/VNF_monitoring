/* -------------------------------------------------------------------------
  File Name       : bpapp.h
  Author          : Hyeong-Ik Jeon 
  Copyright       : Copyright (C) 2005 - 2014 KBell Inc.
  Descriptions    : BPAPP API Header
  History         : 14/07/04       Initial Coded
 ------------------------------------------------------------------------- */

#ifndef __BPAPP_H__
#define __BPAPP_H__

/* ------------------------------------------------------------------------ */
/* Define Value                                                             */
/* ------------------------------------------------------------------------ */
#define MAX_DATA_LENGTH              1400
#define MAX_INFO_LENGTH              256
#define MAX_STR_LENGTH               256
#define MAX_SYSTEM_NUM               128
#define MAX_NAME_LENGTH              32
#define MAX_TIME_LENGTH              32
#define MAX_VM_PROC_NUM              12    /* MAX Process Number */

#define DEFAULT_TCP_COMHDR_LENGTH    (int)(sizeof(common_tcpmsg_t))

/* Timer Key (IPC) */
#define VMC_TMRCMD_QUEUE_KEY         0x00007001

/* vm_status */
#define STATUS_ACTIVE                0x01 /* ACTIVE    */
#define STATUS_BUILD                 0x02 /* BUILD     */
#define STATUS_DELETED               0x03 /*           */
#define STATUS_STOPPED               0x04 /* SHUTOFF   */
#define STATUS_PAUSED                0x05 /* PAUSED    */
#define STATUS_SUSPENDED             0x06 /* SUSPENDED */

/* vm_power_status */
#define PW_STATUS_NOSTATUS           0x00 /* NOSTATUS       */
#define PW_STATUS_RUNNING            0x01 /* Running        */
#define PW_STATUS_BLOCKED            0x02 /* Blocked        */
#define PW_STATUS_PAUSED             0x03 /* Á¤ÁöÁß(Paused) */
#define PW_STATUS_SHUTDOWN           0x04 /* Shutdown       */

#define CTRL_BYPASS_MODE_ON          "%s/bin/bpctl_util all set_bypass on"
#define CTRL_BYPASS_MODE_OFF         "%s/bin/bpctl_util all set_bypass off"
#define CTRL_BYPASS_WATCHDOG_SET     "%s/bin/bpctl_util all set_bypass_wd %d"
#define CTRL_BYPASS_WATCHDOG_RESET   "%s/bin/bpctl_util all reset_bypass_wd"


/* Process Status */
#define PROCESS_STATE_UP             0x01
#define PROCESS_STATE_DOWN           0x02

/* VM Type */
#define VM_TYPE_UNKNOWN              0x00
#define VM_TYPE_UTM                  0x01
#define VM_TYPE_IPS                  0x02
#define VM_TYPE_HOST                 0x03




/* ------------------------------------------------------------------------ */
/* Message Default Type                                                     */
/* ------------------------------------------------------------------------ */
/* HOST <--> VMCT */
#define MSG_TYPE_BASE                0x0000   /* reserved     */
#define VM_PROC_STATE_REQ            0x0001   /* vm_proc_state_msg */
#define VM_PROC_STATE_RSP            0x0002   /*  */

#define BPAPP_MSG_BASE               0xFFFF   /* reserved     */


/* ------------------------------------------------------------------------ */
/* Network Byte Order                                                       */
/* ------------------------------------------------------------------------ */
#define NBO_HTONL(x)               htonl(x)
#define NBO_HTONS(x)               htons(x)
#define NBO_NTOHL(x)               ntohl(x)
#define NBO_NTOHS(x)               ntohs(x)

#define TCP_MSG_HTON(sMsg)                          \
{                                                   \
    (sMsg)->SystemID = NBO_HTONL((sMsg)->SystemID); \
    (sMsg)->MsgType  = NBO_HTONS((sMsg)->MsgType);  \
    (sMsg)->MsgSize  = NBO_HTONS((sMsg)->MsgSize);  \
}

#define TCP_MSG_NTOH(rMsg)                          \
{                                                   \
    (rMsg)->SystemID = NBO_NTOHL((rMsg)->SystemID); \
    (rMsg)->MsgType  = NBO_NTOHS((rMsg)->MsgType);  \
    (rMsg)->MsgSize  = NBO_NTOHS((rMsg)->MsgSize);  \
}

/* ------------------------------------------------------------------------ */
/* Define struct                                                            */
/* ------------------------------------------------------------------------ */
typedef struct {
     unsigned char  process_name[MAX_NAME_LENGTH * 2];
     unsigned int   process_state;        /* Process Status */
} __attribute__((__packed__)) process_state_t;

typedef struct {
    unsigned int    vm_id;
    unsigned int    vm_ipaddr;
    unsigned short  vm_status;
    unsigned short  vm_power_status;
    char            vm_name[MAX_NAME_LENGTH];
    char            vm_uuid[MAX_NAME_LENGTH * 4];
    char            vm_host_name[MAX_NAME_LENGTH];
} __attribute__((packed)) vmc_status_t;

typedef struct {
    unsigned int    vm_ip;  
    unsigned short  vm_type;
    unsigned short  pcount;

    /* only baremetal */
    unsigned char   vm_id[MAX_NAME_LENGTH * 2];
    unsigned char   vm_name[MAX_NAME_LENGTH];
    unsigned char   manage_ip[MAX_NAME_LENGTH / 2];

    process_state_t pstate[MAX_VM_PROC_NUM];
} __attribute__((__packed__)) vmc_stateinfo_t;



/* ------------------------------------------------------------------------ */
/* Message struct                                                           */
/* ------------------------------------------------------------------------ */
/* common TCP header */
typedef struct {
    unsigned int      TpktHdr;
    unsigned int      SystemID;
    unsigned short    MsgType;
    unsigned short    MsgSize;
} __attribute__((packed)) common_tcpmsg_t;

/* VM_PROC_STATE_REQ */
typedef struct {
    unsigned int       vm_ipaddress;
    unsigned int       vm_type;
    unsigned int       pcount;

    /* only baremetal */
    unsigned char      vm_id[MAX_NAME_LENGTH * 2];
    unsigned char      vm_name[MAX_NAME_LENGTH];
    unsigned char      manage_ip[MAX_NAME_LENGTH / 2];

    process_state_t    pstate[MAX_VM_PROC_NUM];
} __attribute__((packed)) vm_proc_state_msg;

/* Orchestrator, 20140731, 20140811, by jeon */
typedef struct {
     unsigned char     TimeStamp[MAX_TIME_LENGTH];
     unsigned char     vm_id[MAX_NAME_LENGTH * 2];    /* by  jeon */
     unsigned char     vm_name[MAX_NAME_LENGTH * 4];  /* by  jeon */
     unsigned char     host_id[MAX_NAME_LENGTH * 4];  /* by  jeon */
     unsigned char     host_ip[MAX_NAME_LENGTH / 2];
     unsigned short    vm_type;                       /* VM Type  */
     unsigned short    vm_state;                      /* VM state */
     unsigned short    vm_power_state;                /* VM state */
     unsigned short    bp_state;                      /* VM state */
     unsigned short    vm_process_count;              /* VM Process Count */
     process_state_t   pstate[MAX_VM_PROC_NUM];
} __attribute__((packed)) vm_process_t;

#endif /* __BPAPP_H__*/


