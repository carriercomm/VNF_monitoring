/*
 -------------------------------------------------------------------------
  File Name       : dbmb.h
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2007 KBell Inc.
  Descriptions    : DBMB(DataBase Manager Block) Library API's
  History         : 14/08/15       Initial Coded
 -------------------------------------------------------------------------
*/

#ifndef __DBMB_H__
#define __DBMB_H__

#ifndef TRUE
#define TRUE     1
#endif

#ifndef FALSE
#define FALSE    0
#endif

/* TB NAME */
#define OPENSTACK_VM_TB                "instances"
#define ORCHESTRATOR_VM_STATE_TB       "bypass_vm_state_info"


/* ------------------------------------------------------------------------ */
/*     MAIN                                                                 */
/* ------------------------------------------------------------------------ */
extern void CloseDatabase(void);
extern int DatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port);

extern int SelectAllVMStatus(vmc_status_t *vm);


/* ------------------------------------------------------------------------ */
/*     Orchestrator                                                         */
/* ------------------------------------------------------------------------ */
extern void OrcheCloseDatabase(void);
extern int OrcheDatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port);

extern int OrcheInsertVMStateInfo(vm_process_t *v);
extern int CheckTimeOrcheHLR(void);





#endif /* __DBMB_H__ */
