/*-------------------------------------------------------------------------
  File Name       : pqma_main.c
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2007 KBell Inc.
  Descriptions    : DBMB(DataBase Manager Block) manager(init, event)
  History         : 10/05/10       Initial Coded
  -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if 0
#include <errmsg.h>
#endif
#include <libpq-fe.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#include "pqmb.h"
#include "pqmb_main.h"


PGconn *pgconn1   = NULL;
PGconn *pgconn2   = NULL;
PGconn *pgManager = NULL;

PQ_DBParm_t PQ_DBParm;
char DBPort[MAX_DBPARM_LENGTH];


/*-------------------------------------------------------------------------
   INIT
 -------------------------------------------------------------------------*/
static int OpenDatabase(void)
{
    pgconn1 = PQsetdbLogin(PQ_DBParm.Address, DBPort, NULL, NULL, PQ_DBParm.DBName, PQ_DBParm.UserID, PQ_DBParm.Password);
    if (PQstatus(pgconn1) == CONNECTION_BAD) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Failed to connect to pgsql database %s\n", __FUNCTION__, __LINE__, PQerrorMessage(pgconn1));
        return (-1);
    }
    xprint(FMS_PQM | FMS_INFO1, "PostgreDB Conn1 Success\n");

    pgconn2 = PQsetdbLogin(PQ_DBParm.Address, DBPort, NULL, NULL, PQ_DBParm.DBName, PQ_DBParm.UserID, PQ_DBParm.Password);
    if (PQstatus(pgconn2) == CONNECTION_BAD) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Failed to connect to pgsql database %s\n", __FUNCTION__, __LINE__, PQerrorMessage(pgconn2));
        return (-1);
    }
    xprint(FMS_PQM | FMS_INFO1, "PostgreDB Conn2 Success\n");

    pgManager = PQsetdbLogin(PQ_DBParm.Address, DBPort, NULL, NULL, PQ_DBParm.DBName, PQ_DBParm.UserID, PQ_DBParm.Password);
    if (PQstatus(pgManager) == CONNECTION_BAD) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Failed to connect to pgsql database %s\n", __FUNCTION__, __LINE__, PQerrorMessage(pgManager));
        return (-1);
    }
    xprint(FMS_PQM | FMS_INFO1, "PostgreDB pgManager Success\n");

    return 0;
}

void PQ_CloseDatabase(void)
{
    if (pgconn1 != NULL) {
        PQfinish(pgconn1);
        pgconn1 = NULL;
    }

    if (pgconn2 != NULL) {
        PQfinish(pgconn2);
        pgconn2 = NULL;
    }

    if (pgManager != NULL) {
        PQfinish(pgManager);
        pgManager = NULL;
    }
}

/* DB connection and table create, by jeon */
int PQ_DatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port)
{
    int retval;

    memset((char *)&PQ_DBParm, 0x00, sizeof(PQ_DBParm_t));
    memset((char *)&DBPort, 0x00, sizeof(MAX_DBPARM_LENGTH));

    sprintf(PQ_DBParm.Address,  "%s", addr);
    sprintf(PQ_DBParm.UserID,   "%s", userid);
    sprintf(PQ_DBParm.Password, "%s", pass);
    sprintf(PQ_DBParm.DBName,   "%s", name);
    sprintf(DBPort,          "%u", port);

    retval = OpenDatabase();

    return retval;
}

/*-------------------------------------------------------------------------
   Query Default
 -------------------------------------------------------------------------*/
int PQ_ExecQuery1(char *query, int len)
{
    PGresult *res = NULL;

    res = PQexec(pgconn1, query);
	if ((!res) || (PQresultStatus(res) != PGRES_COMMAND_OK)) {
		xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Conn1 result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(res));
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : query : %s\n", __FUNCTION__, __LINE__, query);
        PQclear(res);
        return (-1);
    }

    PQclear(res);
    return (0);
}

int PQ_ExecSelectQuery1(char *query, PGresult **res)
{
    *res = PQexec(pgconn1, query);
    if ((!*res) || (PQresultStatus(*res) != PGRES_TUPLES_OK)) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Conn1 result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(*res));
        return (-1);
    }
    return (0);
}

int PQ_ExecQuery2(char *query, int len)
{
    PGresult *res = NULL;

    res = PQexec(pgconn2, query);
    if ((!res) || (PQresultStatus(res) != PGRES_COMMAND_OK)) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Conn2 result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(res));
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : query : %s\n", __FUNCTION__, __LINE__, query);
        PQclear(res);
        return (-1);
    }

    PQclear(res);
    return (0);
}

int PQ_ExecSelectQuery2(char *query, PGresult **res)
{
    *res = PQexec(pgconn2, query);
    if ((!*res) || (PQresultStatus(*res) != PGRES_TUPLES_OK)) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Conn2 result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(*res));
        return (-1);
    }
    return (0);
}

/* Add by icecom 090513 */
int PQ_ExecDBM(char *query, int len)
{
    PGresult *res = NULL;

    res = PQexec(pgManager, query);
    if ((!res) || (PQresultStatus(res) != PGRES_COMMAND_OK)) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : pgManager result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(res));
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : query : %s\n", __FUNCTION__, __LINE__, query);
        PQclear(res);
        return (-1);
    }

    PQclear(res);
    return (0);
}

