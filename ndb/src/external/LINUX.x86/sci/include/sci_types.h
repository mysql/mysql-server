/* $Id: sci_types.h,v 1.1 2002/12/13 12:17:21 hin Exp $  */

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


#ifndef _SCI_TYPES_H_
#define _SCI_TYPES_H_

/*
 *   Remains for the time being for backward compatibility ....
 */

/* #define UNIQUE(type) struct { type x; } *  */
#ifndef UNIQUE
#define UNIQUE(type) type
#endif
#include "os/inttypes.h"

#if defined(WIN32) 
#if defined(_KERNEL)
#include <ntddk.h>
#else
#include <WTYPES.H>
#endif /* _KERNEL */
#else
#if defined(Linux)
#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <sys/types.h>
#endif
#else
#include <sys/types.h>
#endif
#ifdef SUNOS5
#include <sys/ddi.h>
#include <sys/sunddi.h>
#endif
#ifdef OS_IS_TRU64
#include <io/common/devdriver.h>
#endif
#ifdef OS_IS_HP_UX11
#if defined(_KERNEL)
#include <../wsio/wsio.h>
#else
#include <sys/wsio.h>
#endif
#endif
#endif

/* See comments about "UNCONFIGURED_ADAPTERS" in config.h  */
#define UNCONFIGURED_ADAPTERS 100 

#ifndef FALSE               
#define FALSE 0
#endif

#ifndef TRUE           
#define TRUE  1
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef IN
#define IN
#endif

#ifndef NOT
#define NOT !
#endif

/*
 * --------------------------------------------------------------------------------------
 * Basic types of various sizes.
 * --------------------------------------------------------------------------------------
 */

typedef signed32            scibool;
#ifndef OS_IS_VXWORKS
typedef signed32            BOOL;
#else
/* VXWORKS has already defined BOOL */
#endif
typedef unsigned32          node_t;       /* This is the logical nodeid */
typedef unsigned32          sciNodeId_t;  /* This is the physical 16 bit SCI nodeid */

/*
 * --------------------------------------------------------------------------------------
 * Various register types.
 * --------------------------------------------------------------------------------------
 */
typedef volatile unsigned32 register32;


/*
Temporary for Windows NT, until we use only the above types.
*/

#ifdef WIN32

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef char * caddr_t;

typedef long off_t;
typedef unsigned int size_t;

#endif
#ifdef OS_IS_VXWORKS
#include <vxWorks.h>
#endif

/*
 * --------------------------------------------------------------------------------------
 * Various address types.
 *
 * We are using a struct * instead of unsigned long (int) inorder to enforce strong
 * type checking
 *
 * --------------------------------------------------------------------------------------
 */
typedef UNIQUE(void *) vkaddr_t;        /* Virtual kernel address          */
typedef UNIQUE(uptr_t) vuaddr_t;        /* Virtual user address            */

typedef UNIQUE(unsigned32) remaddr_t;   /* Remote IO address (physical address on PCs) */
typedef UNIQUE(unsigned32) sciofs_lo_t; /* Lower 32 bits of an SCI offset. */
typedef UNIQUE(unsigned32) sciofs_hi_t; /* The upper 16 bits of an SCI offset. */

typedef UNIQUE(unsigned32) ioaddr_t;    /* Local  IO address (physical address on PCs) */
typedef unsigned32 u_ioaddr_t;          
typedef unsigned32 iooffset_t;          
typedef unsigned32 iosize_t;          

typedef uptr_t     vkoffset_t;          
typedef uptr_t     u_vkaddr_t;
typedef uptr_t     u_vuaddr_t;
typedef unsigned32 u_sciofs_lo_t;       
typedef unsigned32 u_sciofs_hi_t;       
typedef unsigned32 u_remaddr_t;
typedef unsigned32 attOffset_t;         /* Address displacement from start of ATT entry */

typedef unsigned32 adapterNo_t;

