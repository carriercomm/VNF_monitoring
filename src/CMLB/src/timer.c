/*
 -------------------------------------------------------------------------
  File Name       : timer.c
  Author          : Hyo-Seok Kang
  Copyright       : Copyright (C) 2005 by KBell Inc.
  Descriptions    : 타이머 

  History         : 05/08/17       Initial Coded
 -------------------------------------------------------------------------
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "xprint.h"
#include "timer.h"

#ifndef FALSE
#define FALSE          0
#endif

#ifndef TRUE
#define TRUE           1
#endif

#define TMR_STOP       0        /* timer stop */
#define TMR_RUN        1        /* timer run */

#define NANO_TICK_CNT  99000000 /* nanosleep tick */

#define timer_mutex_lock(a) {                \
    if (pthread_mutex_lock(a) != 0) {        \
        xprint(FMS_ERROR | gMask, "pthread_mutex_lock failed(%d at %s)\n", __LINE__, __FUNCTION__);\
    }                        \
}
#define timer_mutex_unlock(a) {                \
    if (pthread_mutex_unlock(a) != 0) {        \
        xprint(FMS_ERROR | gMask, "pthread_mutex_unlock failed(%d at %s)\n", __LINE__, __FUNCTION__);\
    }                        \
}

typedef struct _timer_list {
    int    id;                /* timer id */
    int    period;            /* timer period */
    int    tv_sec;            /* start second */
    int    tv_usec;           /* start micro second */
    int    arg1;              /* argument 1 */
    int    arg2;              /* argument 2 */
    int    arg3;              /* argument 3 */
    CB     action;            /* expire callback function */
    struct _timer_list *next; /* timer list next */
    struct _timer_list *prev; /* timer list prev */
} timer_list;

typedef struct {
    int    flag;
    int    id;
} _id_list;

int        assign_ptr;
int        cancel_ptr;
int        comeback;
int        maxtnum;
int        tremainder;        /* tid remainder */
_id_list   *tid_list;

static timer_list *head, *tail; /* timer linked list */
pthread_mutex_t   time_mutex;   /* 타이머 mutex */
int        MaxTmrNum;           /* MaxTmrNum: 최대 타이머 갯수 */
int        tmrKey;              /* 타이머 expire시 메세지 전송할 큐키 */
int        fullcnt=0;

static int init_dlist(void);
static timer_list *search_timer_list(int id);
static int delete_timer_list(int id);
static int insert_timer_list(int id, int period, CB callback, int arg1, int arg2, int arg3);
/*
static void delete_all_timer_list(void);
*/
static int func_init_tid(int max);
static int func_assign_tid(void);
static int func_cancel_tid(int id);
static void tmr_expire_func(int timerId, int argc1, int argc2, int argc3);
static void cmp_timer_value(void);
static void *timer_task(void *argc);

static int init_dlist(void)
{
    head = (timer_list *)malloc(sizeof(timer_list));
    if (head == NULL) {
        xprint(FMS_FATAL | gMask, "malloc error(%s) at %d of %s\n", strerror(errno), __LINE__, __FUNCTION__);
        return(-1);
    }

    tail = (timer_list *)malloc(sizeof(timer_list));
    if (tail == NULL) {
        xprint(FMS_FATAL | gMask, "malloc error(%s) at %d of %s\n", strerror(errno), __LINE__, __FUNCTION__);
        return(-1);
    }

    head->prev = head;
    head->next = tail;
    tail->prev = head;
    tail->next = tail;

    return(0);
}

static timer_list *search_timer_list(int id)
{
    timer_list    *s;

    s = head->next;
    while(s != tail) {
        if (s->id == id) {
            break;
        }
        s = s->next;
    }

    return(s);
}

static int delete_timer_list(int id)
{
    timer_list    *s;

    if (id <= 0) {
        xprint(FMS_ERROR | gMask, "id(%d) is unavailable at %s\n", id, __FUNCTION__);
        return(-1);
    }

    s = search_timer_list(id);
    if (s != tail) {
        s->prev->next = s->next;
        s->next->prev = s->prev;
        free(s);
    } else {
        xprint(FMS_ERROR | gMask, "id(%d) can't search at %s\n", id, __FUNCTION__);
        return(-1);
    }

    return(0);
}

