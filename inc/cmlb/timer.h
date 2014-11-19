/*
 -------------------------------------------------------------------------
  File Name       : timer.h
  Author          : Hyo-Seok Kang
  Copyright       : Copyright (C) 2005 by KBell Inc.
  Descriptions    : ≈∏¿Ã∏”

  History         : 05/08/17       Initial Coded
 -------------------------------------------------------------------------
*/

#ifndef __TIMER_H__
#define __TIMER_H__

#define TMR_CALLBACK      1    /* callback timer         */
#define TMR_SOCKET        2    /* udp socket timer       */
#define TMR_MSGQUEUE      3    /* message queue timer    */

#define TMR_STACK_USED    1    /* tmrStop used stack     */
#define TMR_TIMER_USED    2    /* tmrStop used timer     */

#define TMR_REPEAT        0x1001

typedef struct {
    int timerId;               /* timer id               */
    int argc1;                 /* local parameter (arg1) */
    int argc2;                 /* local parameter (arg2) */
    int argc3;                 /* local parameter (arg3) */
} tmr_expire_t;

typedef void (*CB)(int, int, int, int);    /* timer callback function */

extern int timer_init(int maxTmr, int port);
extern int timer_start(int msecs, CB callback, int arg1, int arg2, int arg3, int type);
extern int timer_stop(int timerId, int mode);
extern int timer_reset(int timerId, int msecs);

#endif  /* __TIMER_H__ */
