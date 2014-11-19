/*
 -------------------------------------------------------------------------
  File Name       : pqmb_main.h
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2014 KBell Inc.
  Descriptions    : PQMB(DataBase Manager Block) manager header (init, event)
  History         : 14/07/10       Initial Coded
 -------------------------------------------------------------------------
*/

#ifndef __PQMB_MAIN_H__
#define __PQMB_MAIN_H__



#define MAX_QUERY_LENGTH      (1024*4)
#define MAX_DBPARM_LENGTH     128

#define SELECT_COUNT_VM_STATE_INFO "SELECT COUNT(*) FROM %s WHERE create_time = '%s' AND vm_id = '%s'"

#define INSERT_VM_STATE_INFO  "INSERT INTO %s \
(create_time, vm_id, vm_name, host_name, vm_type, vm_state, vm_power_state, \
vm_process1_name, vm_process1_state, vm_process2_name, vm_process2_state, \
vm_process3_name, vm_process3_state, vm_process4_name, vm_process4_state, \
vm_process5_name, vm_process5_state, vm_process6_name, vm_process6_state, \
vm_process7_name, vm_process7_state, vm_process8_name, vm_process8_state, \
vm_process9_name, vm_process9_state, vm_process10_name, vm_process10_state, \
vm_process11_name, vm_process11_state, vm_process12_name, vm_process12_state, \
bypass_mode_state, host_ip, process_count) \
VALUES ('%s', '%s', '%s', '%s', %hu, %hu, %hu, \
'%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, \
'%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, '%s', %hu, \
%hu, '%s', %hu)"

#define UPDATE_VM_STATE_INFO  "UPDATE %s SET \
create_time = '%s', vm_id = '%s', vm_name = '%s', host_name = '%s', \
vm_type = %hu, vm_state = %hu, vm_power_state = %hu, \
vm_process1_name = '%s', vm_process1_state = %hu, vm_process2_name = '%s', vm_process2_state = %hu, \
vm_process3_name = '%s', vm_process3_state = %hu, vm_process4_name = '%s', vm_process4_state = %hu, \
vm_process5_name = '%s', vm_process5_state = %hu, vm_process6_name = '%s', vm_process6_state = %hu, \
vm_process7_name = '%s', vm_process7_state = %hu, vm_process8_name = '%s', vm_process8_state = %hu, \
vm_process9_name = '%s', vm_process9_state = %hu, vm_process10_name = '%s', vm_process10_state = %hu, \
vm_process11_name = '%s', vm_process11_state = %hu, vm_process12_name = '%s', vm_process12_state = %hu, \
bypass_mode_state = %hu, host_ip = '%s', process_count = %hu \
WHERE create_time = '%s' AND vm_id = '%s'"

#define SELECT_PROCESS_COUNT_VM_STATE_INFO "SELECT process_count FROM %s WHERE create_time = '%s' AND vm_id = '%s'"




typedef struct {
    char            Address[MAX_DBPARM_LENGTH];
    char            UserID[MAX_DBPARM_LENGTH];
    char            Password[MAX_DBPARM_LENGTH];
    char            DBName[MAX_DBPARM_LENGTH];
    unsigned int    Port;
} PQ_DBParm_t;

extern int PQ_ExecQuery1(char *query, int len);
extern int PQ_ExecSelectQuery1(char *query, PGresult **res);
extern int PQ_ExecQuery2(char *query, int len);
extern int PQ_ExecSelectQuery2(char *query, PGresult **res);
extern int PQ_ExecDBM(char *query, int len);
extern int PQ_ExecSelectQueryDBM(char *query, PGresult **res);


#endif /* __PQMB_MAIN_H__ */
