/* $Id: sisci_types.h,v 1.1 2002/12/13 12:17:21 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License,              *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 


#ifndef _SISCI_TYPES_H
#define _SISCI_TYPES_H

#include "sisci_error.h"

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef IN_OUT
#define IN_OUT
#endif

/* Opaque data types for descriptors/handles */
typedef struct sci_desc *sci_desc_t;
typedef struct sci_local_segment *sci_local_segment_t;
typedef struct sci_remote_segment *sci_remote_segment_t;

typedef struct sci_map *sci_map_t;
typedef struct sci_sequence *sci_sequence_t;
#ifndef KERNEL
typedef struct sci_dma_queue *sci_dma_queue_t; 
#endif
typedef struct sci_remote_interrupt *sci_remote_interrupt_t;
typedef struct sci_local_interrupt *sci_local_interrupt_t;
typedef struct sci_block_transfer *sci_block_transfer_t;

/*
 * Constants defining reasons for segment callbacks:
 */

typedef enum {
    SCI_CB_CONNECT = 1,
    SCI_CB_DISCONNECT,
    SCI_CB_NOT_OPERATIONAL,
    SCI_CB_OPERATIONAL,
    SCI_CB_LOST
} sci_segment_cb_reason_t;

#define MAX_CB_REASON SCI_CB_LOST

/* dma_queue_states is identical to the dma_queue_state_t in genif.h, they must be consistent.*/
typedef enum {
   SCI_DMAQUEUE_IDLE,
   SCI_DMAQUEUE_GATHER,
   SCI_DMAQUEUE_POSTED,
   SCI_DMAQUEUE_DONE,
   SCI_DMAQUEUE_ABORTED,
   SCI_DMAQUEUE_ERROR
} sci_dma_queue_state_t;


typedef enum {
    SCI_SEQ_OK,
    SCI_SEQ_RETRIABLE,
    SCI_SEQ_NOT_RETRIABLE,
    SCI_SEQ_PENDING
} sci_sequence_status_t;


typedef struct {
    unsigned short nodeId;      /* SCI Address bit 63 - 48 */
    unsigned short offsHi;      /* SCI Address bit 47 - 32 */
    unsigned int   offsLo;      /* SCI Address bit 31 -  0 */
} sci_address_t;


typedef unsigned int sci_ioaddr_t;

typedef enum {
    SCI_CALLBACK_CANCEL = 1,
    SCI_CALLBACK_CONTINUE
} sci_callback_action_t;

#ifndef KERNEL
typedef sci_callback_action_t (*sci_cb_local_segment_t)(void *arg,
                                                        sci_local_segment_t segment,
                                                        sci_segment_cb_reason_t reason,
                                                        unsigned int nodeId, 
                                                        unsigned int localAdapterNo,
                                                        sci_error_t error);

typedef sci_callback_action_t (*sci_cb_remote_segment_t)(void *arg,
                                                         sci_remote_segment_t segment,
                                                         sci_segment_cb_reason_t reason,
                                                         sci_error_t status);


typedef sci_callback_action_t (*sci_cb_dma_t)(void IN *arg,
                                              sci_dma_queue_t queue,
                                              sci_error_t status);


typedef int (*sci_cb_block_transfer_t)(void *arg,
                                       sci_block_transfer_t block,
                                       sci_error_t status);


typedef sci_callback_action_t (*sci_cb_interrupt_t)(void *arg,
                                                    sci_local_interrupt_t interrupt,
                                                    sci_error_t status);

#endif /* KERNEL */
#endif
