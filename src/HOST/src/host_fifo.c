/* -------------------------------------------------------------------------
 * File Name   : host_fifo.c
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2013 by KBell Inc.
 * Description : Host FIFO Fn. 
 * History     : 14/07/07    Initial Coded.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>

#include "bpapp.h"

#include "xprint.h"
#include "kutil.h"

#include "host_main.h"
#include "host_fifo.h"
#if 0
#include "host_msg.h"
#endif


block_fifo_t *TcpFifo;


/* int PutFifo(common_tcpmsg_t *rcvmsg) */
block_t *block_New(common_tcpmsg_t *rcvmsg)
{
    block_t *p_block = (block_t *)malloc(sizeof(block_t));

    if (p_block == NULL) {
        xprint(FMS_HST | FMS_ERROR, "Error p_block not allocated(%s : %d)\n", __func__, __LINE__);
        return(NULL);
    }

    /* Fill all fields */
    p_block->p_next = NULL;
    p_block->p_prev = NULL;

    p_block->systemid = rcvmsg->SystemID;
    p_block->msgtype  = rcvmsg->MsgType;
    p_block->msgsize  = rcvmsg->MsgSize;
#if 0 /* block by icecom 20111028 */
    p_block->msgcnt   = rcvmsg->MsgCnt;
    p_block->lineid   = rcvmsg->LineId;
#endif

    p_block->message = (unsigned char *)malloc(rcvmsg->MsgSize * sizeof(unsigned char));

    if (p_block->message == NULL) {
        xprint(FMS_HST | FMS_ERROR, "Error message not allocated(%s : %d)\n", __func__, __LINE__);
        free(p_block);
        return(NULL);
    }

    return(p_block);
}

/* int GetFifo(void) -> Release() */
void block_Release(block_t *p_block)
{
    if (p_block) {
        free(p_block->message);
        free(p_block);
    }
}

/* void InitFifo(void) */
block_fifo_t *block_FifoNew(void)
{
    block_fifo_t *p_fifo;

    p_fifo = (block_fifo_t *)malloc(sizeof(block_fifo_t));

    if (p_fifo == NULL) return(NULL);

    pthread_mutex_init(&p_fifo->lock, NULL);
    pthread_cond_init(&p_fifo->wait, NULL);

    xprint(FMS_HST | FMS_LOOKS, "p_fifo->lock Mutex  = %p / %p\n", p_fifo->lock, &p_fifo->lock);
    xprint(FMS_HST | FMS_LOOKS, "p_fifo->wait Cond   = %p / %p\n", p_fifo->wait, &p_fifo->wait);

    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;

    return(p_fifo);
}

/* thread cancel */
void block_FifoEmpty(block_fifo_t *p_fifo)
{
    block_t *b;

    if (&p_fifo->lock) {
        pthread_mutex_unlock(&p_fifo->lock);
    }

    pthread_mutex_lock(&p_fifo->lock);
    for(b = p_fifo->p_first; b != NULL; ) {
        block_t *p_next;

        p_next = b->p_next;
        block_Release(b);
        b = p_next;
    }

    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    pthread_mutex_unlock(&p_fifo->lock);
}

/* thread cancel */
void block_FifoRelease(block_fifo_t *p_fifo)
{
    if (p_fifo == NULL) return;

    pthread_cond_destroy(&p_fifo->wait);
    pthread_mutex_destroy(&p_fifo->lock);
    free(p_fifo);
    p_fifo = NULL;
}

/* int PutFifo(common_tcpmsg_t *rcvmsg) */
int block_FifoPut(block_fifo_t *p_fifo, block_t *p_block)
{
    int i_size = 0;

    pthread_mutex_lock(&p_fifo->lock);

    do {
        i_size += p_block->msgsize;
        
        *p_fifo->pp_last = p_block;
        p_fifo->pp_last = &p_block->p_next;
        p_fifo->i_depth++;
        p_fifo->i_size += p_block->msgsize;

        p_block = p_block->p_next;

    } while(p_block);

    if (p_fifo->i_depth >= MAX_FIFO_DEPTH) {
        xprint(FMS_HST | FMS_WARNING, "fifo i_depth(%d) more than %d\n", p_fifo->i_depth, MAX_FIFO_DEPTH);
    }

    pthread_cond_signal(&p_fifo->wait);
    pthread_mutex_unlock(&p_fifo->lock);

    return(i_size);
}

/* int GetFifo(void) */
block_t *block_FifoGet(block_fifo_t *p_fifo)
{
    block_t *b;
    struct timespec ts;

    pthread_mutex_lock(&p_fifo->lock);

    /* We do a while here because there is a race condition in the
     * win32 implementation of vlc_cond_wait() (We can't be sure the fifo
     * hasn't been emptied again since we were signaled). */
#if 0
    while(p_fifo->p_first == NULL) {
        pthread_cond_wait(&p_fifo->wait, &p_fifo->lock);
    }
#else
    if (p_fifo->p_first == NULL) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec  = ts.tv_sec;
        ts.tv_nsec = ts.tv_nsec + 900000;

        if (pthread_cond_timedwait(&p_fifo->wait, &p_fifo->lock, &ts) != 0) {
            pthread_mutex_unlock(&p_fifo->lock);
            return(NULL);
        }
    }
#endif
    b = p_fifo->p_first;

    p_fifo->p_first = b->p_next;
    p_fifo->i_depth--;
    p_fifo->i_size -= b->msgsize;

    if (p_fifo->p_first == NULL) {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    pthread_mutex_unlock(&p_fifo->lock);

    b->p_next = NULL;

    return(b);
}

block_t *block_FifoShow(block_fifo_t *p_fifo)
{
    block_t *b;

    pthread_mutex_lock(&p_fifo->lock);

    if (p_fifo->p_first == NULL) {
        pthread_cond_wait(&p_fifo->wait, &p_fifo->lock);
    }

    b = p_fifo->p_first;

    pthread_mutex_unlock(&p_fifo->lock);

    return(b);
}

int tcp_PutFifo(common_tcpmsg_t *rcvmsg, int alist_idx, unsigned char *message)
{
    block_t *p_block;

    p_block = block_New(rcvmsg);
    if (p_block == NULL) {
        xprint(FMS_HST | FMS_ERROR, "not allocate memory at block_New\n");
        return(-1);
    }

#if 0 /* NEXT_CHECK by icecom 20111028 */
    memcpy(p_block->message, (unsigned char *)&message[0], rcvmsg->MsgSize);
#else
    memcpy(p_block->message, message, rcvmsg->MsgSize);
#endif
    p_block->alist_idx  = alist_idx;

    if (block_FifoPut(TcpFifo, p_block) < 0) {
        return(-1);
    }

    return(0);
}
