/*
 -------------------------------------------------------------------------
  File Name       : dbmb_main.c
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2011 KBell Inc.
  Descriptions    : DBMB(DataBase Manager Block) manager(init, event)
  History         : 14/07/15       Initial Coded
 -------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <errmsg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#include "dbmb.h"
#include "dbmb_main.h"


MYSQL *handler1     = NULL;
MYSQL *handler2     = NULL;
MYSQL *DBManager    = NULL; 
MYSQL *orchehandler = NULL; 

DBParm_t DBParm;
DBParm_t OrcheDBParm;


static void CloseDatabase1(void)
{
    if (handler1 != NULL) {
        mysql_close(handler1);
        handler1 = NULL;
    }
}

static void CloseDatabase2(void)
{
    if (handler2 != NULL) {
        mysql_close(handler2);
        handler2 = NULL;
    }
}

/* 20090709, by jeon */
static void CloseDBM(void)
{
    if (DBManager != NULL) {
        mysql_close(DBManager);
        DBManager = NULL;
    }
}

static int OpenDatabase(void)
{
    handler1 = mysql_init(handler1);
    if(!mysql_real_connect(handler1,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database: %s\n",
                mysql_error(handler1));
        CloseDatabase1();

        return -1;
    }

    handler2 = mysql_init(handler2);
    if(!mysql_real_connect(handler2,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database: %s\n",
                mysql_error(handler2));
        CloseDatabase2();

        return -1;
    }    

    DBManager = mysql_init(DBManager);
    if(!mysql_real_connect(DBManager,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database DBManager: %s\n",
                mysql_error(DBManager));
        CloseDBM();

        return -1;
    }

    return 0;
}

static int OpenDatabase1(void)
{
    handler1 = mysql_init(handler1);
    if(!mysql_real_connect(handler1,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database: %s\n",
                mysql_error(handler1));
        CloseDatabase1();

        return -1;
    }

    return 0;
}

static int OpenDatabase2(void)
{
    handler2 = mysql_init(handler2);
    if(!mysql_real_connect(handler2,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database: %s\n",
                mysql_error(handler2));
        CloseDatabase2();

        return -1;
    }

    return 0;
}

static int OpenDBM(void)
{
    DBManager = mysql_init(DBManager);
    if(!mysql_real_connect(DBManager,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database DBManager: %s\n",
                mysql_error(DBManager));
        CloseDBM();

        return -1;
    }

    return 0;
}

