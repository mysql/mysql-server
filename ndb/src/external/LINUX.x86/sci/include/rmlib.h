/* $Id: rmlib.h,v 1.1 2002/12/13 12:17:20 hin Exp $  */

/*********************************************************************************
 *                                                                               *
 * Copyright (C) 1993 - 2000                                                     * 
 *         Dolphin Interconnect Solutions AS                                     *
 *                                                                               *
 * This program is free software; you can redistribute it and/or modify          * 
 * it under the terms of the GNU Lesser General Public License as published by   *
 * the Free Software Foundation; either version 2.1 of the License,              *
 * or (at your option) any later version.                                        *
 *                                                                               *
 * This program is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 * 
 * GNU General Public License for more details.                                  *
 *                                                                               *
 * You should have received a copy of the GNU Lesser General Public License      *
 * along with this program; if not, write to the Free Software                   *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/ 

/********************************************************************************/
/*  This header file contains the declarations of the SCI Reflective Memory     */
/*  library rmlib. The implementation of the library functions is in rmlib.c.   */
/*  The library contains all the functions that operate on the reflective       */
/*  memory.                                                                     */
/*                                                                              */
/*  NB!                                                                         */  
/*                                                                              */
/*  DOLPHIN'S SCI REFLECTIVE MEMORY FILES ARE UNDER DEVELOPMENT AND MAY CHANGE. */
/*  PLEASE CONTACT DOLPHIN FOR FURTHER INFORMATION.                             */
/*                                                                              */
/*                                                                              */
/********************************************************************************/

#include "sisci_error.h"
#include "sisci_api.h"
#include "sisci_demolib.h"
#include "sisci_types.h"

unsigned int seqerr, syncseqerr;

#ifndef _RMLIB_H
#define _RMLIB_H


#if defined(_REENTRANT)

#define _RMLIB_EXPAND_NAME(name)  _RMLIB_MT_ ## name

#else

#define _RMLIB_EXPAND_NAME(name)  _RMLIB_ST_ ## name

#endif

#ifdef __sparc
#define CACHE_SIZE 2097152
#else
#define CACHE_SIZE 8192
#endif

/*********************************************************************************/
/*                         FLAG VALUES                                           */
/*********************************************************************************/

#define REFLECT_ERRCHECK          0x2

struct ReflectiveMemorySpace {    
    unsigned int        localAdapterNo;
    unsigned int        localNodeId;
    unsigned int        remoteNodeId;
    sci_desc_t          sd;
    sci_desc_t          syncsd;
    sci_map_t           localMap;
    sci_map_t           remoteMap;
    unsigned int        localSegmentId;
    unsigned int        remoteSegmentId;
    unsigned int        syncSegmentId;
    unsigned int        sync_rSegmentId;
    unsigned int        segmentSize;
    unsigned int        *localMapAddr;
    volatile unsigned int *remoteMapAddr;
    sci_local_segment_t  localSegment;
    sci_remote_segment_t remoteSegment;
    sci_local_segment_t  syncSegment;
    sci_remote_segment_t sync_rSegment;
    sci_map_t            syncMap;
    sci_map_t            sync_rMap;
    sci_sequence_t       syncsequence;
    sci_sequence_t       sequence;    
    unsigned int         protection;
    unsigned int         retry_value;
    sci_sequence_status_t   sequenceStatus, syncsequenceStatus;
    volatile unsigned int *syncMapAddr;
    volatile unsigned int *sync_rMapAddr;    
};

/*********************************************************************************/
/*          P R I N T  R E F L E C T I V E   M E M O R Y   S P A C E             */
/*                                                                               */
/*********************************************************************************/
#define ReflectPrintParameters             _RMLIB_EXPAND_NAME(ReflectPrintParameters)
void ReflectPrintParameters(FILE *stream, struct ReflectiveMemorySpace RM_space);

/*********************************************************************************/
/*      			  R E F L E C T   D M A   S E T U P                          */
/*                                                                               */
/*********************************************************************************/
#define ReflectDmaSetup             _RMLIB_EXPAND_NAME(ReflectDmaSetup)
sci_error_t ReflectDmaSetup(struct ReflectiveMemorySpace RM_space, sci_dma_queue_t *dmaQueue);

