/* $Id: sisci_api.h,v 1.1 2002/12/13 12:17:22 hin Exp $  */
/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2001                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU Lesser General Public License as published by *
 * the Free Software Foundation; either version 2.1 of the License,            *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 

#ifndef _SISCI_API_H
#define _SISCI_API_H

#include "sisci_types.h"
#include "sisci_error.h"

 
#ifdef WIN32
#ifdef API_DLL
#define DLL __declspec(dllexport)
#elif CLIENT_DLL
#define DLL __declspec(dllimport) 
#endif
#endif /* WIN32 */


#ifndef DLL
#define DLL 
#endif

#if defined(_REENTRANT)
#define _SISCI_EXPANDE_FUNCTION_NAME(name)  _SISCI_PUBLIC_FUNC_MT_ ## name
#define _SISCI_EXPANDE_VARIABLE_NAME(name)  _SISCI_PUBLIC_VAR_MT_ ## name
#else
#define _SISCI_EXPANDE_FUNCTION_NAME(name)  _SISCI_PUBLIC_FUNC_ST_ ## name
#define _SISCI_EXPANDE_VARIABLE_NAME(name)  _SISCI_PUBLIC_VAR_ST_ ## name
#endif
#define _SISCI_EXPANDE_CONSTANT_NAME(name)  _SISCI_PUBLIC_CONST_ ## name