int PQ_ExecSelectQueryDBM(char *query, PGresult **res)
{
    *res = PQexec(pgManager, query);
    if ((!*res) || (PQresultStatus(*res) != PGRES_TUPLES_OK)) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : pgManager result Err : %s\n", __FUNCTION__, __LINE__, PQresultErrorMessage(*res));
        return (-1);
    }
    return (0);
}

/*-------------------------------------------------------------------------
   Query1 (Statistic & Session & Configuration)
 -------------------------------------------------------------------------*/
int PQ_CheckPgHandler1(void)
{
    int retval;
    char Query[MAX_QUERY_LENGTH];
    PGconn *h;
    PGresult *res = NULL;

    h = pgconn1;

    if (h == NULL) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : Handler is NULL\n", __FUNCTION__, __LINE__);
        return (-1);
    }

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, "%s", "SELECT NOW()");

    retval = PQ_ExecSelectQuery1(Query, &res);
    if (retval < 0) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : PQ_ExecSelectQuery1 Error\n", __FUNCTION__, __LINE__);
        PQclear(res);
        return (-1);
    }

    xprint(FMS_PQM | FMS_INFO5, "%s(%d) : time = [%s] \n", __FUNCTION__, __LINE__, PQgetvalue(res, 0, 0));

    PQclear(res);
    return (0);
}

int PQ_SelectCountVMStateInfo(vm_process_t *v)
{
    int retval = 0;
    char Query[MAX_QUERY_LENGTH];
    PGresult *res = NULL;

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, SELECT_COUNT_VM_STATE_INFO, BYPASS_VM_STATE_INFO_TB,
                v->TimeStamp, v->vm_id);

    retval = PQ_ExecSelectQuery1(Query, &res);
    if (retval < 0) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : PQ_ExecSelectQuery1 Error\n", __FUNCTION__, __LINE__);
        PQclear(res);
        return (-1);
    }

    retval = atoi(PQgetvalue(res, 0, 0));

    PQclear(res);

    return (retval);
}


int PQ_InsertVMStateInfo(vm_process_t *v)
{
    int retval = 0;
    char Query[MAX_QUERY_LENGTH];

    memset((char *)&Query, 0x00, sizeof(Query));

    sprintf(Query, INSERT_VM_STATE_INFO, BYPASS_VM_STATE_INFO_TB,
                v->TimeStamp,
                v->vm_id, v->vm_name, v->host_id,
                v->vm_type, v->vm_state, v->vm_power_state, 
                v->pstate[0].process_name, v->pstate[0].process_state,
                v->pstate[1].process_name, v->pstate[1].process_state,
                v->pstate[2].process_name, v->pstate[2].process_state,
                v->pstate[3].process_name, v->pstate[3].process_state,
                v->pstate[4].process_name, v->pstate[4].process_state,
                v->pstate[5].process_name, v->pstate[5].process_state,
                v->pstate[6].process_name, v->pstate[6].process_state,
                v->pstate[7].process_name, v->pstate[7].process_state,
                v->pstate[8].process_name, v->pstate[8].process_state,
                v->pstate[9].process_name, v->pstate[9].process_state,
                v->pstate[10].process_name, v->pstate[10].process_state,
                v->pstate[11].process_name, v->pstate[11].process_state,
                v->bp_state, v->host_ip, v->vm_process_count);

    retval = PQ_ExecQuery1(Query, strlen(Query));
    return retval;
}

int PQ_UpdateVMStateInfo(vm_process_t *v)
{
    int retval = 0;
    char Query[MAX_QUERY_LENGTH];

    memset((char *)&Query, 0x00, sizeof(Query));

    sprintf(Query, UPDATE_VM_STATE_INFO, BYPASS_VM_STATE_INFO_TB,
                v->TimeStamp,
                v->vm_id, v->vm_name, v->host_id,
                v->vm_type, v->vm_state, v->vm_power_state,
                v->pstate[0].process_name, v->pstate[0].process_state,
                v->pstate[1].process_name, v->pstate[1].process_state,
                v->pstate[2].process_name, v->pstate[2].process_state,
                v->pstate[3].process_name, v->pstate[3].process_state,
                v->pstate[4].process_name, v->pstate[4].process_state,
                v->pstate[5].process_name, v->pstate[5].process_state,
                v->pstate[6].process_name, v->pstate[6].process_state,
                v->pstate[7].process_name, v->pstate[7].process_state,
                v->pstate[8].process_name, v->pstate[8].process_state,
                v->pstate[9].process_name, v->pstate[9].process_state,
                v->pstate[10].process_name, v->pstate[10].process_state,
                v->pstate[11].process_name, v->pstate[11].process_state,
                v->bp_state, v->host_ip, v->vm_process_count, 
                v->TimeStamp, v->vm_id);

    retval = PQ_ExecQuery1(Query, strlen(Query));
    return retval;
}

int PQ_CheckVMStateInfo(vm_process_t *v)
{
    int retval = 0;
    char Query[MAX_QUERY_LENGTH];
    PGresult *res = NULL;

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, SELECT_PROCESS_COUNT_VM_STATE_INFO, BYPASS_VM_STATE_INFO_TB,
                v->TimeStamp, v->vm_id);

    retval = PQ_ExecSelectQuery1(Query, &res);
    if (retval < 0) {
        xprint(FMS_PQM | FMS_ERROR, "%s(%d) : PQ_ExecSelectQuery1 Error\n", __FUNCTION__, __LINE__);
        PQclear(res);
        return (-1);
    }

    retval = atoi(PQgetvalue(res, 0, 0));

    PQclear(res);

    return (retval);
}