static int insert_timer_list(int id, int period, CB callback, int arg1, int arg2, int arg3)
{
    timer_list        *s;
    timer_list        *i;
    struct timeval    tp;

    i = (timer_list *)malloc(sizeof(*i));
    if (i == NULL) {
        xprint(FMS_ERROR | gMask, "malloc error(%s) at %s\n", strerror(errno), __FUNCTION__);
        return(-1);
    }

    if (gettimeofday(&tp, NULL) == -1) {
        xprint(FMS_ERROR | gMask, "gettimeofday error(%s) at %s\n", strerror(errno), __FUNCTION__);
        free(i);
        return(-1);
    }

    i->id      = id;
    i->period  = period;
    i->tv_sec  = tp.tv_sec;
    i->tv_usec = tp.tv_usec;
    i->arg1    = arg1;
    i->arg2    = arg2;
    i->arg3    = arg3;
    i->action  = callback;

    s = tail;
    if (s == head) {
        xprint(FMS_ERROR | gMask, "s equal head at %s\n", __FUNCTION__);
        free(i);
        return(-1);
    }

    s->prev->next = i;
    i->prev = s->prev;
    i->next = s;
    s->prev = i;

    return(0);
}

static int func_init_tid(int max)
{
    int     i;

    assign_ptr = 0;
    cancel_ptr = 0;
    comeback   = FALSE;
    maxtnum    = max;
    tremainder  = max;

    tid_list = (_id_list *)malloc(max * sizeof(_id_list));
    if (tid_list == NULL) {
        xprint(FMS_ERROR | gMask, "malloc error(%s) at %s\n", strerror(errno), __FUNCTION__);
        return(-1);
    }

    for(i = 0; i < max; i++) {
        tid_list[i].flag = FALSE;
        tid_list[i].id = i + 1;
    }

    return(0);
}

static int func_assign_tid(void)
{
    int    id;

    if (tremainder <= 0) {
        xprint(FMS_ERROR | gMask, "the remainder of timer not exist(%d, %d)\n", assign_ptr, cancel_ptr);
        return(-1);
    }

    if (comeback == FALSE) {
        if (tid_list[assign_ptr].flag == FALSE) {
            tid_list[assign_ptr].flag = TRUE;
            tremainder--;
            id = tid_list[assign_ptr++].id;
            if (assign_ptr >= maxtnum) {
                assign_ptr = 0;
                comeback = TRUE;
            }

            return(id);
        } else {
            xprint(FMS_ERROR | gMask, "Error assign_ptr(%d) tid(%d) used\n", assign_ptr, tid_list[assign_ptr].id);
        }
    } else {
        if ((assign_ptr - cancel_ptr) < 0) {
            if (tid_list[assign_ptr].flag == FALSE) {
                tid_list[assign_ptr].flag = TRUE;
                tremainder--;
                id = tid_list[assign_ptr++].id;
                if (assign_ptr >= maxtnum) {
                    assign_ptr = 0;
                    comeback = TRUE;
                }

                return(id);
            } else {
                xprint(FMS_ERROR | gMask, "Error assign_ptr(%d) tid(%d) used\n", assign_ptr, tid_list[assign_ptr].id);
            }
        } else {
            xprint(FMS_ERROR | gMask, "Error : assign_ptr < cancel_ptr\n");
            return(-1);
        }
    }

    return(-1);
}

static int func_cancel_tid(int id)
{
    if (id > 0 && id <= maxtnum) {
        if (comeback == FALSE) {
            if ((assign_ptr - cancel_ptr) > 0) {
                tid_list[cancel_ptr].id = id;
                tid_list[cancel_ptr++].flag = FALSE;
                tremainder++;
                if (tremainder > maxtnum) {
                    tremainder = maxtnum;
                }

                if (cancel_ptr >= maxtnum) {
                    cancel_ptr = 0;
                    comeback = FALSE;
                }

                return(0);
            } else {
                xprint(FMS_ERROR | gMask, "if comeback is FALSE, assign_ptr - cancel_ptr is rather than 0\n");
            }
        } else {
            tid_list[cancel_ptr].id = id;
            tid_list[cancel_ptr++].flag = FALSE;
            tremainder++;
            if (tremainder > maxtnum) {
                tremainder = maxtnum;
            }

            if (cancel_ptr >= maxtnum) {
                cancel_ptr = 0;
                comeback = FALSE;
            }

            return(0);
        }
    }

    return(-1);
}