int ExecQuery1(char *query, int len)
{
    int retval;

#if 0
    xprint(FMS_DBM | FMS_INFO5, "%s(%d) : h1 query (%s)\n", __func__, __LINE__, query);
#endif
    if (handler1 == NULL) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : handler1 is NULL\n", __func__, __LINE__);
        if (OpenDatabase1() < 0) {
            if (OpenDatabase1() < 0) {
                /* NEXT_FIX, by icecom 090513 DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler1 Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler1 Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    retval = mysql_real_query(handler1, query, len);
    if (retval) {
        retval = mysql_errno(handler1);
        xprint(FMS_DBM | FMS_ERROR, "%s(%d : %d) : %s\n", __func__, __LINE__, retval, mysql_error(handler1));

        /* HSKANG */
        if (retval == CR_SERVER_GONE_ERROR || retval == CR_SERVER_LOST || retval == CR_UNKNOWN_ERROR || retval == CR_COMMANDS_OUT_OF_SYNC) {
            CloseDatabase1();
            if (OpenDatabase1() < 0) {
                /* NEXT_FIX, DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler1 Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler1 Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : query = %s\n", __func__, __LINE__, query);
    }

    return retval;
}

int ExecQuery2(char *query, int len)
{
    int retval;

#if 0
    xprint(FMS_DBM | FMS_INFO5, "%s(%d) : h2 query (%s)\n", __func__, __LINE__, query);
#endif
    if (handler2 == NULL) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : handler2 is NULLn", __func__, __LINE__);
        if (OpenDatabase2() < 0) {
            if (OpenDatabase2() < 0) {
                /* NEXT_FIX, DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler2 Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler2 Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    retval = mysql_real_query(handler2, query, len);
    if (retval) {
        retval = mysql_errno(handler2);
        xprint(FMS_DBM | FMS_ERROR, "%s(%d : %d) : %s\n", __func__, __LINE__, retval, mysql_error(handler2));

        /* HSKANG */
        if (retval == CR_SERVER_GONE_ERROR || retval == CR_SERVER_LOST || retval == CR_UNKNOWN_ERROR || retval == CR_COMMANDS_OUT_OF_SYNC) {
            CloseDatabase2();
            if (OpenDatabase2() < 0) {
                /* NEXT_FIX, DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler2 Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler2 Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : query = %s\n", __func__, __LINE__, query);
    }

    return retval;
}

int ExecDBM(char *query, int len)
{
    int retval;

#if 0
    xprint(FMS_DBM | FMS_INFO5, "%s(%d) : dbm query (%s)\n", __func__, __LINE__, query);
#endif
    if (DBManager == NULL) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : DBManager is NULLn", __func__, __LINE__);
        if (OpenDBM() < 0) {
            xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB DBManager Open failed. Restart Process...\n", __func__, __LINE__);
            return(-1);
        } else {
            xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB DBManager Re-Open Success.\n", __func__, __LINE__);
        }
    }

    retval = mysql_real_query(DBManager, query, len);
    if (retval) {
        retval = mysql_errno(DBManager);
        xprint(FMS_DBM | FMS_ERROR, "%s(%d : %d) : %s\n", __func__, __LINE__, retval, mysql_error(DBManager));

        /* HSKANG */
        if (retval == CR_SERVER_GONE_ERROR || retval == CR_SERVER_LOST || retval == CR_UNKNOWN_ERROR || retval == CR_COMMANDS_OUT_OF_SYNC) {
            CloseDBM();
            if (OpenDBM() < 0) {
                /* NEXT_FIX, by icecom 090513 DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB DBManager Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB DBManager Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : query = %s\n", __func__, __LINE__, query);
    }

    return retval;
}

int CommitID(void)
{
    int retval;
    char Query[16];

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, "commit");
    retval = ExecQuery1(Query, strlen(Query));

    return retval;
}

int CommitID2(void)
{
    int retval;
    char Query[16];

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, "commit");
    retval = ExecQuery2(Query, strlen(Query));

    return retval;
}

int CommitDBM(void)
{
    int retval;
    char Query[16];

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, "commit");
    retval = ExecDBM(Query, strlen(Query));

    return retval;
}

void CloseDatabase(void)
{
    if (handler1 != NULL) {
        mysql_close(handler1);
        handler1 = NULL;
    }

    if (handler2 != NULL) {
        mysql_close(handler2);
        handler2 = NULL;
    }

    if (DBManager != NULL) {
        mysql_close(DBManager);
        DBManager = NULL;
    }
}

/* DB connection and table create, by jeon */
int DatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port)
{
    int retval;

    memset((char *)&DBParm, 0x00, sizeof(DBParm_t));
    sprintf(DBParm.Address,  "%s", addr);
    sprintf(DBParm.UserID,   "%s", userid);
    sprintf(DBParm.Password, "%s", pass);
    sprintf(DBParm.DBName,   "%s", name);
    DBParm.Port = port;

    retval = OpenDatabase();

    xprint(FMS_DBM | FMS_LOOKS, "## ================================================================= ##\n");
    xprint(FMS_DBM | FMS_LOOKS, "      DB Address    : %s\n", DBParm.Address);
    xprint(FMS_DBM | FMS_LOOKS, "      DB UserID     : %s\n", DBParm.UserID);
    xprint(FMS_DBM | FMS_LOOKS, "      DB Schema     : %s\n", DBParm.DBName);
    xprint(FMS_DBM | FMS_LOOKS, "      DB Port       : %u\n", DBParm.Port);
    xprint(FMS_DBM | FMS_LOOKS, "## ================================================================= ##\n\n");

    return retval;
}

MYSQL *OpenDBHandler(void)
{
    MYSQL *h = NULL;

    h = mysql_init(h);
    if(!mysql_real_connect(h,
                DBParm.Address,
                DBParm.UserID,
                DBParm.Password,
                DBParm.DBName,
                DBParm.Port,
                NULL,
                0)) {
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database DBHandler: %s\n", mysql_error(h));
        CloseDBHandler(h);

        return NULL;
    }

    return h;
}

void CloseDBHandler(MYSQL *h)
{
    if (h != NULL) {
        mysql_close(h);
        h = NULL;
    }
}

int ExecQuery(MYSQL *h, char *query, int len)
{
    int retval;

    if (h == NULL) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : handler is NULL\n", __func__, __LINE__);
        h = OpenDBHandler();
        if (h == NULL) {
            xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler Open failed. Restart Process...\n", __func__, __LINE__);
            return(-1);
        } else {
            xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler Re-Open Success.\n", __func__, __LINE__);
        }
    }

    retval = mysql_real_query(h, query, len);
    if (retval) {
        retval = mysql_errno(h);
        xprint(FMS_DBM | FMS_ERROR, "%s(%d : %d) : %s\n", __func__, __LINE__, retval, mysql_error(h));

        /* HSKANG */
        if (retval == CR_SERVER_GONE_ERROR || retval == CR_SERVER_LOST || retval == CR_UNKNOWN_ERROR || retval == CR_COMMANDS_OUT_OF_SYNC) {
            CloseDBHandler(h);
            h = OpenDBHandler();
            if (h == NULL) {
                /* NEXT_FIX, by icecom 090513 DB Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB handler Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB handler Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : query = %s\n", __func__, __LINE__, query);
    }

    return retval;
}

int ExecCommit(MYSQL *h)
{
    int retval;
    char Query[16];

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, "commit");
    retval = ExecQuery(h, Query, strlen(Query));

    return retval;
}



int SelectAllVMStatus(vmc_status_t *vm)
{
    int retval, count = 0;
    char Query[MAX_QUERY_LENGTH];
    MYSQL_RES *res;
    MYSQL_ROW row;

    memset((char *)&Query, 0x00, sizeof(Query));
#if 0
    sprintf(Query, SELECT_ALL_VM_STATUS, OPENSTACK_VM_TB);
#else
    sprintf(Query, SELECT_ALL_VM_STATUS);
#endif

    retval = ExecQuery1(Query, strlen(Query));
    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : ExecQuery1 Error.\n", __func__, __LINE__);
        return -1;
    }

    res = mysql_store_result(handler1);
    for (;;) {

        if (count >= MAX_SYSTEM_NUM) {
            xprint(FMS_DBM | FMS_WARNING, "%s(%d) : count over (= %d)\n", __func__, __LINE__, count);
            break;
        }

        row = mysql_fetch_row(res);

        if (row == NULL) {
            break;
        } else {
            sscanf(row[0], "%u", &vm[count].vm_id);
            if (strcmp(row[1], "active") == 0) {
                vm[count].vm_status = STATUS_ACTIVE;
            } else if (strcmp(row[1], "build") == 0) {
                vm[count].vm_status = STATUS_BUILD;
            } else if (strcmp(row[1], "deleted") == 0) {
                vm[count].vm_status = STATUS_DELETED;
            } else if (strcmp(row[1], "stopped") == 0) {
                vm[count].vm_status = STATUS_STOPPED;
            } else if (strcmp(row[1], "paused") == 0) {
                vm[count].vm_status = STATUS_PAUSED;
            } else if (strcmp(row[1], "suspended") == 0) {
                vm[count].vm_status = STATUS_SUSPENDED;
            } 
            sscanf(row[2], "%hu", &vm[count].vm_power_status);
            if (row[3] != NULL) strcpy(&vm[count].vm_name[0], row[3]);
            if (row[4] != NULL) vm[count].vm_ipaddr = inet_addr(row[4]);
            if (row[5] != NULL) strcpy(&vm[count].vm_uuid[0], row[5]);
            if (row[6] != NULL) strcpy(&vm[count].vm_host_name[0], row[6]);
        }

        count ++;
    }

    mysql_free_result(res);
    return count;
}


/* ------------------------------------------------------------------------------------------- */
/* Orchestrator DB */
/* ------------------------------------------------------------------------------------------- */

/* 20090709, by jeon */
static void CloseOrcheDB(void)
{
    if (orchehandler != NULL) {
        mysql_close(orchehandler);
        orchehandler = NULL;
    }
}

static int OpenOrcheDB(void)
{
    orchehandler = mysql_init(orchehandler);
    if(!mysql_real_connect(orchehandler,
                OrcheDBParm.Address,
                OrcheDBParm.UserID,
                OrcheDBParm.Password,
                OrcheDBParm.DBName,
                OrcheDBParm.Port,
                NULL,
                0)){
        xprint(FMS_DBM | FMS_ERROR, "Failed to connect to database orchehandler: %s\n",
                mysql_error(orchehandler));
        CloseOrcheDB();
        return -1;
    }

    return 0;
}

int ExecOrcheDB(char *query, int len)
{
    int retval;

#if 0
    xprint(FMS_DBM | FMS_INFO5, "%s(%d) : dbm query (%s)\n", __func__, __LINE__, query);
#endif
    if (orchehandler == NULL) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : orchehandler is NULL \n", __func__, __LINE__);
        if (OpenOrcheDB() < 0) {
            xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB orchehandler Open failed. Restart Process...\n", __func__, __LINE__);
            return(-1);
        } else {
            xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB orchehandler Re-Open Success.\n", __func__, __LINE__);
        }
    }

    retval = mysql_real_query(orchehandler, query, len);
    if (retval) {
        retval = mysql_errno(orchehandler);
        xprint(FMS_DBM | FMS_ERROR, "%s(%d : %d) : %s\n", __func__, __LINE__, retval, mysql_error(orchehandler));

        /* HSKANG */
        if (retval == CR_SERVER_GONE_ERROR || retval == CR_SERVER_LOST || retval == CR_UNKNOWN_ERROR || retval == CR_COMMANDS_OUT_OF_SYNC) {
            CloseOrcheDB();
            if (OpenOrcheDB() < 0) {
                /* NEXT_FIX, Handler를 못얻어왔을때 재구동하는 스킴 적용할것 */
                xprint(FMS_DBM | FMS_FATAL, "%s(%d) : DB orchehandler Open failed. Restart Process...\n", __func__, __LINE__);
            } else {
                xprint(FMS_DBM | FMS_LOOKS, "%s(%d) : DB orchehandler Re-Open Success.\n", __func__, __LINE__);
            }
        }
    }

    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : query = %s\n", __func__, __LINE__, query);
    }

    return retval;
}

void OrcheCloseDatabase(void)
{
    if (orchehandler != NULL) {
        mysql_close(orchehandler);
        orchehandler = NULL;
    }
}

/* DB connection and table create, by jeon */
int OrcheDatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port)
{
    int retval;

    memset((char *)&OrcheDBParm, 0x00, sizeof(DBParm_t));
    sprintf(OrcheDBParm.Address,  "%s", addr);
    sprintf(OrcheDBParm.UserID,   "%s", userid);
    sprintf(OrcheDBParm.Password, "%s", pass);
    sprintf(OrcheDBParm.DBName,   "%s", name);
    OrcheDBParm.Port = port;

    retval = OpenOrcheDB();

    xprint(FMS_DBM | FMS_LOOKS, "## ================================================================= ##\n");
    xprint(FMS_DBM | FMS_LOOKS, "      DB Address    : %s\n", OrcheDBParm.Address);
    xprint(FMS_DBM | FMS_LOOKS, "      DB UserID     : %s\n", OrcheDBParm.UserID);
    xprint(FMS_DBM | FMS_LOOKS, "      DB Schema     : %s\n", OrcheDBParm.DBName);
    xprint(FMS_DBM | FMS_LOOKS, "      DB Port       : %u\n", OrcheDBParm.Port);
    xprint(FMS_DBM | FMS_LOOKS, "## ================================================================= ##\n\n");

    return retval;
}

int OrcheInsertVMStateInfo(vm_process_t *v)
{
    int retval = 0;
    char Query[MAX_QUERY_LENGTH];

    memset((char *)&Query, 0x00, sizeof(Query));

    sprintf(Query, INSERT_VM_STATE_INFO, ORCHESTRATOR_VM_STATE_TB,
                v->TimeStamp,
                v->vm_id, v->vm_name, v->host_id, v->host_ip,
                v->vm_type, v->vm_state, v->vm_power_state, v->vm_process_count,
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
                v->bp_state);

    retval = ExecOrcheDB(Query, strlen(Query));
    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : ExecOrcheDB Error.\n", __func__, __LINE__);
        return -1;
    }

    return retval;
}

int CheckTimeOrcheHLR(void)
{
    int retval;
    char Query[MAX_QUERY_LENGTH];
    MYSQL_RES *res;
    MYSQL_ROW row;

    memset((char *)&Query, 0x00, sizeof(Query));
    sprintf(Query, SELECT_CHECK_TIMES);

    retval = ExecOrcheDB(Query, strlen(Query));
    if (retval) {
        xprint(FMS_DBM | FMS_ERROR, "%s(%d) : ExecOrcheDB Error.\n", __func__, __LINE__);
        return retval;
    }

    res = mysql_use_result(orchehandler);
    row = mysql_fetch_row(res);

    if (row == NULL) {
        mysql_free_result(res);
        xprint(FMS_DBM | FMS_ERROR, " %s : result error\n", __func__);
        return 0;
    }

    sscanf(row[0], "%d", &retval);

    mysql_free_result(res);
    return retval;
}




