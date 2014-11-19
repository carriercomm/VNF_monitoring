/*
 -------------------------------------------------------------------------
  File Name       : dbmb_main.h
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2014 KBell Inc.
  Descriptions    : DBMB(DataBase Manager Block) manager(init, event)
  History         : 14/07/15       Initial Coded
 -------------------------------------------------------------------------
*/
#ifndef __DBMB_MAIN_H__
#define __DBMB_MAIN_H__

#define OPEN_DB            0x01

#define GET_ADDR           0x01
#define GET_USER           0x02
#define GET_PASSWORD       0x03
#define GET_NAME           0x04
#define GET_PORT           0x05

/* ==============================================================================================================*/
#define MAX_QUERY_LENGTH   (1024*4)
#define MAX_DB_TABLE       64
#define MAX_DBM_INFO       16

#define SELECT_CHECK_TIMES        "SELECT TIME_TO_SEC(now())"

#ifdef __TEST__
#define SELECT_ALL_VM_STATUS      "SELECT T3.id, T3.vm_state, T3.power_state, T3.display_name, T4.ip_address, T3.uuid, T3.host FROM \
(SELECT id, vm_state, power_state, display_name, uuid, host FROM nova.instances WHERE deleted = 0) T3 \
LEFT JOIN \
(SELECT T1.*, T2.ip_address FROM \
(SELECT id, device_id FROM neutron_ml2.ports) T1 \
LEFT JOIN (SELECT * FROM neutron_ml2.ipallocations) T2 \
ON T1.id = T2.port_id) T4 \
ON T3.uuid = T4.device_id"
#else
#define SELECT_ALL_VM_STATUS      "SELECT T3.id, T3.vm_state, T3.power_state, T3.display_name, T4.ip_address, T3.uuid, T3.host FROM \
(SELECT id, vm_state, power_state, display_name, uuid, host FROM nova.instances WHERE deleted = 0) T3 \
LEFT JOIN \
(SELECT T1.*, T2.ip_address FROM \
(SELECT id, device_id FROM neutron.ports) T1 \
LEFT JOIN (SELECT * FROM neutron.ipallocations) T2 \
ON T1.id = T2.port_id) T4 \
ON T3.uuid = T4.device_id"
#endif


#define INSERT_VM_STATE_INFO      "INSERT INTO %s \
VALUES ('%s', '%s', '%s', '%s', '%s', %hu, %hu, %hu, %hu, \
'%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, \
'%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, %hu)"



typedef struct {
    char           Address[128];
    char           UserID[128];
    char           Password[128];
    char           DBName[128];
    unsigned int   Port;
} DBParm_t;


extern MYSQL *handler1;
extern MYSQL *handler2;
extern MYSQL *DBManager;
extern MYSQL *orchehandler;


extern int ExecQuery1(char *query, int len);
extern int ExecQuery2(char *query, int len);
extern int ExecDBM(char *query, int len);
extern int CommitID(void);
extern int CommitID2(void);
extern int CommitDBM(void);

extern MYSQL *OpenDBHandler(void);
extern void CloseDBHandler(MYSQL *h);
extern int ExecQuery(MYSQL *h, char *query, int len);
extern int ExecCommit(MYSQL *h);

/* orchestrator */
extern int ExecOrcheDB(char *query, int len);

#endif /* __DBMB_MAIN_H__ */