static void tmr_expire_func(int timerId, int argc1, int argc2, int argc3)
{
    typedef struct {
        long    msgType;            /* primitive type */
        int     src_qid;            /* source queue id */
        int     dst_qid;            /* destnation queue id */
        int     datalen;            /* userData length */
        char    data[sizeof(tmr_expire_t)];    /* user data */
    } ipc_msg_t;

    ipc_msg_t    uiBuf;
    tmr_expire_t tmBuf;
    int msg_qid = -1;

    if ((msg_qid = msgget(tmrKey, 0)) == -1) {
        xprint(FMS_CLB | FMS_FATAL, "msgget failed at tmrExpire(%s)\n", strerror(errno));
        return;
    }

    tmBuf.timerId = timerId;
    tmBuf.argc1   = argc1;
    tmBuf.argc2   = argc2;
    tmBuf.argc3   = argc3;

    uiBuf.msgType = (long)argc1;        /* 초기 타이머 등록시 argc1을 메세지 타입으로 사용한다. */
    uiBuf.src_qid = msg_qid;
    uiBuf.dst_qid = msg_qid;
    uiBuf.datalen = sizeof(tmr_expire_t);
    memcpy(uiBuf.data, (char *)&tmBuf, sizeof(tmr_expire_t));

    if (msgsnd(msg_qid, (void *)&uiBuf, sizeof(tmr_expire_t) + 16, IPC_NOWAIT) < 0) {
        if ((fullcnt%100) == 0) {
            xprint(FMS_CLB | FMS_FATAL, "msgsnd failed at tmrExpire(%s)\n", strerror(errno));
        }
        fullcnt++;
        return;
    } else {
        fullcnt = 0;
    }

    return;
}

int timer_init(int maxTmr, int port)
{
    pthread_t        tId;
    pthread_attr_t    attr;

    if (maxTmr < 1) {
        xprint(FMS_FATAL | gMask, "unavailabe maxTmr(%d) at tmrInit\n", maxTmr);
        return(-1);
    }

    MaxTmrNum = maxTmr;
    tmrKey    = port;

    if (func_init_tid(MaxTmrNum) == -1) {
        xprint(FMS_FATAL | gMask, "func_init_tid failed at %s\n", __FUNCTION__);
        return(-1);
    }

    /* KBCS의 tmrCb = (timerCb *)malloc(csMaxTmr * sizeof(timerCb))과 동일, by jeon */
    if (init_dlist() == -1) {
        xprint(FMS_FATAL | gMask, "init_dlist failed at %s\n", __FUNCTION__);
        return(-1);
    }

    if (pthread_mutex_init(&time_mutex, NULL) != 0) {
        xprint(FMS_FATAL | gMask, "pthread_mutex_init error(%s) at %s\n", strerror(errno), __FUNCTION__);
        return(-1);
    }

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if ((pthread_create(&tId, &attr, timer_task, NULL)) != 0) {
        xprint(FMS_FATAL | gMask, "timer_task create failed(%s) at %s\n", strerror(errno), __FUNCTION__);
        return(-1);
    }

    return(0);
}

