/* -------------------------------------------------------------------------
 * File Name   : host_debug.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : HOST(Host Server) debug.
 * History     : 13/07/02    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_DEBUG_H__
#define __HOST_DEBUG_H__

#ifdef __DEBUG__

extern void DebugSizeofStruct(void);
extern void DebugHostInfo(void);
extern void DebugVmStatus(void);
extern void DebugVmProcessState(vm_proc_state_msg *s);
extern void DebugVmcStateInfo(void);

#endif /* __DEBUG__ */


#endif /* __HOST_DEBUG_H__ */