#if defined(CPLUSPLUS) || defined(__cplusplus)
extern "C" {
#endif


/*********************************************************************************/
/*                         FLAG VALUES                                           */
/*********************************************************************************/

#define SCI_FLAG_FIXED_INTNO                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FIXED_INTNO)
extern const unsigned int SCI_FLAG_FIXED_INTNO;

#define SCI_FLAG_SHARED_INT                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_SHARED_INT)
extern const unsigned int SCI_FLAG_SHARED_INT;

#define SCI_FLAG_FIXED_MAP_ADDR                            _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FIXED_MAP_ADDR)
extern const unsigned int SCI_FLAG_FIXED_MAP_ADDR;

#define SCI_FLAG_READONLY_MAP                              _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_READONLY_MAP)
extern const unsigned int SCI_FLAG_READONLY_MAP; 

#define SCI_FLAG_USE_CALLBACK                              _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_USE_CALLBACK)
extern const unsigned int SCI_FLAG_USE_CALLBACK; 

#define SCI_FLAG_BLOCK_READ                                _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_BLOCK_READ)
extern const unsigned int SCI_FLAG_BLOCK_READ;

#define SCI_FLAG_THREAD_SAFE                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_THREAD_SAFE)
extern const unsigned int SCI_FLAG_THREAD_SAFE;

#define SCI_FLAG_ASYNCHRONOUS_CONNECT                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_ASYNCHRONOUS_CONNECT)
extern const unsigned int SCI_FLAG_ASYNCHRONOUS_CONNECT;

#define SCI_FLAG_EMPTY                                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_EMPTY)
extern const unsigned int SCI_FLAG_EMPTY;

#define SCI_FLAG_PRIVATE                                   _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_PRIVATE)
extern const unsigned int SCI_FLAG_PRIVATE;

#define SCI_FLAG_FORCE_DISCONNECT                          _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FORCE_DISCONNECT)
extern const unsigned int SCI_FLAG_FORCE_DISCONNECT;

#define SCI_FLAG_NOTIFY                                    _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_NOTIFY)
extern const unsigned int SCI_FLAG_NOTIFY;

#define SCI_FLAG_DMA_READ                                  _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_READ)
extern const unsigned int SCI_FLAG_DMA_READ;

#define SCI_FLAG_DMA_POST                                  _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_POST)
extern const unsigned int SCI_FLAG_DMA_POST;

#define SCI_FLAG_DMA_WAIT                                  _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_WAIT)
extern const unsigned int SCI_FLAG_DMA_WAIT;

#define SCI_FLAG_DMA_RESET                                 _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_RESET)
extern const unsigned int SCI_FLAG_DMA_RESET;

#define SCI_FLAG_NO_FLUSH                                  _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_NO_FLUSH)
extern const unsigned int SCI_FLAG_NO_FLUSH;

#define SCI_FLAG_NO_STORE_BARRIER                          _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_NO_STORE_BARRIER)
extern const unsigned int SCI_FLAG_NO_STORE_BARRIER;

#define SCI_FLAG_FAST_BARRIER                              _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FAST_BARRIER)
extern const unsigned int SCI_FLAG_FAST_BARRIER;

#define SCI_FLAG_ERROR_CHECK                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_ERROR_CHECK)
extern const unsigned int SCI_FLAG_ERROR_CHECK;

#define SCI_FLAG_FLUSH_CPU_BUFFERS_ONLY                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FLUSH_CPU_BUFFERS_ONLY)
extern const unsigned int SCI_FLAG_FLUSH_CPU_BUFFERS_ONLY;

/* the FLUSH_CPU_BUFFERS_ONLY flag is for backwards compabillity only and should never be used */
#define FLUSH_CPU_BUFFERS_ONLY                             _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_FLUSH_CPU_BUFFERS_ONLY) 

#define SCI_FLAG_LOCK_OPERATION                            _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_LOCK_OPERATION)
extern const unsigned int SCI_FLAG_LOCK_OPERATION;

#define SCI_FLAG_READ_PREFETCH_AGGR_HOLD_MAP                    _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_READ_PREFETCH_AGGR_HOLD_MAP)
extern const unsigned int SCI_FLAG_READ_PREFETCH_AGGR_HOLD_MAP;

#define SCI_FLAG_READ_PREFETCH_NO_HOLD_MAP                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_READ_PREFETCH_NO_HOLD_MAP)
extern const unsigned int SCI_FLAG_READ_PREFETCH_NO_HOLD_MAP;

#define SCI_FLAG_IO_MAP_IOSPACE                                 _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_IO_MAP_IOSPACE)
extern const unsigned int SCI_FLAG_IO_MAP_IOSPACE;

#define SCI_FLAG_DMOVE_MAP                                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMOVE_MAP)
extern const unsigned int SCI_FLAG_DMOVE_MAP;

#define SCI_FLAG_WRITES_DISABLE_GATHER_MAP                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_WRITES_DISABLE_GATHER_MAP)
extern const unsigned int SCI_FLAG_WRITES_DISABLE_GATHER_MAP;

#define SCI_FLAG_DISABLE_128_BYTES_PACKETS                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DISABLE_128_BYTES_PACKETS)
extern const unsigned int SCI_FLAG_DISABLE_128_BYTES_PACKETS;

#define SCI_FLAG_DMA_SOURCE_ONLY                                _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_SOURCE_ONLY)
extern const unsigned int SCI_FLAG_DMA_SOURCE_ONLY;

#define SCI_FLAG_CONDITIONAL_INTERRUPT                          _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_CONDITIONAL_INTERRUPT)
extern const unsigned int SCI_FLAG_CONDITIONAL_INTERRUPT;

#define SCI_FLAG_CONDITIONAL_INTERRUPT_MAP                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_CONDITIONAL_INTERRUPT_MAP)
extern const unsigned int SCI_FLAG_CONDITIONAL_INTERRUPT_MAP;

#define SCI_FLAG_UNCONDITIONAL_DATA_INTERRUPT_MAP               _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_UNCONDITIONAL_DATA_INTERRUPT_MAP)
extern const unsigned int SCI_FLAG_UNCONDITIONAL_DATA_INTERRUPT_MAP;

#define SCI_FLAG_NO_MEMORY_LOOPBACK_MAP                         _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_NO_MEMORY_LOOPBACK_MAP)
extern const unsigned int SCI_FLAG_NO_MEMORY_LOOPBACK_MAP;

#if defined(OS_IS_LYNXOS) || defined(OS_IS_VXWORKS)
#define SCI_FLAG_WRITE_BACK_CACHE_MAP                           _SISCI_EXPANDE_CONSTANT_NAME(WRITE_BACK_CACHE_MAP)
extern const unsigned int SCI_FLAG_WRITE_BACK_CACHE_MAP;
#endif

#define SCI_FLAG_DMA_PHDMA                                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_FLAG_DMA_PHDMA)
extern const unsigned int SCI_FLAG_DMA_PHDMA;

/*********************************************************************************/
/*                         GENERAL VALUES                                        */
/*********************************************************************************/
#define SCI_LOCAL_HOST                                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_LOCAL_HOST)
extern const unsigned int SCI_LOCAL_HOST;        

#define SCI_INFINITE_TIMEOUT                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_INFINITE_TIMEOUT)
extern const unsigned int SCI_INFINITE_TIMEOUT;    

/*********************************************************************************/
/*                       GENERAL ERROR CODES                                     */
/*                                                                               */
/*  SCI_ERR_ILLEGAL_FLAG          - Illegal flag value.                          */ 
/*  SCI_ERR_FLAG_NOT_IMPLEMENTED  - Flag legal but flag feature not implemented. */
/*  SCI_ERR_NOT_IMPLEMENTED       - Function not implemented.                    */
/*  SCI_ERR_SYSTEM                - A system error. Check errno.                 */
/*  SCI_ERR_NOSPC                 - Unable to allocate OS resources.             */
/*  SCI_ERR_API_NOSPC             - Unable to allocate API resources.            */
/*  SCI_ERR_HW_NOSPC              - Unable to allocate HW resources (Hardware)   */
/*                                                                               */
/*********************************************************************************/


/*********************************************************************************/
/*                    GENERAL "ADAPTER" ERROR CODES                              */
/*                                                                               */
/*  SCI_ERR_NO_SUCH_ADAPTERNO     - Adapter number is legal but does not exist.  */
/*  SCI_ERR_ILLEGAL_ADAPTERNO     - Illegal local adapter number  (i.e. outside  */
/*                                  legal range).                                */
/*                                                                               */
/*********************************************************************************/


/*********************************************************************************/
/*                     GENERAL "NODEID" ERROR CODES                              */
/*                                                                               */
/*  SCI_ERR_NO_SUCH_NODEID        - The remote adapter identified by nodeId does */
/*                                  not respond, but the intermediate link(s)    */
/*                                  seem(s) to be operational.                   */
/*  SCI_ERR_ILLEGAL_NODEID        - Illegal NodeId.                              */
/*                                                                               */
/*********************************************************************************/



/*********************************************************************************
 *                                                                               *
 * S C I   I N I T I A L I Z E                                                   *
 *                                                                               *
 * This function initializes the SISCI library.                                  *
 * The function must be called before SCIOpen().                                 *
 *                                                                               *
 *  Flags:                                                                       *
 *    None                                                                       *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *    None                                                                       *
 *                                                                               *
 *********************************************************************************/
#define SCIInitialize                            _SISCI_EXPANDE_FUNCTION_NAME(SCIInitialize)
DLL void SCIInitialize(unsigned int flags,
                       sci_error_t *error);
#if 0
unsigned int __Internal_SISCI_version_var;
#endif

/*********************************************************************************
 *                                                                               *
 * S C I   T E R M I N A T E                                                     *
 *                                                                               *
 * This function terminates the SISCI library.                                   *
 * The function must be called after SCIClose().                                 *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCITerminate                             _SISCI_EXPANDE_FUNCTION_NAME(SCITerminate)
DLL void SCITerminate(void);

/*********************************************************************************
 *                                                                               *
 * S C I   O P E N                                                               *
 *                                                                               *
 *                                                                               *
 * Opens a SCI virtual device.                                                   *
 * Caller must supply a pointer to a variable of type sci_desc_t to be           *
 * initialized.                                                                  *
 *                                                                               *
 *  Flags                                                                        *
 *  SCI_FLAG_THREAD_SAFE  - Operations on resources associated with this         *
 *                          descriptor will be performed in a multithread-safe   *
 *                          manner.                                              *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_INCONSISTENT_VERSIONS    - Inconsistency between the SISCI library   *
 *                                     and the SISCI driver versions.            *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIOpen                                  _SISCI_EXPANDE_FUNCTION_NAME(SCIOpen)
DLL void SCIOpen(sci_desc_t   *sd,
                 unsigned int flags,
                 sci_error_t  *error);




/*********************************************************************************
 *                                                                               *
 * S C I  C L O S E                                                              *
 *                                                                               *
 * This function closes an open SCI virtual device.                              *
 *                                                                               *
 *  Flags:                                                                       *
 *    None                                                                       *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_BUSY 		             - All resources are not deallocated.        *
 *                                                                               *
 *********************************************************************************/
#define SCIClose                                 _SISCI_EXPANDE_FUNCTION_NAME(SCIClose)
DLL void SCIClose(sci_desc_t sd,
                  unsigned int flags,
                  sci_error_t *error);




/*********************************************************************************
 *                                                                               *
 * S C I   C O N N E C T   S  E G M E N T                                        *
 *                                                                               *
 * Connects to a remote shared memory segment located at <nodeId> with the       *
 * identifier <segmentId>.                                                       *
 * The user may then call SCIMapRemoteSegment() to map shared memory             *
 * into user space.                                                              *
 *                                                                               *
 *  Flags                                                                        *
 *  SCI_FLAG_USE_CALLBACK                                                        *
 *  SCI_FLAG_ASYNCHRONOUS_CONNECT                                                *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_NO_SUCH_SEGMENT       - Could not find the remote segment with the   *
 *                                  given segmentId.                             *
 *  SCI_ERR_CONNECTION_REFUSED    - Connection attempt refused by remote node.   *
 *  SCI_ERR_TIMEOUT               - The function timed out after specified       *
 *                                  timeout value.                               *
 *  SCI_ERR_NO_LINK_ACCESS        - It was not possible to communicate via the   *
 *                                  local adapter.                               *
 *  SCI_ERR_NO_REMOTE_LINK_ACCESS - It was not possible to communicate via a     *
 *                                  remote switch port.                          *
 *                                                                               *
 *********************************************************************************/
#define SCIConnectSegment                        _SISCI_EXPANDE_FUNCTION_NAME(SCIConnectSegment)
DLL void SCIConnectSegment(sci_desc_t              sd,
                           sci_remote_segment_t    *segment,
                           unsigned int            nodeId,
                           unsigned int            segmentId,
                           unsigned int            localAdapterNo,
                           sci_cb_remote_segment_t callback, 
                           void                    *callbackArg,
                           unsigned int            timeout,
                           unsigned int            flags,
                           sci_error_t             *error);


/*********************************************************************************
 *                                                                               *
 * S C I   D I S C O N N E C T   S E G M E N T                                   *
 *                                                                               *
 * Disconnects from the give mapped shared memory segment                        *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_BUSY		          - The segment is currently mapped or in use.   *
 *                                                                               *
 *********************************************************************************/
#define SCIDisconnectSegment                     _SISCI_EXPANDE_FUNCTION_NAME(SCIDisconnectSegment)
DLL void SCIDisconnectSegment(sci_remote_segment_t segment,
                              unsigned int         flags,
                              sci_error_t          *error);



/*********************************************************************************
 *                                                                               *
 * S C I   G E T  R E M O T E  S E G M E N T  S I Z E                            *
 *                                                                               *
 *********************************************************************************/
#define SCIGetRemoteSegmentSize                  _SISCI_EXPANDE_FUNCTION_NAME(SCIGetRemoteSegmentSize)
DLL unsigned int SCIGetRemoteSegmentSize(sci_remote_segment_t segment);



/*********************************************************************************
 *                                                                               *
 * S C I  W A I T   F O R   R E M O T E  S E G M E N T  E V E N T                *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_TIMEOUT               - The function timed out after specified       *
 *                                  timeout value.                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation.                           *
 *  SCI_ERR_CANCELLED             - The wait operation has been cancelled du     *
 *                                  to a SCIDisconnectSegment() on the same      *
 *                                  handle. The handle is invalid when this      *
 *                                  error is returned.                           *
 *                                                                               *
 *********************************************************************************/
#define SCIWaitForRemoteSegmentEvent             _SISCI_EXPANDE_FUNCTION_NAME(SCIWaitForRemoteSegmentEvent)
DLL sci_segment_cb_reason_t SCIWaitForRemoteSegmentEvent(
                                            sci_remote_segment_t segment,
                                            sci_error_t          *status,
                                            unsigned int         timeout,
                                            unsigned int         flags,
                                            sci_error_t          *error);




/*********************************************************************************
 *                                                                               *
 * S C I   M A P   R E M O T E   S E G M E N T                                   *
 *                                                                               *
 * This function is used to include a shared memory segment in the virtual       *
 * address space of the application.                                             *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_FIXED_MAP_ADDR       - Map at the suggested virtual address         *
 *  SCI_FLAG_READONLY_MAP         - The segment is mapped in read-only mode      *
 *  SCI_FLAG_LOCK_OPERATION       - Enable Lock operations (fetch and add)       *
 *  SCI_FLAG_READ_PREFETCH_AGGR_HOLD_MAP                                         *
 *                                - Enable aggressive prefetch with speculative  *
 *                                  hold.                                        *
 *                                                                               *
 *  SCI_FLAG_READ_PREFETCH_NO_HOLD_MAP                                            * 
 *                                - The PSB66 will prefetch 64 bytes. As soon    *
 *                                  as the PCI read retry has been accepted,     *
 *                                  the stream will change state to FREE, even   *
 *                                  if less than 64 bytes were actually read.    *
 *                                                                               *
 *  SCI_FLAG_IO_MAP_IOSPACE       - Enable No Prefetch, no speculative hold.     *
 *                                                                               *
 *  SCI_FLAG_DMOVE_MAP            - Enable DMOVE packet type. The stream will be *
 *                                  set into FREE state immediately.             *
 *                                                                               *
 *  SCI_FLAG_WRITES_DISABLE_GATHER_MAP                                           *
 *                                - Disable use of gather.                       *
 *                                                                               *
 *  SCI_FLAG_DISABLE_128_BYTES_PACKETS                                           * 
 *                                - Disable use of 128-Byte packets              *
 *                                                                               *
 *  SCI_FLAG_CONDITIONAL_INTERRUPT_MAP                                           *
 *                                - Write operations through this map will cause *
 *                                  an atomic "fetch-and-add-one" operation on   *
 *                                  remote memory, but in addition an interrupt  *
 *                                  will be generated if the target memory       *
 *                                  location contained a "null value" before the *
 *                                  add operation was carried out.               *
 *                                  The conditional interrupt flag must also be  *
 *                                  specified in the SCIRegisterInterruptFlag()  *
 *                                  function.                                    *
 *                                                                               *
 *  SCI_FLAG_UNCONDITIONAL_INTERRUPT_MAP                                         *
 *                                - Write operations through this map will cause *
 *                                  an interrupt for the remote adapter          *
 *                                  "in addition to" updating the corresponding  *
 *                                  remote memory location with the data being   *
 *                                  written.                                     *
 *                                  The unconditional interrupt flag must also   *
 *                                  be specified in the                          *
 *                                  SCIRegisterInterruptFlag() function.         *
 *                                                                               *
 *  SCI_FLAG_WRITE_BACK_CACHE_MAP                                                *
 *                                - Enable cacheing of the mapped region.        *
 *                                  Writes through this map will be written to a *
 *                                  write back cache, hence no remote SCI updates*
 *                                  until the cache line is flushed. The         *
 *                                  application is responsible for the cache     *
 *                                  flush operation.                             *
 *                                  The SCImemCopy() function will handle this   *
 *                                  correctly by doing cache flushes internally. *
 *                                  This feature is architechture dependent and  *
 *                                  not be available on all plattforms.          *
 *                                                                               *
 *  SCI_FLAG_NO_MEMORY_LOOPBACK_MAP                                              *
 *                                - Forces a map to a remote segment located     *
 *                                  in the local machine to be mapped using      *
 *                                  SCI loopback. This is useful i.e. if you     *
 *                                  want to use a regular map access to be       *
 *                                  serialized with lock operations.             *
 *                                  The default behaviour is to access a remte   *
 *                                  segment located in the local machine as a    *
 *                                  local MMU operation.                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the offset and size is            *
 *                                  larger than the segment size.                *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as             *
 *                                  required by the implementation.              *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as           *
 *                                  required by the implementation.              *
 *                                                                               *
 *********************************************************************************/
#define SCIMapRemoteSegment                      _SISCI_EXPANDE_FUNCTION_NAME(SCIMapRemoteSegment)
DLL volatile void *SCIMapRemoteSegment(
                              sci_remote_segment_t segment,
                              sci_map_t            *map,
                              unsigned int         offset,
                              unsigned int         size,
                              void                *addr,
                              unsigned int         flags,
                              sci_error_t          *error);




/*********************************************************************************
 *                                                                               *
 * S C I   M A P   L O C A L   S E G M E N T                                     *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_FIXED_MAP_ADDR                                                      *
 *  SCI_FLAG_READONLY_MAP                                                        *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the offset and size is            *
 *                                  larger than the segment size.                *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as             *
 *                                  required by the implementation.              *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as           *
 *                                  required by the implementation.              *
 *                                                                               *
 *********************************************************************************/
#define SCIMapLocalSegment                       _SISCI_EXPANDE_FUNCTION_NAME(SCIMapLocalSegment)
DLL void *SCIMapLocalSegment(sci_local_segment_t segment,
                             sci_map_t           *map,
                             unsigned int        offset,
                             unsigned int        size,
                             void                *addr,
                             unsigned int        flags,
                             sci_error_t         *error);



/*********************************************************************************
 *                                                                               *
 * S C I   U N M A P  S E G M E N T                                              *
 *                                                                               *
 * This function unmaps pages of shared memory from the callers virtual          *
 * address space.                                                                *
 *                                                                               *
 *  Flags                                                                        *
 *    None.                                                                      *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_BUSY		          - The map is currently in use.                 *
 *                                                                               *
 *********************************************************************************/
#define SCIUnmapSegment                          _SISCI_EXPANDE_FUNCTION_NAME(SCIUnmapSegment)
DLL void SCIUnmapSegment(sci_map_t    map,
                         unsigned int flags,
                         sci_error_t  *error);




/*********************************************************************************
 *                                                                               *
 * S C I   C R E A T E   S E G M E N T                                           *
 *                                                                               *
 * Make the specified segment available for connections via the specified        *
 * adapter. If successful, the segment can be accessed from remote nodes         *
 * via the specified adapter.                                                    *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_USE_CALLBACK  - The callback function will be invoked for events    *
 *                           on this segment.                                    *
 *  SCI_FLAG_EMPTY         - No memory will be allocated for the segment.        *
 *  SCI_FLAG_PRIVATE       - The segment will be private meaning it will never   *
 *                           be any connections to it.                           *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_SEGMENTID_USED - The segment with this segmentId is already used     *
 *  SCI_ERR_SIZE_ALIGNMENT - Size is not correctly aligned as required           *
 *                           by the implementation.                              *
 *                                                                               *
 *********************************************************************************/
#define SCICreateSegment                         _SISCI_EXPANDE_FUNCTION_NAME(SCICreateSegment)
DLL void SCICreateSegment(sci_desc_t             sd,
                          sci_local_segment_t    *segment,
                          unsigned int           segmentId,
                          unsigned int           size,
                          sci_cb_local_segment_t callback,
                          void                   *callbackArg, 
                          unsigned int           flags,
                          sci_error_t            *error);



/*********************************************************************************
 *                                                                               *
 * S C I  W A I T   F O R   L O C A L   S E G M E N T  E V E N T                 *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_TIMEOUT      - The function timed out after specified timeout value. *
 *  SCI_ERR_CANCELLED    - The wait operation has been cancelled du to a         *
 *                         SCIRemoveSegment() on the same handle.                *
 *                         The handle is invalid when this error is returned.    *
 *                                                                               *
 *********************************************************************************/
#define SCIWaitForLocalSegmentEvent              _SISCI_EXPANDE_FUNCTION_NAME(SCIWaitForLocalSegmentEvent)
DLL sci_segment_cb_reason_t SCIWaitForLocalSegmentEvent(
                                                sci_local_segment_t segment,     
                                                unsigned int *sourcenodeId,
                                                unsigned int *localAdapterNo,
                                                unsigned int timeout,
                                                unsigned int flags,
                                                sci_error_t  *error);



/*********************************************************************************
 *                                                                               *
 * S C I   P R E P A R E   S E G M E N T                                         *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_DMA_SOURCE_ONLY - The segment will be used as a source segment      *
 *                             for DMA operations. On some system types this     *
 *                             will enable the SISCI driver to use performance   *
 *                             improving features.                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIPrepareSegment                        _SISCI_EXPANDE_FUNCTION_NAME(SCIPrepareSegment)
DLL void SCIPrepareSegment(sci_local_segment_t segment,
                           unsigned int        localAdapterNo,
                           unsigned int        flags,
                           sci_error_t         *error);



/*********************************************************************************
 *                                                                               *
 * S C I   R E M O V E   S E G M E N T                                           *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_BUSY  - Unable to remove the segment. The segment is currently       *
 *                  in use.                                                      *
 *                                                                               *
 *********************************************************************************/
#define SCIRemoveSegment                         _SISCI_EXPANDE_FUNCTION_NAME(SCIRemoveSegment)
DLL void SCIRemoveSegment(sci_local_segment_t segment,
                          unsigned int        flags, 
                          sci_error_t         *error);




/*********************************************************************************
 *                                                                               *
 * S C I   S E T   S E G M E N T   A V A I L A B L E                             *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  SCI_ERR_SEGMENT_NOT_PREPARED  - The segment has not been prepared for access *
 *                                  from this adapter.                           *
 *  SCI_ERR_ILLEGAL_OPERATION     - The segment is created with the              *
 *                                  SCI_FLAG_PRIVATE flag specified and          *
 *                                  therefore has no segmentId.                  *
 *                                                                               *
 *********************************************************************************/
#define SCISetSegmentAvailable                   _SISCI_EXPANDE_FUNCTION_NAME(SCISetSegmentAvailable)
DLL void SCISetSegmentAvailable(sci_local_segment_t segment,
                                unsigned int        localAdapterNo,
                                unsigned int        flags,
                                sci_error_t         *error);



/*********************************************************************************
 *                                                                               *
 * S C I   S E T   S E G M E N T   U N A V A I L A B L E                         *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_FORCE_DISCONNECT                                                    *
 *  SCI_FLAG_NOTIFY                                                              *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation.                           *
 *                                                                               *
 *********************************************************************************/
#define SCISetSegmentUnavailable                 _SISCI_EXPANDE_FUNCTION_NAME(SCISetSegmentUnavailable)
DLL void SCISetSegmentUnavailable(sci_local_segment_t segment,
                                  unsigned int        localAdapterNo,
                                  unsigned int        flags,
                                  sci_error_t         *error);




/*********************************************************************************
 *                                                                               *
 * S C I  C R E A T E   M A P   S E Q U E N C E                                  *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_FAST_BARRIER                                                        *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCICreateMapSequence                     _SISCI_EXPANDE_FUNCTION_NAME(SCICreateMapSequence)
DLL void SCICreateMapSequence(sci_map_t   map, 
                           sci_sequence_t *sequence, 
                           unsigned int   flags, 
                           sci_error_t    *error);



/*********************************************************************************
 *                                                                               *
 * S C I   R E M O V E   S E Q U E N C E                                         *
 *                                                                               *
 *  Flags:                                                                       *
 *   None                                                                        *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCIRemoveSequence                        _SISCI_EXPANDE_FUNCTION_NAME(SCIRemoveSequence)
DLL void SCIRemoveSequence(sci_sequence_t sequence, 
                           unsigned int   flags, 
                           sci_error_t    *error);



/*********************************************************************************
 *                                                                               *
 * S C I   S T A R T   S E Q U E N C E                                           *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIStartSequence                         _SISCI_EXPANDE_FUNCTION_NAME(SCIStartSequence)
DLL sci_sequence_status_t SCIStartSequence(sci_sequence_t sequence,
                                           unsigned int   flags,
                                           sci_error_t    *error);



/*********************************************************************************
 *                                                                               *
 * S C I   C H E C K   S E Q U E N CE                                            *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_NO_FLUSH                                                            *
 *  SCI_FLAG_NO_STORE_BARRIER                                                    *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCICheckSequence                         _SISCI_EXPANDE_FUNCTION_NAME(SCICheckSequence)
DLL sci_sequence_status_t SCICheckSequence(sci_sequence_t sequence, 
                                           unsigned int   flags,
                                           sci_error_t    *error);



/*********************************************************************************
 *                                                                               *
 * S C I   S T O R E   B A R R I E R                                             *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIStoreBarrier                          _SISCI_EXPANDE_FUNCTION_NAME(SCIStoreBarrier)
DLL void SCIStoreBarrier(sci_sequence_t sequence,
                         unsigned int   flags);




/*********************************************************************************
 *                                                                               *
 * S C I   F L U S H   R E A D   B U F F E R S                                   *
 *                                                                               *
 *********************************************************************************/
#define SCIFlushReadBuffers                      _SISCI_EXPANDE_FUNCTION_NAME(SCIFlushReadBuffers)
DLL void SCIFlushReadBuffers(sci_sequence_t sequence);




/*********************************************************************************
 *                                                                               *
 * S C I   P R O B E   N O D E                                                   *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_NO_LINK_ACCESS        - It was not possible to communicate via the   *
 *                                  local adapter.                               *
 *  SCI_ERR_NO_REMOTE_LINK_ACCESS - It was not possible to communicate via a     *
 *                                  remote switch port.                          *
 *                                                                               *
 *********************************************************************************/
#define SCIProbeNode                             _SISCI_EXPANDE_FUNCTION_NAME(SCIProbeNode)
DLL int SCIProbeNode(sci_desc_t   sd,
                     unsigned int localAdapterNo,
                     unsigned int nodeId,
                     unsigned int flags,
                     sci_error_t  *error);



/*********************************************************************************
 *                                                                               *
 * S C I   G E T   C S R   R E G I S T E R                                       *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_NO_LINK_ACCESS        - It was not possible to communicate via the   *
 *                                  local adapter.                               *
 *  SCI_ERR_NO_REMOTE_LINK_ACCESS - It was not possible to communicate via a     *
 *                                  remote switch port.                          *
 *                                                                               *
 *********************************************************************************/
#define SCIGetCSRRegister                        _SISCI_EXPANDE_FUNCTION_NAME(SCIGetCSRRegister)
DLL unsigned int SCIGetCSRRegister(sci_desc_t   sd,
                                   unsigned int localAdapterNo,
                                   unsigned int SCINodeId,
                                   unsigned int CSROffset,
                                   unsigned int flags,
                                   sci_error_t  *error);



/*********************************************************************************
 *                                                                               *
 * S C I   S E T   C S R   R E G I S T E R                                       *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_NO_LINK_ACCESS        - It was not possible to communicate via the   *
 *                                  local adapter.                               *
 *  SCI_ERR_NO_REMOTE_LINK_ACCESS - It was not possible to communicate via a     *
 *                                  remote switch port.                          *
 *                                                                               *
 *********************************************************************************/
#define SCISetCSRRegister                        _SISCI_EXPANDE_FUNCTION_NAME(SCISetCSRRegister)
DLL void SCISetCSRRegister(sci_desc_t   sd,
                           unsigned int localAdapterNo,
                           unsigned int SCINodeId,
                           unsigned int CSROffset,
                           unsigned int CSRValue,
                           unsigned int flags,
                           sci_error_t  *error);


/*********************************************************************************
 *                                                                               *
 * S C I   G E T   L O C A L   C S R                                             *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIGetLocalCSR                           _SISCI_EXPANDE_FUNCTION_NAME(SCIGetLocalCSR)
DLL unsigned int SCIGetLocalCSR(sci_desc_t   sd,
                                unsigned int localAdapterNo,
                                unsigned int CSROffset,
                                unsigned int flags,
                                sci_error_t  *error);



/*********************************************************************************
 *                                                                               *
 * S C I   S E T   L O C A L   C S R                                             *
 *                                                                               *
 *  SISCI Priveleged function
 *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCISetLocalCSR                           _SISCI_EXPANDE_FUNCTION_NAME(SCISetLocalCSR)
DLL void SCISetLocalCSR(sci_desc_t        sd,
                        unsigned int      localAdapterNo,
                        unsigned int      CSROffset,
                        unsigned int      CSRValue,
                        unsigned int      flags,
                        sci_error_t       *error);



/*********************************************************************************
 *                                                                               *
 * S C I   A T T A C H   P H Y S I C A L   M E M O R Y                           *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *  Description:                                                                 *
 *                                                                               *
 *  This function enables usage of physical devices and memory regions where the *
 *  Physical PCI bus address ( and mapped CPU address ) are already known.       *
 *  The function will register the physical memory as a SISCI segment which can  *
 *  be connected and mapped as a regular SISCI segment.                          *
 *                                                                               *
 *  Requirements:                                                                *
 *                                                                               *
 *  SCICreateSegment() with flag SCI_FLAG_EMPTY must have been called in advance *                  
 *                                                                               *                 
 *  Parameter description:                                                       *  
 *  sci_ioaddr_t ioaddress : This is the address on the PCI bus that a PCI bus   *
 *                           master has to use to write to the specified memory  *
 *  void * address         : This is the (mapped) virtual address that the       *
 *                           application has to use to access the device.        *
 *                           This means that the device has to be mapped in      *
 *                           advance bye the devices own driver.                 *
 *                           If the device is not to be accessed by the local    *
 *                           CPU, the address pointer shold be set to NULL       *
 *   Flags                                                                       *
 *                                                                               *
 *   None                                                                        *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIAttachPhysicalMemory                  _SISCI_EXPANDE_FUNCTION_NAME(SCIAttachPhysicalMemory)
DLL void SCIAttachPhysicalMemory(sci_ioaddr_t         ioaddress,
                                 void                *address,
                                 unsigned int         busNo,
                                 unsigned int         size,
                                 sci_local_segment_t  segment,
                                 unsigned int         flags,
                                 sci_error_t         *error);



/*********************************************************************************
 *                                                                               *
 * S C I   Q U E R Y                                                             *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_QUERY         - Unrecognized command.                        *
 *                                                                               *
 *********************************************************************************/
#define SCIQuery                                 _SISCI_EXPANDE_FUNCTION_NAME(SCIQuery)
DLL void SCIQuery(unsigned int command,
                  void         *data,
                  unsigned int flags,
                  sci_error_t  *error);


/* MAJOR QUERY COMMANDS                                             */

/* This command requires a pointer to a structure of type           */
/* "sci_query_string". The string will be filled in by the query.   */
#define SCI_Q_VENDORID                                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_VENDORID)
extern const unsigned int SCI_Q_VENDORID; 


/* Same as for SCI_VENDOR_ID                                        */
#define SCI_Q_API                                          _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_API)
extern const unsigned int SCI_Q_API;       


/* User passes a pointer to an allocated object of the              */
/* "sci_query_adapter" struct.                                      */
#define SCI_Q_ADAPTER                                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER)
extern const unsigned int SCI_Q_ADAPTER;


/* User passes a pointer to an allocated object of the              */
/* "sci_query_system" struct.                                       */
#define SCI_Q_SYSTEM                                       _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SYSTEM)
extern const unsigned int SCI_Q_SYSTEM; 

#define SCI_Q_LOCAL_SEGMENT                                _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_LOCAL_SEGMENT)
extern const unsigned int SCI_Q_LOCAL_SEGMENT; 

#define SCI_Q_REMOTE_SEGMENT                                _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_REMOTE_SEGMENT)
extern const unsigned int SCI_Q_REMOTE_SEGMENT; 

#define SCI_Q_MAP                                           _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_MAP)
extern const unsigned int SCI_Q_MAP; 

typedef struct {
    char         *str;                     /* Pointer to a string of minimum "length" characters */
    unsigned int length;
} sci_query_string_t;


typedef struct {
    unsigned int localAdapterNo;           /* The adapter no. that the query concern.          */
    unsigned int portNo;                   /* The SCI Link port number that the query concern. */
    unsigned int subcommand;               /* A subcommand as specified below.                 */
    void         *data;                    /* A pointer to an unsigned int that will return    */
                                           /* the response to the query.                       */
} sci_query_adapter_t;


typedef struct {
    unsigned int subcommand;               /* A subcommand as specified below.                 */
    void         *data;                    /* A pointer to an unsigned int that will return    */
                                           /* the response to the query.                       */
} sci_query_system_t;

typedef struct {
    sci_local_segment_t  segment;
    unsigned int         subcommand;
    union {
        sci_ioaddr_t     ioaddr;
    }                    data;
} sci_query_local_segment_t;

typedef struct {
    sci_remote_segment_t segment;
    unsigned int         subcommand;
    union {
        sci_ioaddr_t     ioaddr;
    }                    data;
} sci_query_remote_segment_t;

typedef struct {
    sci_map_t            map;
    unsigned int         subcommand;
    unsigned int         data;
} sci_query_map_t;

/* Minor query commands (sub-commands) for adapter specific information SCI_ADAPTER */
#define SCI_Q_ADAPTER_DMA_SIZE_ALIGNMENT                   _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_DMA_SIZE_ALIGNMENT)
extern const unsigned int SCI_Q_ADAPTER_DMA_SIZE_ALIGNMENT;

#define SCI_Q_ADAPTER_DMA_OFFSET_ALIGNMENT                 _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_DMA_OFFSET_ALIGNMENT)
extern const unsigned int SCI_Q_ADAPTER_DMA_OFFSET_ALIGNMENT;

#define SCI_Q_ADAPTER_DMA_MTU                              _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_DMA_MTU)
extern const unsigned int SCI_Q_ADAPTER_DMA_MTU;

#define SCI_Q_ADAPTER_SUGGESTED_MIN_DMA_SIZE               _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_SUGGESTED_MIN_DMA_SIZE)
extern const unsigned int SCI_Q_ADAPTER_SUGGESTED_MIN_DMA_SIZE;

#define SCI_Q_ADAPTER_SUGGESTED_MIN_BLOCK_SIZE             _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_SUGGESTED_MIN_BLOCK_SIZE)
extern const unsigned int SCI_Q_ADAPTER_SUGGESTED_MIN_BLOCK_SIZE;

#define SCI_Q_ADAPTER_NODEID                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_NODEID)
extern const unsigned int SCI_Q_ADAPTER_NODEID;

#define SCI_Q_ADAPTER_SERIAL_NUMBER                        _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_SERIAL_NUMBER)
extern const unsigned int SCI_Q_ADAPTER_SERIAL_NUMBER;

#define SCI_Q_ADAPTER_CARD_TYPE                            _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_CARD_TYPE)
extern const unsigned int SCI_Q_ADAPTER_CARD_TYPE;

#define SCI_Q_ADAPTER_NUMBER_OF_STREAMS                    _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_NUMBER_OF_STREAMS)
extern const unsigned int SCI_Q_ADAPTER_NUMBER_OF_STREAMS;

#define SCI_Q_ADAPTER_STREAM_BUFFER_SIZE                   _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_STREAM_BUFFER_SIZE)
extern const unsigned int SCI_Q_ADAPTER_STREAM_BUFFER_SIZE;

#define SCI_Q_ADAPTER_CONFIGURED                           _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_CONFIGURED)
extern const unsigned int SCI_Q_ADAPTER_CONFIGURED;

#define SCI_Q_ADAPTER_LINK_OPERATIONAL                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_LINK_OPERATIONAL)
extern const unsigned int SCI_Q_ADAPTER_LINK_OPERATIONAL;

#define SCI_Q_ADAPTER_HW_LINK_STATUS_IS_OK                 _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_HW_LINK_STATUS_IS_OK)
extern const unsigned int SCI_Q_ADAPTER_HW_LINK_STATUS_IS_OK;

#define SCI_Q_ADAPTER_NUMBER                               _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_NUMBER)
extern const unsigned int SCI_Q_ADAPTER_NUMBER;

#define SCI_Q_ADAPTER_INSTANCE_NUMBER                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_INSTANCE_NUMBER)
extern const unsigned int SCI_Q_ADAPTER_INSTANCE_NUMBER;

#define SCI_Q_ADAPTER_FIRMWARE_OK                          _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_FIRMWARE_OK)
extern const unsigned int SCI_Q_ADAPTER_FIRMWARE_OK;

#define SCI_Q_ADAPTER_CONNECTED_TO_SWITCH                  _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_CONNECTED_TO_SWITCH)
extern const unsigned int SCI_Q_ADAPTER_CONNECTED_TO_SWITCH;

#define SCI_Q_ADAPTER_LOCAL_SWITCH_TYPE                    _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_LOCAL_SWITCH_TYPE)
extern const unsigned int SCI_Q_ADAPTER_LOCAL_SWITCH_TYPE;

#define SCI_Q_ADAPTER_LOCAL_SWITCH_PORT_NUMBER             _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_LOCAL_SWITCH_PORT_NUMBER)
extern const unsigned int SCI_Q_ADAPTER_LOCAL_SWITCH_PORT_NUMBER;

#define SCI_Q_ADAPTER_CONNECTED_TO_EXPECTED_SWITCH_PORT    _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_CONNECTED_TO_EXPECTED_SWITCH_PORT)
extern const unsigned int SCI_Q_ADAPTER_CONNECTED_TO_EXPECTED_SWITCH_PORT;

#define SCI_Q_ADAPTER_ATT_PAGE_SIZE                        _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_ATT_PAGE_SIZE)
extern const unsigned int SCI_Q_ADAPTER_ATT_PAGE_SIZE;

#define SCI_Q_ADAPTER_ATT_NUMBER_OF_ENTRIES                _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_ATT_NUMBER_OF_ENTRIES)
extern const unsigned int SCI_Q_ADAPTER_ATT_NUMBER_OF_ENTRIES;

#define SCI_Q_ADAPTER_ATT_AVAILABLE_ENTRIES                _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_ATT_AVAILABLE_ENTRIES)
extern const unsigned int SCI_Q_ADAPTER_ATT_AVAILABLE_ENTRIES;

#define SCI_Q_ADAPTER_PHYS_MEM_NODEID                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SCI_Q_ADAPTER_PHYS_MEM_NODEID)
extern const unsigned int SCI_Q_ADAPTER_PHYS_MEM_NODEID;

#define SCI_Q_ADAPTER_PHYS_MBX_NODEID                      _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SCI_Q_ADAPTER_PHYS_MBX_NODEID)
extern const unsigned int SCI_Q_ADAPTER_PHYS_MBX_NODEID;

#define SCI_Q_ADAPTER_PHYS_LINK_PORT_NODEID                _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_PHYS_LINK_PORT_NODEID)
extern const unsigned int SCI_Q_ADAPTER_PHYS_LINK_PORT_NODEID;

#define SCI_Q_ADAPTER_SCI_LINK_FREQUENCY                   _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_SCI_LINK_FREQUENCY)
extern const unsigned int SCI_Q_ADAPTER_SCI_LINK_FREQUENCY;

#define SCI_Q_ADAPTER_B_LINK_FREQUENCY                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_B_LINK_FREQUENCY)
extern const unsigned int SCI_Q_ADAPTER_B_LINK_FREQUENCY;

#define SCI_Q_ADAPTER_IO_BUS_FREQUENCY                     _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_ADAPTER_IO_BUS_FREQUENCY)
extern const unsigned int SCI_Q_ADAPTER_IO_BUS_FREQUENCY;

/* Minor query commands (sub-commands) for adapter specific information SCI_SYSTEM */
#define SCI_Q_SYSTEM_HOSTBRIDGE                            _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SYSTEM_HOSTBRIDGE)
extern const unsigned int SCI_Q_SYSTEM_HOSTBRIDGE;

#define SCI_Q_SYSTEM_WRITE_POSTING_ENABLED                 _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SYSTEM_WRITE_POSTING_ENABLED)
extern const unsigned int SCI_Q_SYSTEM_WRITE_POSTING_ENABLED;

#define SCI_Q_SYSTEM_WRITE_COMBINING_ENABLED               _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_SYSTEM_WRITE_COMBINING_ENABLED)
extern const unsigned int SCI_Q_SYSTEM_WRITE_COMBINING_ENABLED;

#define SCI_Q_LOCAL_SEGMENT_IOADDR                         _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_LOCAL_SEGMENT_IOADDR)
extern const unsigned int SCI_Q_LOCAL_SEGMENT_IOADDR;

#define SCI_Q_REMOTE_SEGMENT_IOADDR                         _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_REMOTE_SEGMENT_IOADDR)
extern const unsigned int SCI_Q_REMOTE_SEGMENT_IOADDR;

#define SCI_Q_MAP_MAPPED_TO_LOCAL_TARGET                    _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_MAP_MAPPED_TO_LOCAL_TARGET)
extern const unsigned int SCI_Q_MAP_MAPPED_TO_LOCAL_TARGET;

#define SCI_Q_MAP_QUERY_REMOTE_MAPPED_TO_LOCAL_TARGET       _SISCI_EXPANDE_CONSTANT_NAME(SCI_Q_MAP_QUERY_REMOTE_MAPPED_TO_LOCAL_TARGET)
extern const unsigned int SCI_Q_MAP_QUERY_REMOTE_MAPPED_TO_LOCAL_TARGET;

#define HOSTBRIDGE_NOT_AVAILABLE                           _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_NOT_AVAILABLE)
extern const unsigned int HOSTBRIDGE_NOT_AVAILABLE; 

#define HOSTBRIDGE_UNKNOWN                                 _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_UNKNOWN)
extern const unsigned int HOSTBRIDGE_UNKNOWN; 

#define HOSTBRIDGE_440FX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_440FX)
extern const unsigned int HOSTBRIDGE_440FX;  

#define HOSTBRIDGE_440LX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_440LX)
extern const unsigned int HOSTBRIDGE_440LX;   

#define HOSTBRIDGE_440BX_A                                 _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_440BX_A)
extern const unsigned int HOSTBRIDGE_440BX_A;

#define HOSTBRIDGE_440BX_B                                 _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_440BX_B)
extern const unsigned int HOSTBRIDGE_440BX_B;

#define HOSTBRIDGE_440GX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_440GX)
extern const unsigned int HOSTBRIDGE_440GX; 

#define HOSTBRIDGE_450KX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_450KX)
extern const unsigned int HOSTBRIDGE_450KX; 

#define HOSTBRIDGE_430NX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_430NX)
extern const unsigned int HOSTBRIDGE_430NX;

#define HOSTBRIDGE_450NX                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_450NX)
extern const unsigned int HOSTBRIDGE_450NX;

#define HOSTBRIDGE_450NX_MICO                              _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_450NX_MICO)
extern const unsigned int HOSTBRIDGE_450NX_MICO;

#define HOSTBRIDGE_450NX_PXB                               _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_450NX_PXB)
extern const unsigned int HOSTBRIDGE_450NX_PXB;

#define HOSTBRIDGE_I810                                    _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I810)
extern const unsigned int HOSTBRIDGE_I810;

#define HOSTBRIDGE_I810_DC100                              _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I810_DC100)
extern const unsigned int HOSTBRIDGE_I810_DC100;

#define HOSTBRIDGE_I810E                                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I810E)
extern const unsigned int HOSTBRIDGE_I810E;

#define HOSTBRIDGE_I815                                    _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I815)
extern const unsigned int HOSTBRIDGE_I815;

#define HOSTBRIDGE_I840                                    _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I840)
extern const unsigned int HOSTBRIDGE_I840;

#define HOSTBRIDGE_I850                                    _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I850)
extern const unsigned int HOSTBRIDGE_I850;

#define HOSTBRIDGE_I860                                    _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_I860)
extern const unsigned int HOSTBRIDGE_I860;

#define HOSTBRIDGE_VIA_KT133                               _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_VIA_KT133)
extern const unsigned int HOSTBRIDGE_VIA_KT133;

#define HOSTBRIDGE_VIA_KX133                               _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_VIA_KX133)
extern const unsigned int HOSTBRIDGE_VIA_KX133;

#define HOSTBRIDGE_VIA_APOLLO_PRO_133A                     _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_VIA_APOLLO_PRO_133A)
extern const unsigned int HOSTBRIDGE_VIA_APOLLO_PRO_133A;

#define HOSTBRIDGE_VIA_APOLLO_PRO_266                       _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_VIA_APOLLO_PRO_266)
extern const unsigned int HOSTBRIDGE_VIA_APOLLO_PRO_266;

#define HOSTBRIDGE_AMD_760_MP                              _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_AMD_760_MP)
extern const unsigned int HOSTBRIDGE_AMD_760_MP;

#define HOSTBRIDGE_SERVERWORKS_HE                          _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_SERVERWORKS_HE)
extern const unsigned int HOSTBRIDGE_SERVERWORKS_HE;

#define HOSTBRIDGE_SERVERWORKS_HE_B                        _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_SERVERWORKS_HE_B)
extern const unsigned int HOSTBRIDGE_SERVERWORKS_HE_B;

#define HOSTBRIDGE_SERVERWORKS_LE                          _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_SERVERWORKS_LE)
extern const unsigned int HOSTBRIDGE_SERVERWORKS_LE;



#define HOSTBRIDGE_WRITE_POSTING_DISABLED                  _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_WRITE_POSTING_DISABLED)
extern const unsigned int HOSTBRIDGE_WRITE_POSTING_DISABLED;

#define HOSTBRIDGE_WRITE_POSTING_ENABLED                   _SISCI_EXPANDE_CONSTANT_NAME(HOSTBRIDGE_WRITE_POSTING_ENABLED)
extern const unsigned int HOSTBRIDGE_WRITE_POSTING_ENABLED;




/*********************************************************************************
 *                                                                               *
 * S C I   C R E A T E   D M A   Q U E U E                                       *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *   SCI_FLAG_DMA_PHDMA : Create physical DMA queue. Please note that this is an *
 *                        priveleged operation.                                  *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCICreateDMAQueue                        _SISCI_EXPANDE_FUNCTION_NAME(SCICreateDMAQueue)
DLL void SCICreateDMAQueue(sci_desc_t      sd,
                           sci_dma_queue_t *dq,
                           unsigned int    localAdapterNo,
                           unsigned int    maxEntries,
                           unsigned int    flags,
                           sci_error_t     *error);



/*********************************************************************************
 *                                                                               *
 * S C I   R E M O V E   D M A   Q U E U E                                       *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *                                                                               *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Not allowed in this queue state.             *
 *                                                                               *
 *********************************************************************************/
#define SCIRemoveDMAQueue                        _SISCI_EXPANDE_FUNCTION_NAME(SCIRemoveDMAQueue)
DLL void SCIRemoveDMAQueue(sci_dma_queue_t dq,
                           unsigned int    flags,
                           sci_error_t     *error);



/*********************************************************************************
 *                                                                               *
 * S C I   E N Q U E U E   D M A   T R A N S F E R                               *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_DMA_READ             - The DMA will be remote --> local             *
 *                                  (default is local --> remote)                *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the offset and size is larger     *
 *                                  than the segment size or larger than max     *
 *                                  DMA size.                                    *
 *  SCI_ERR_MAX_ENTRIES	          - The DMA queue is full                        *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation                            *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as required    *
 *                                  by the implementation.                       *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as required  *
 *                                  by the implementation.                       *
 *  SCI_ERR_SEGMENT_NOT_PREPARED  - The local segment has not been prepared for  *
 *                                  access from the adapter associated with the  *
 *                                  queue.                                       *
 *  SCI_ERR_SEGMENT_NOT_CONNECTED - The remote segment is not connected through  *
 *                                  the adapter associated with the queue.       *   
 *********************************************************************************/
#define SCIEnqueueDMATransfer                    _SISCI_EXPANDE_FUNCTION_NAME(SCIEnqueueDMATransfer)
DLL sci_dma_queue_state_t SCIEnqueueDMATransfer(sci_dma_queue_t      dq,
                               sci_local_segment_t  localSegment,
                               sci_remote_segment_t remoteSegment,
                               unsigned int         localOffset,
                               unsigned int         remoteOffset,
                               unsigned int         size,
                               unsigned int         flags,
                               sci_error_t          *error);



/*********************************************************************************
 *                                                                               *
 * S C I   P O S T   D M A   Q U E U E                                           *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_USE_CALLBACK      - The end of the transfer will cause the callback *
 *                               function to be invoked.                         *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION  - Illegal operation                               *
 *                                                                               *
 *********************************************************************************/
#define SCIPostDMAQueue                          _SISCI_EXPANDE_FUNCTION_NAME(SCIPostDMAQueue)
DLL void SCIPostDMAQueue(sci_dma_queue_t dq, 
                         sci_cb_dma_t    callback, 
                         void            *callbackArg,
                         unsigned int    flags,
                         sci_error_t     *error);



/*********************************************************************************
 *                                                                               *
 * S C I   A B O R T   D M A   Q U E U E                                         *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation                            *
 *                                                                               *
 *********************************************************************************/
#define SCIAbortDMAQueue                         _SISCI_EXPANDE_FUNCTION_NAME(SCIAbortDMAQueue)
DLL void SCIAbortDMAQueue(sci_dma_queue_t dq,
                          unsigned int    flags,
                          sci_error_t     *error);


/*********************************************************************************
 *                                                                               *
 * S C I   R E S E T   D M A   Q U E U E                                         *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *                                                                               *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIResetDMAQueue                         _SISCI_EXPANDE_FUNCTION_NAME(SCIResetDMAQueue)
DLL void SCIResetDMAQueue(sci_dma_queue_t dq,
                          unsigned int    flags,
                          sci_error_t     *error);



/*********************************************************************************
 *                                                                               *
 * S C I   D M A   Q U E U E   S T A T E                                         *
 *                                                                               *
 *********************************************************************************/
#define SCIDMAQueueState                         _SISCI_EXPANDE_FUNCTION_NAME(SCIDMAQueueState)
DLL sci_dma_queue_state_t SCIDMAQueueState(sci_dma_queue_t dq);



/*********************************************************************************
 *                                                                               *
 * S C I   W A I T   F O R   D M A   Q U E U E                                   *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation                            *
 *  SCI_ERR_TIMEOUT               - The function timed out after specified       *
 *                                  timeout value.                               * 
 *                                                                               *
 *********************************************************************************/
#define SCIWaitForDMAQueue                       _SISCI_EXPANDE_FUNCTION_NAME(SCIWaitForDMAQueue)
DLL sci_dma_queue_state_t SCIWaitForDMAQueue(sci_dma_queue_t dq,
                                             unsigned int    timeout,
                                             unsigned int    flags,
                                             sci_error_t     *error);




/*********************************************************************************
 *                                                                               *
 *    S C I   P H   D M A   E N Q U E U E                                        *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *   SCI_FLAG_DMA_READ                                                           *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCIphDmaEnqueue _SISCI_EXPANDE_FUNCTION_NAME(SCIphDmaEnqueue)
DLL void SCIphDmaEnqueue(sci_dma_queue_t  dmaqueue,
                         unsigned int     size,
                         sci_ioaddr_t     localBusAddr,
                         unsigned int     remote_nodeid,
                         unsigned int     remote_highaddr,
                         unsigned int     remote_lowaddr,
                         unsigned int     flags,
                         sci_error_t     *error);

/*********************************************************************************
 *                                                                               *
 *    S C I   P H   D M A   S T A R T                                            *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_DMA_WAIT                                                            *
 *  SCI_FLAG_USE_CALLBACK                                                        *
 *  SCI_FLAG_DMA_RESET                                                           *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCIphDmaStart _SISCI_EXPANDE_FUNCTION_NAME(SCIphDmaStart)
DLL sci_dma_queue_state_t SCIphDmaStart(sci_dma_queue_t  dmaqueue,
                                        sci_cb_dma_t     callback,
                                        void            *callbackArg,
                                        unsigned int     flags,
                                        sci_error_t     *error);

/*********************************************************************************
 *                                                                               *
 * S C I   C R E A T E   I N T E R R U P T                                       *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_USE_CALLBACK                                                        *
 *  SCI_FLAG_FIXED_INTNO                                                         *
 *  SCI_FLAG_SHARED_INT                                                          *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_INTNO_USED     - This interrupt number is already used.              *
 *                                                                               *
 *********************************************************************************/
#define SCICreateInterrupt                       _SISCI_EXPANDE_FUNCTION_NAME(SCICreateInterrupt)
DLL void SCICreateInterrupt(sci_desc_t            sd,
                            sci_local_interrupt_t *interrupt,
                            unsigned int          localAdapterNo,
                            unsigned int          *interruptNo,
                            sci_cb_interrupt_t    callback,
                            void                  *callbackArg,
                            unsigned int          flags,
                            sci_error_t           *error);



/*********************************************************************************
 *                                                                               *
 * S C I   R E M O V E   I N T E R R U P T                                       *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *********************************************************************************/
#define SCIRemoveInterrupt                       _SISCI_EXPANDE_FUNCTION_NAME(SCIRemoveInterrupt)
DLL void SCIRemoveInterrupt(sci_local_interrupt_t interrupt,
                            unsigned int          flags,
                            sci_error_t           *error);




/*********************************************************************************
 *                                                                               *
 * S C I   W A I T   F O R   I N T E R R U P T                                   *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_TIMEOUT     - The function timed out after specified timeout value.  *
 *  SCI_ERR_CANCELLED   - The wait was interrupted by a call to                  *
 *                        SCIRemoveInterrupt.                                    *
 *                        The handle is invalid when this error code is returned.*
 *                                                                               *
 *********************************************************************************/
#define SCIWaitForInterrupt                      _SISCI_EXPANDE_FUNCTION_NAME(SCIWaitForInterrupt)
DLL void SCIWaitForInterrupt(sci_local_interrupt_t interrupt, 
                             unsigned int          timeout,
                             unsigned int          flags, 
                             sci_error_t           *error);




/*********************************************************************************
 *                                                                               *
 * S C I   C O N N E C T  I N T E R R U P T                                      *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_NO_SUCH_INTNO	    - No such interrupt number.                      *
 *  SCI_ERR_CONNECTION_REFUSED  - Connection attempt refused by remote node.     *
 *  SCI_ERR_TIMEOUT             - The function timed out after specified         *
 *                                timeout value.                                 *
 *                                                                               *
 *********************************************************************************/
#define SCIConnectInterrupt                      _SISCI_EXPANDE_FUNCTION_NAME(SCIConnectInterrupt)
DLL void SCIConnectInterrupt(sci_desc_t             sd,
                             sci_remote_interrupt_t *interrupt,
                             unsigned int           nodeId,
                             unsigned int           localAdapterNo,
                             unsigned int           interruptNo,
                             unsigned int           timeout,
                             unsigned int           flags,
                             sci_error_t            *error);


/*********************************************************************************
 *                                                                               *
 * S C I   D I S C O N N E C T   I N T E R R U P T                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIDisconnectInterrupt                   _SISCI_EXPANDE_FUNCTION_NAME(SCIDisconnectInterrupt)
DLL void SCIDisconnectInterrupt(sci_remote_interrupt_t interrupt,
                                unsigned int           flags,
                                sci_error_t            *error);




/*********************************************************************************
 *                                                                               *
 * S C I   T R I G G E R   I N T E R R U P T                                     *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCITriggerInterrupt                      _SISCI_EXPANDE_FUNCTION_NAME(SCITriggerInterrupt)
DLL void SCITriggerInterrupt(sci_remote_interrupt_t interrupt,
                             unsigned int           flags,
                             sci_error_t            *error);




/*********************************************************************************
 *                                                                               *
 *  S C I   R E G I S T E R   I N T E R R U P T  F L A G                         *
 *                                                                               *
 *                                                                               *
 *  This function register an "interrupt flag" that is identified as an unique   *
 *  location within a local segment. If successful, the resulting interrupt      *
 *  handle will have been associated with the specified local segment.           *
 *                                                                               *
 *  It is up to the (remote) client(s) to set up an "interrupt mapping" for the  *
 *  corresponding segment offset using either the                                *
 *                                                                               *
 *  - SCI_FLAG_CONDITIONAL_INTERRUPT_MAP                                         *
 *                                                                               *
 *     or the                                                                    *
 *                                                                               *
 *  -  SCI_FLAG_UNCONDITIONAL_DATA_INTERRUPT_MAP                                 *
 *                                                                               *
 *  option to "SCIMapRemoteSegment()". - I.e. after having established a         *
 *  connection to the corresponding segment. A trigger operation can then        *
 *  be implemented using a store operation via the relevant "interrupt map".     *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *  Flags:                                                                       *
 *                                                                               *
 *  SCI_FLAG_CONDITIONAL_INTERRUPT - Triggering is to take place using           *
 *                                   "conditional interrupts".                   *
 *                                                                               *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *     None.                                                                     *
 *                                                                               *
 *********************************************************************************/
#define SCIRegisterInterruptFlag                 _SISCI_EXPANDE_FUNCTION_NAME(SCIRegisterInterruptFlag)
DLL void SCIRegisterInterruptFlag(
                            unsigned int           localAdapterNo,
                            sci_local_interrupt_t *interrupt,
                            sci_local_segment_t    segment,
                            unsigned int           offset,
                            sci_cb_interrupt_t     callback,
                            void                  *callbackArg,
                            unsigned int           flags,
                            sci_error_t           *error);




/*********************************************************************************
 *                                                                               *
 *  S C I   E N A B L E   C O N D I T I O N A L   I N T E R R U P T              *
 *                                                                               *
 *                                                                               *
 *  This function make sure that another HW interrupt will take place the next   *
 *  time the corresponding interrupt flag is triggered by a                      *
 *  "conditional interrupt" operation.                                           *
 *                                                                               *
 *  Default semantics:                                                           *
 *                                                                               *
 *  When successful, the client can rely on that the first subsequent trigger    *
 *  operation will cause a HW interrupt and subsequently cause the client        *
 *  handler function to be invoked.                                              *
 *                                                                               *
 *  If an interrupt was triggered in parallell with the enable operation, then   *
 *  the operation will fail (SCI_ERR_COND_INT_RACE_PROBLEM), and the client can  *
 *  not rely on another trigger operation will lead to handler invocation.       *
 *  Hence, any state checking normally associated with handling the              *
 *  corresponding interrupt should take place before attempting to enable        *
 *  again.                                                                       *
 *                                                                               *              
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_COND_INT_RACE_PROBLEM - The enable operation failed because an       *
 *                                  incomming trigger operation happened         *
 *                                  concurrently.                                *
 *                                                                               *
 *********************************************************************************/
#define SCIEnableConditionalInterrupt            _SISCI_EXPANDE_FUNCTION_NAME(SCIEnableConditionalInterrupt)
DLL void SCIEnableConditionalInterrupt(
                            sci_local_interrupt_t interrupt,
                            unsigned int          flags,
                            sci_error_t           *error);




/*********************************************************************************
 *                                                                               *
 *  S C I   D I S A B L E   C O N D I T I O N A L   I N T E R R U P T            *
 *                                                                               *
 *                                                                               *  
 *  Prevent subsequent "conditional interrupt"trigger operations for             *
 *  the specified interupt flag from causing HW interrupt and handler            *
 *  invocations.                                                                 * 
 *                                                                               *
 *                                                                               *
 *  Default semantics:                                                           *
 *                                                                               *
 *  If successful, no subsequent HW interrupts will take place, but handler      *
 *  invocations that have already been scheduled may still take place.           *
 *                                                                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *                                                                               *
 *********************************************************************************/
#define SCIDisableConditionalInterrupt           _SISCI_EXPANDE_FUNCTION_NAME(SCIDisableConditionalInterrupt)
DLL void SCIDisableConditionalInterrupt(
                             sci_local_interrupt_t interrupt,
                             unsigned int          flags,
                             sci_error_t           *error);



/*********************************************************************************
 *                                                                               *
 *  S C I   G E T   C O N D I T I O N A L   I N T E R R U P T   C O U N T E R    *
 *                                                                               *
 *                                                                               *
 *  Returns a value that indicates the number of times this flag has             *
 *  been trigged since the last time it was enabled or disabled.                 *
 *  Calling the SCIEnableConditionalInterrupt / SCIDisableConditionalInterrupt   *
 *  functions will reset the counter value.                                      *
 *                                                                               *
 *  Default semantics:                                                           *
 *                                                                               *
 *  If successful, the current trig count is returned in the                     *
 *  interruptTrigCounter parameter.                                              *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OVERFLOW  - The number of trig operations have exceeded the range    *
 *                   that can be counted.                                        *
 *********************************************************************************/
#define SCIGetConditionalInterruptTrigCounter    _SISCI_EXPANDE_FUNCTION_NAME(SCIGetConditionalInterruptTrigCounter)
DLL void SCIGetConditionalInterruptTrigCounter(
                            sci_local_interrupt_t interrupt,
                            unsigned int          *interruptTrigCounter,
                            unsigned int          flags,
                            sci_error_t           *error);




/*********************************************************************************
 *                                                                               *
 *  S C I   T R A N S F E R  B L O C K                                           *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the size and offset is larger     *
 *                                  than the corresponding map size.             *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as required    *
 *                                  by the implementation.                       *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as required  *
 *                                  by the implementation.                       *
 *  SCI_ERR_TRANSFER_FAILED       - The data transfer failed.                    *
 *                                                                               *
 *********************************************************************************/
#define SCITransferBlock                         _SISCI_EXPANDE_FUNCTION_NAME(SCITransferBlock)
DLL void SCITransferBlock(sci_map_t    sourceMap,
                          unsigned int sourceOffset,
                          sci_map_t    destinationMap,
                          unsigned int destinationOffset,
                          unsigned int size,
                          unsigned int flags,
                          sci_error_t  *error);




/*********************************************************************************
 *                                                                               *
 * S C I   T R A N S F E R  B L O C K   A S Y N C                                *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCI_FLAG_BLOCK_READ                                                          *
 *  SCI_FLAG_USE_CALLBACK                                                        *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE      - The sum of the size and offset is larger than    *
 *                              the corresponding map size.                      *
 *  SCI_ERR_SIZE_ALIGNMENT    - Size is not correctly aligned as required by     *
 *                              the implementation.                              *
 *  SCI_ERR_OFFSET_ALIGNMENT  - Offset is not correctly aligned as required      *
 *                              by the implementation.                           *
 *  SCI_ERR_TRANSFER_FAILED   - The data transfer failed.                        *
 *                                                                               *
 *********************************************************************************/
#define SCITransferBlockAsync                    _SISCI_EXPANDE_FUNCTION_NAME(SCITransferBlockAsync)
DLL void SCITransferBlockAsync(sci_map_t               sourceMap,
                               unsigned int            sourceOffset,
                               sci_map_t               destinationMap,
                               unsigned int            destinationOffset,
                               unsigned int            size,
                               sci_block_transfer_t    *block,
                               sci_cb_block_transfer_t callback,
                               void                    *callbackArg,
                               unsigned int            flags,
                               sci_error_t             *error);




/*********************************************************************************
 *                                                                               *
 * S C I   W A I T   F O R   B L O C K   T R A N S F E R                         *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation                            *
 *  SCI_ERR_TIMEOUT               - The function timed out after specified       *
 *                                  timeout value.                               *
 *                                                                               *
 *********************************************************************************/
#define SCIWaitForBlockTransfer                  _SISCI_EXPANDE_FUNCTION_NAME(SCIWaitForBlockTransfer)
DLL void SCIWaitForBlockTransfer(sci_block_transfer_t block,
                                 unsigned int         timeout,
                                 unsigned int         flags,
                                 sci_error_t          *error);



/*********************************************************************************
 *                                                                               * 
 * S C I   A B O R T   B L O C K   T R A N S F E R                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_ILLEGAL_OPERATION     - Illegal operation                            *
 *                                                                               *
 *********************************************************************************/
#define SCIAbortBlockTransfer                    _SISCI_EXPANDE_FUNCTION_NAME(SCIAbortBlockTransfer)
DLL void SCIAbortBlockTransfer(sci_block_transfer_t block,
                               unsigned int         flags,
                               sci_error_t          *error);




/*********************************************************************************
 *                                                                               *
 * S C I   M E M   C P Y                                                         *
 *                                                                               *
 *  Flags:                                                                       *
 *    SCI_FLAG_BLOCK_READ                                                        *
 *    SCI_FLAG_ERROR_CHECK                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the size and offset is larger     *
 *                                  than the corresponding map size.             *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as required    *
 *                                  by the implementation.                       *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as required  *
 *                                  by the implementation.                       *
 *  SCI_ERR_TRANSFER_FAILED       - The data transfer failed.                    *
 *                                                                               *
 *********************************************************************************/

#define SCIMemCpy                                _SISCI_EXPANDE_FUNCTION_NAME(SCIMemCpy)
DLL void SCIMemCpy(sci_sequence_t sequence,
                   void           *memAddr,
                   sci_map_t      remoteMap,
                   unsigned int   remoteOffset,
                   unsigned int   size,
                   unsigned int   flags,
                   sci_error_t    *error);



/*********************************************************************************
 *                                                                               *
 * S C I   M E M   C O P Y                                                       *
 *                                                                               *
 *  Flags:                                                                       *
 *    SCI_FLAG_BLOCK_READ                                                        *
 *    SCI_FLAG_ERROR_CHECK                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_OUT_OF_RANGE          - The sum of the size and offset is larger     *
 *                                  than the corresponding map size.             *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as required    *
 *                                  by the implementation.                       *
 *  SCI_ERR_OFFSET_ALIGNMENT      - Offset is not correctly aligned as required  *
 *                                  by the implementation.                       *
 *  SCI_ERR_TRANSFER_FAILED       - The data transfer failed.                    *
 *                                                                               *
 *********************************************************************************/


#define SCIMemCopy                               _SISCI_EXPANDE_FUNCTION_NAME(SCIMemCopy)
DLL void SCIMemCopy(void         *memAddr,
                    sci_map_t    remoteMap,
                    unsigned int remoteOffset,
                    unsigned int size,
                    unsigned int flags,
                    sci_error_t  *error);



/*********************************************************************************
 *                                                                               * 
 * S C I   R E G I S T E R   S E G M E N T   M E M O R Y                         *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_SIZE_ALIGNMENT   - Size is not correctly aligned as required by      *
 *                             the implementation.                               *
 *  SCI_ERR_ILLEGAL_ADDRESS  - Illegal address.                                  *
 *  SCI_ERR_OUT_OF_RANGE     - Size is larger than the maximum size for the      *
 *                             local segment.                                    *
 *                                                                               *
 *********************************************************************************/
#define SCIRegisterSegmentMemory                 _SISCI_EXPANDE_FUNCTION_NAME(SCIRegisterSegmentMemory)
DLL void SCIRegisterSegmentMemory(void                *address,
                                  unsigned int        size,
                                  sci_local_segment_t segment,
                                  unsigned int        flags,
                                  sci_error_t         *error);





/*********************************************************************************
 *                                                                               *
 * S C I   C O N N E C T   S C I   S P A C E                                     *
 *                                                                               *
 *  SISCI Priveleged function                                                    *
 *                                                                               *
 *                                                                               *
 *  Flags                                                                        *
 *   None.                                                                       *
 *                                                                               *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  SCI_ERR_SIZE_ALIGNMENT        - Size is not correctly aligned as required    *
 *                                  by the implementation.                       *
 *  SCI_ERR_CONNECTION_REFUSED    - Connection attempt refused by remote node.   *
 *                                                                               *
 *********************************************************************************/
#define SCIConnectSCISpace                       _SISCI_EXPANDE_FUNCTION_NAME(SCIConnectSCISpace)
DLL void SCIConnectSCISpace(sci_desc_t           sd,
                            unsigned int         localAdapterNo,
                            sci_remote_segment_t *segment,
                            sci_address_t        address,
                            unsigned int         size,
                            unsigned int         flags,
                            sci_error_t          *error);



/*
 * =====================================================================================
 *
 *    S C I   A T T A C H   L O C A L   S E G M E N T
 *   Description:
 *
 *     SCIAttachLocalSegment() permits an application to "attach" to an already existing
 *     local segment, implying that two or more application want
 *     share the same local segment. The prerequest, is  that the
 *     application which originally created the segment ("owner") has
 *     preformed a SCIShareSegment() in order to mark the segment
 *     "shareable".
 *
 *
 *   Flags:
 *  
 *     SCI_FLAG_USE_CALLBACK  - The callback function will be invoked for events 
 *                              on this segment.     
 *
 *
 *   Specific error codes for this function:  
 * 
 *     SCI_ERR_ACCESS          - No such shared segment 
 *     SCI_ERR_NO_SUCH_SEGMENT - No such segment 
 *     Note: Current implenentation will return SCI_ERR_ACCESS for both cases. This will
 *           change from next release. Application should handle both cases.   
 *
 * =====================================================================================
 */
#define SCIAttachLocalSegment                    _SISCI_EXPANDE_FUNCTION_NAME(SCIAttachLocalSegment)

DLL void
SCIAttachLocalSegment(sci_desc_t              sd,
                      sci_local_segment_t    *segment,
                      unsigned int            segmentId,
                      unsigned int           *size,
                      sci_cb_local_segment_t callback,
                      void                   *callbackArg,
                      unsigned int           flags,
                      sci_error_t            *error);
/*
 * =====================================================================================
 *
 *    S C I   S H A R E   S E G M E N T
 * 
 *   Description:
 *
 *     SCIShareSegment() permits other application to "attach" to an already existing
 *     local segment, implying that two or more application want
 *     share the same local segment. The prerequest, is  that the
 *     application which originally created the segment ("owner") has
 *     preformed a SCIShareSegment() in order to mark the segment
 *     "shareable".
 *
 *
 *   Flags:
 *     none
 *
 *   Specific error codes for this function:  
 *
 *
 *
 * =====================================================================================
 */
#define SCIShareSegment                          _SISCI_EXPANDE_FUNCTION_NAME(SCIShareSegment)

DLL void
SCIShareSegment(sci_local_segment_t segment,
                unsigned int flags,
                sci_error_t *error);


/*********************************************************************************
 *                                                                               *
 * S C I   F L U S H                                                             *
 *                                                                               *
 *  This function will flush the CPU buffers and the PSB buffers.                *
 *                                                                               *
 *  Flags                                                                        *
 *   SCI_FLAG_FLUSH_CPU_BUFFERS_ONLY :                                           *
 *                                    Only flush CPU buffers ( Write combining   *
 *                                    etc buffers).                              *
 *                                                                               *
 *********************************************************************************/

#define SCIFlush                   _SISCI_EXPANDE_FUNCTION_NAME(SCIFlush)
DLL void SCIFlush(sci_sequence_t sequence,
                         unsigned int   flags);

#if defined(CPLUSPLUS) || defined(__cplusplus)
}
#endif


#endif