int timer_start(int msecs, CB callback, int arg1, int arg2, int arg3, int type)
{
    int idx, period;

    timer_mutex_lock(&time_mutex);

    if (msecs < 0) {
        xprint(FMS_ERROR | gMask, "unavailable msecs(%d) at %s\n", msecs, __FUNCTION__);
        timer_mutex_unlock(&time_mutex);
        return(-1);
    }

    if (type == TMR_CALLBACK && callback == NULL) {
        xprint(FMS_ERROR | gMask, "callback function is null at %s\n", __FUNCTION__);
        timer_mutex_unlock(&time_mutex);
        return(-1);
    }

    period = ((msecs / 1000) * 1000000) + ((msecs % 1000) * 1000);

    if ((idx = func_assign_tid()) == -1) {
        xprint(FMS_ERROR | gMask, "func_assign_tid failed at %s\n", __FUNCTION__);
        timer_mutex_unlock(&time_mutex);
        return(-1);
    }

    if (type == TMR_MSGQUEUE) {
        if (insert_timer_list(idx, period, tmr_expire_func, arg1, arg2, arg3) < 0) {
            xprint(FMS_ERROR | gMask, "insert_timer_list failed at %s\n", __FUNCTION__);
            timer_mutex_unlock(&time_mutex);
            return(-1);
        }
    } else if (type == TMR_SOCKET) {
    } else {
        if (insert_timer_list(idx, period, callback, arg1, arg2, arg3) < 0) {
            xprint(FMS_ERROR | gMask, "insert_timer_list failed at %s\n", __FUNCTION__);
            timer_mutex_unlock(&time_mutex);
            return(-1);
        }
    }

    timer_mutex_unlock(&time_mutex);

    return(idx);
}

int timer_stop(int timerId, int mode)
{
    if (mode == TMR_STACK_USED) {
        timer_mutex_lock(&time_mutex);
    }

    if (timerId < 1 || timerId > MaxTmrNum) {
        xprint(FMS_WARNING | gMask, "unavailable id(%d) at %s\n", timerId, __FUNCTION__);
        if (mode == TMR_STACK_USED) {
            timer_mutex_unlock(&time_mutex);
        }
        return(-1);
    }

    if (delete_timer_list(timerId) < 0) {
        xprint(FMS_WARNING | gMask, "delete_timer_list(%d) error at %s\n", timerId, __FUNCTION__);
        if (mode == TMR_STACK_USED) {
            timer_mutex_unlock(&time_mutex);
        }
        return(-1);
    }

    if (func_cancel_tid(timerId) == -1) {
        xprint(FMS_WARNING | gMask, "func_cancel_tid(%d) error at %s\n", timerId, __FUNCTION__);
        if (mode == TMR_STACK_USED) {
            timer_mutex_unlock(&time_mutex);
        }
        return(-1);
    }

    if (mode == TMR_STACK_USED) {
        timer_mutex_unlock(&time_mutex);
    }

    return(0);
}

int timer_reset(int timerId, int msecs)
{
    int            period=0;
    struct timeval    tp;
    timer_list        *s;

    timer_mutex_lock(&time_mutex);

    if (timerId < 1 || timerId > MaxTmrNum) {
        xprint(FMS_ERROR | gMask, "unavailable timerId(%d) at %s\n", timerId, __FUNCTION__);
        timer_mutex_unlock(&time_mutex);
        return(-1);
    }

    if (msecs < 0) {
        xprint(FMS_ERROR | gMask, "unavailable msecs(%d) at %s\n", msecs, __FUNCTION__);
        timer_mutex_unlock(&time_mutex);
        return(-1);
    }

    if (msecs != 0) {
        period = ((msecs / 1000) * 1000000) + ((msecs % 1000) * 1000);
    }

    s = search_timer_list(timerId);
    if (s != tail) {
        if (gettimeofday(&tp, NULL) < 0) {
            xprint(FMS_ERROR | gMask, "gettimeofday error(%s) at %s\n", strerror(errno), __FUNCTION__);
            timer_mutex_unlock(&time_mutex);
            return(-1);
        }

        s->id      = timerId;
        s->period  = period;
        s->tv_sec  = tp.tv_sec;
        s->tv_usec = tp.tv_usec;
    } else {
        xprint(FMS_ERROR | gMask, "not search id(%d) at %s\n", timerId, __FUNCTION__);
    }

    timer_mutex_unlock(&time_mutex);

    return(0);
}

