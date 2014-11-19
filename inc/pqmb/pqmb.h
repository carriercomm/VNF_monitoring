/*
 -------------------------------------------------------------------------
  File Name       : pqmb.h
  Author          : Hyeong-Ik Jeon
  Copyright       : Copyright (C) 2007 KBell Inc.
  Descriptions    : DBMB(DataBase Manager Block) Library API's
  History         : 14/08/15       Initial Coded
 -------------------------------------------------------------------------
*/

#ifndef __PQMB_H__
#define __PQMB_H__

#ifndef TRUE
#define TRUE     1
#endif

#ifndef FALSE
#define FALSE    0
#endif

#ifdef __TEST__
#define BYPASS_VM_STATE_INFO_TB        "bypass_vm_state_info"
#else
#define BYPASS_VM_STATE_INFO_TB        "bypass.bypass_vm_state_info"
#endif



/* ------------------------------------------------------------------------ */
/*     MAIN                                                                 */
/* ------------------------------------------------------------------------ */
extern void PQ_CloseDatabase(void);
extern int PQ_DatabaseInit(char *addr, char *userid, char *pass, char *name, unsigned int port);

extern int PQ_CheckPgHandler1(void);

extern int PQ_SelectCountVMStateInfo(vm_process_t *v);
extern int PQ_InsertVMStateInfo(vm_process_t *v);
extern int PQ_UpdateVMStateInfo(vm_process_t *v);
extern int PQ_CheckVMStateInfo(vm_process_t *v);


#endif /* __PQMB_H__ */
