/* -------------------------------------------------------------------------
 * File Name   : host_fifo.h
 * Author      : Hyeong-Ik Jeon
 * Copyright   : Copyright (C) 2014 by KBell Inc.
 * Description : Host FIFO header
 * History     : 14/07/07    Initial Coded.
 * -------------------------------------------------------------------------*/

#ifndef __HOST_FIFO_H__
#define __HOST_FIFO_H__


#define MAX_FIFO_DEPTH    10240

#define FIFO_TYPE_ACTION  0x01
#define FIFO_TYPE_REGCTRL 0x02


typedef struct block_t block_t;
typedef struct block_fifo_t block_fifo_t;

struct block_t {
    block_t         *p_next;
    block_t         *p_prev;

    unsigned int   systemid;
    unsigned short msgtype;
    unsigned short msgsize;
    int            alist_idx;
    unsigned char  *message;
};

struct block_fifo_t {
    pthread_mutex_t lock;                         /* fifo data lock */
    pthread_cond_t  wait;         /* fifo data conditional variable */

    int             i_depth;
    block_t         *p_first;
    block_t         **pp_last;
    int             i_size;
};

extern block_t *block_New(common_tcpmsg_t *rcvmsg);
extern void block_Release(block_t *p_block);
extern block_fifo_t *block_FifoNew(void);
extern void block_FifoEmpty(block_fifo_t *p_fifo);
extern void block_FifoRelease(block_fifo_t *p_fifo);
extern int block_FifoPut(block_fifo_t *p_fifo, block_t *p_block);
extern block_t *block_FifoGet(block_fifo_t *p_fifo);
extern block_t *block_FifoShow(block_fifo_t *p_fifo);
extern int tcp_PutFifo(common_tcpmsg_t *rcvmsg, int alist_idx, unsigned char *message);

extern block_fifo_t *TcpFifo;

#endif