/*********************************************************************************/
/*      			  R E F L E C T   D M A   R E M O V E                        */
/*                                                                               */
/*********************************************************************************/
#define ReflectDmaRemove             _RMLIB_EXPAND_NAME(ReflectDmaRemove)
sci_error_t ReflectDmaRemove(sci_dma_queue_t dmaQueue);

/*********************************************************************************/
/*   	    			R E F L E C T   D M A   R U N                            */
/*                                                                               */
/*********************************************************************************/
#define ReflectDmaRun             _RMLIB_EXPAND_NAME(ReflectDmaRun)
sci_error_t ReflectDmaRun(struct ReflectiveMemorySpace RM_space, 
                           unsigned int* privateSrc, 
                           unsigned int size, 
                           unsigned int offset, 
                           sci_dma_queue_t dmaQueue);
/*********************************************************************************/
/*          C L O S E   R E F L E C T I V E   M E M O R Y   S P A C E            */
/*                                                                               */
/*********************************************************************************/
#define ReflectClose             _RMLIB_EXPAND_NAME(ReflectClose)
sci_error_t ReflectClose(struct ReflectiveMemorySpace RM_space, unsigned int segment_no);

/*********************************************************************************/
/*           O P E N  R E F L E C T I V E   M E M O R Y   S P A C E              */
/*                                                                               */
/*********************************************************************************/
#define ReflectOpen             _RMLIB_EXPAND_NAME(ReflectOpen)
sci_error_t ReflectOpen(struct ReflectiveMemorySpace *RM_space, 
                       unsigned int size, 
                       unsigned int segment_no,
                       unsigned int localAdapterNo, 
                       unsigned int remoteNodeId, 
                       unsigned int protection, 
                       unsigned int retry_value);

/*********************************************************************************/
/* 			  	    R E F L E C T   G E T   A C C E S S                          */
/*                                                                               */
/*********************************************************************************/
#define ReflectGetAccess             _RMLIB_EXPAND_NAME(ReflectGetAccess)
sci_error_t ReflectGetAccess(struct ReflectiveMemorySpace *RM_space);

/*********************************************************************************/
/* 			  	 R E F L E C T   R E L E A S E   A C C E S S                     */
/*                                                                               */
/*********************************************************************************/
#define ReflectReleaseAccess             _RMLIB_EXPAND_NAME(ReflectReleaseAccess)
sci_error_t ReflectReleaseAccess(struct ReflectiveMemorySpace *RM_space);

/*********************************************************************************/
/*   	    			  	R E F L E C T   D M A                                */
/*                                                                               */
/*********************************************************************************/
#define ReflectDma             _RMLIB_EXPAND_NAME(ReflectDma)
sci_error_t ReflectDma(struct ReflectiveMemorySpace RM_space,
                           unsigned int* privateSrc, 
                           unsigned int size, 
                           unsigned int offset);

/*********************************************************************************/
/*   	    	         R E F L E C T   M E M C O P Y                           */
/*                                                                               */
/*********************************************************************************/
#define ReflectMemCopy             _RMLIB_EXPAND_NAME(ReflectMemCopy)
sci_error_t ReflectMemCopy(struct ReflectiveMemorySpace RM_space,
                           unsigned int* privateSrc, 
                           unsigned int size, 
                           unsigned int offset,
                           unsigned int flags);

/*********************************************************************************/
/*   	    			  	R E F L E C T   S E T                                */
/*                                                                               */
/*********************************************************************************/
#define ReflectSet             _RMLIB_EXPAND_NAME(ReflectSet)
sci_error_t ReflectSet(struct ReflectiveMemorySpace RM_space,
                           unsigned int value,
                           unsigned int size,                            
                           unsigned int offset,
                           unsigned int flags
                           );

/*********************************************************************************/
/*   	    			R E F L E C T   P R I N T                                */
/*                                                                               */
/*********************************************************************************/
#define ReflectPrint             _RMLIB_EXPAND_NAME(ReflectPrint)
sci_error_t ReflectPrint(FILE *stream, 
                          struct ReflectiveMemorySpace RM_space,
                          unsigned int size, 
                          unsigned int offset
                          );


#endif