typedef enum {
    NO_NODE      = 0,
    AD_MEM_NODE  = 1,
    AD_ALT_NODE  = 2,
    AD_MBX_NODE  = 3,
    AD_LC_NODE   = 4,
    AD_LC_PORT_0 = 5,
    AD_LC_PORT_1 = 6,
    AD_LC_PORT_2 = 7,
    PHYS_NODE    = 8
} node_type_t;


/*
 * Currently we don't support more than 32 bit sizes.
 */
#define SIZEOF(x) ((unsigned32)sizeof(x))

#if defined(_KERNEL)

/*
 * --------------------------------------------------------------------------------------
 * Some small macros intended to ease the transition to more strongly typed address
 * types. The intention is that they in the long run shall be removed ...
 * --------------------------------------------------------------------------------------
 */
#define P2SIZE_T(x) ((size_t)((uptr_t)(x)))          /* Pointer to size_t */
#define P2U32(x) ((unsigned32)((uptr_t)(x)))         /* Pointer to Unsigned 32-bit int */
#ifdef WIN32
#define PHADDR(x)  ((ioaddr_t)(x))
#define HASV(x) (x)
#endif
#if 0
static vkaddr_t VKPTR (void * ptr) { return (vkaddr_t)ptr; }
static vkaddr_t VKADDR(volatile void * ptr) { return (vkaddr_t)ptr; }
#else
#define VKPTR(ptr)  (vkaddr_t)ptr
#define VKADDR(ptr) (vkaddr_t)ptr
#endif

#ifdef KLOG
#define KLOG_LOG(n,m,v) ts_log((n),(m),(v))
#else
#define KLOG_LOG(n,m,v)
#endif /* KLOG */


/*
 * --------------------------------------------------------------------------------------
 *
 * M E M   A R E A   T
 *
 * Memory area descriptor.
 *
 * paddr     --  Physical address (aligned) of memory area 
 * ual_vaddr --  (Kernel )Virtual address of the unaligned memory area
 * vaddr     --  (Kernel) Virtual address of memory area
 * rsize     --  Real (Physical) Size of memory area
 * msize     --  Mapped (Virtual) Size of memory area (Size of area mapped 
 *               into virtual) memory
 *
 *      --------------------------------------
 *      |       | <----- msize ----->|       |
 *      |<------|------- rsize ------|------>|
 *      --------------------------------------
 *     /|\     /|\
 *      |       |
 *  ual_vaddr  vaddr/paddr
 *
 * --------------------------------------------------------------------------------------
 */
struct _memarea_ {
	ioaddr_t   ioaddr;            
	vkaddr_t   vaddr;
    vkaddr_t   ual_vaddr;
	size_t     rsize;             
	size_t     msize;
	char      *id;
	unsigned32 cookie;


#ifdef SUNOS5
#ifdef _USE_NEW_SOLARIS_DDI_INTERFACE
        ddi_acc_handle_t mem_handle;
	ddi_dma_handle_t dma_handle;
#else
	ddi_dma_handle_t handle;
#endif
#endif
#ifdef OS_IS_TRU64
	dma_handle_t dma_handle;
#endif

#if OS_IS_LINUX
    unsigned long ph_base_addr;
#endif

#ifdef OS_IS_HP_UX11
	struct isc_table_type * isc;
	wsio_shmem_attr_t type;
#endif
};

typedef struct _memarea_ memarea_t;

#ifdef SCI_MALLOC_DEBUG
struct _maddr_ {
	char		 *id;
	size_t		  size;
	struct _maddr_	 *next;
	struct _maddr_	**prev;
	unsigned32	  cookie;
};

typedef struct _maddr_ maddr_t;

#define MALLOC_COOKIE 0xc3c3c3c3

#else

typedef struct { void *p; } *maddr_t;

#endif /* SCI_MALLOC_DEBUG */


typedef struct {
    scibool disabled;
    unsigned32 disable_cnt;
} disable_info_t;

#endif /* _KERNEL */

#endif /* _SCI_TYPES_H_ */