static void cmp_timer_value(void)
{
    timer_list        *ent;
    struct timeval    tp;
    int            usecs;

    ent = head->next;            /* linked list start pointer    */

    while(ent != tail) {        /* if start point is not null   */
        timer_mutex_lock(&time_mutex);

        if (gettimeofday(&tp, NULL) == -1) {
            xprint(FMS_ERROR | gMask, "gettimeofday error at %s\n", __FUNCTION__);
            timer_mutex_unlock(&time_mutex);
            continue;
        }

        usecs = 0;
        tp.tv_usec += 10000;        /* timer delay complement(10ms) */

        if (tp.tv_sec > ent->tv_sec) {
            usecs = (tp.tv_sec - (ent->tv_sec + 1)) * 1000000;
            usecs += (1000000 - ent->tv_usec) + tp.tv_usec;
        } else {
            usecs = tp.tv_usec - ent->tv_usec;
        }

        if (usecs > ent->period) {
            if (ent->action != NULL) {
                ent->action(ent->id, ent->arg1, ent->arg2, ent->arg3);
#if 0
                /* org */
                timer_stop(ent->id, TMR_TIMER_USED);
#else
                /* TIMER REPEAT, 2008.12.15, by jeon */
                /* 
                 * arg2 : msec       (repeat period)
                 * arg3 : TMR_REPEAT (repeat flag)  
                 */
                if (ent->arg3 == TMR_REPEAT) {
                    timer_mutex_unlock(&time_mutex);
                    if (timer_reset(ent->id, ent->arg2) < 0) {
                        xprint(FMS_ERROR | gMask, "Timer Repeat is Error %s\n", __FUNCTION__);
                    }
                    timer_mutex_lock(&time_mutex);
                } else {
                    timer_stop(ent->id, TMR_TIMER_USED);
                }
#endif
            } else {
                xprint(FMS_ERROR | gMask, "action is null at %s\n", __FUNCTION__);
                timer_mutex_unlock(&time_mutex);
                break;
            }
        }

        ent = ent->next;

        timer_mutex_unlock(&time_mutex);
    }
}

static void *timer_task(void *argc)
{
    unsigned int    time_int;
    double        t1, t2;
    unsigned char    i, cnt, oldTicks, newTicks;
    struct timespec    ts, ts1, ts2;
    unsigned int    err_in_nsec;
    unsigned int    sysTicks;

    err_in_nsec = oldTicks = newTicks = sysTicks = 0;

    /* set up parameter to nanosleep() for 100 milliseconds */
    ts.tv_sec = 0;
    ts.tv_nsec = NANO_TICK_CNT;

    if (clock_gettime(CLOCK_REALTIME, &ts1) == -1) {
        xprint(FMS_FATAL | gMask, "Error in clock_gettime at %s", __FUNCTION__);
    }

    for(;;) {
        /* sleep for 100 milliseconds */
        nanosleep(&ts, NULL);

        if (clock_gettime(CLOCK_REALTIME, &ts2) == -1) {
            xprint(FMS_ERROR | gMask, "Error in clock_gettime at %s", __FUNCTION__);
        }

        t1 = ((double) ts1.tv_sec * (double) 1000000000 + (double) ts1.tv_nsec);
        t2 = ((double) ts2.tv_sec * (double) 1000000000 + (double) ts2.tv_nsec);
        if (ts2.tv_sec >= ts1.tv_sec) {
            time_int = (long) (t2-t1);
        } else {
            t1 = ((double) 1000000000 * (double) 1000000000) - t1;
            time_int = (long) (t1 + t2);
        }
        err_in_nsec += (time_int % (unsigned int)100000000);
        oldTicks = sysTicks;
        sysTicks += ((time_int/(unsigned int)100000000) + (err_in_nsec/(unsigned int)100000000));
        err_in_nsec = err_in_nsec % (unsigned int)100000000;
        newTicks = sysTicks;
        ts1.tv_nsec = ts2.tv_nsec;
        ts1.tv_sec = ts2.tv_sec;

        cnt = newTicks - oldTicks;

        /* call the common timer tick handler */
        for(i = 0; i < cnt; i++) {
            cmp_timer_value();
        }
    }

    return NULL;
}
