/* $Id: scilib.h,v 1.1 2002/12/13 12:17:23 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 2002                                                          * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 *******************************************************************************/


#if defined(_REENTRANT)
#define _SCIL_EXPANDE_FUNCTION_NAME(name)  _SCIL_PUBLIC_FUNC_MT_ ## name
#define _SCIL_EXPANDE_VARIABLE_NAME(name)  _SCIL_PUBLIC_VAR_MT_ ## name
#else
#define _SCIL_EXPANDE_FUNCTION_NAME(name)  _SCIL_PUBLIC_FUNC_ST_ ## name
#define _SCIL_EXPANDE_VARIABLE_NAME(name)  _SCIL_PUBLIC_VAR_ST_ ## name
#endif
#define _SCIL_EXPANDE_CONSTANT_NAME(name)  _SCIL_PUBLIC_CONST_ ## name

#include "sisci_api.h"

#if defined(CPLUSPLUS) || defined(__cplusplus)
extern "C" {
#endif


/* 
 * SISCI segment id pollution:
 * ===========================
 * The SISCI library uses regular SISCI segmens internally. 
 * The MSG_QUEUE_LIB_IDENTIFIER_MASK is a mask which is used by the SISCI 
 * library to identify internal SISCI segments ids, from segments used directly 
 * by the user. 
 * 
 * Future versions of the library may have its own namespace.
 * 
 */

#define MSG_QUEUE_LIB_IDENTIFIER_MASK 0x10000000 


/*********************************************************************************/
/*                         FLAG VALUES                                           */
/*********************************************************************************/

#define SCIL_FLAG_ERROR_CHECK_DATA              _SCIL_EXPANDE_CONSTANT_NAME(SCIL_FLAG_ERROR_CHECK_DATA)
extern const unsigned int SCIL_FLAG_ERROR_CHECK_DATA;

#define SCIL_FLAG_ERROR_CHECK_PROT              _SCIL_EXPANDE_CONSTANT_NAME(SCIL_FLAG_ERROR_CHECK_PROT)
extern const unsigned int SCIL_FLAG_ERROR_CHECK_PROT;

#define SCIL_FLAG_FULL_ERROR_CHECK              _SCIL_EXPANDE_CONSTANT_NAME(SCIL_FLAG_FULL_ERROR_CHECK)
extern const unsigned int SCIL_FLAG_FULL_ERROR_CHECK;





typedef struct sci_msq_queue *sci_msq_queue_t;


/*********************************************************************************
 *                                                                               *
 * S C I L C r e a t e M s g Q u e u e                                           *
 *                                                                               *
 * Parameters:                                                                   *
 *                                                                               *
 * Creates a message queue. The message queue identifier object will be allocated*
 * if the sci_msq_queue_t * msg pointer is NULL. The function will create a      *
 * remote connection. If this connection times out, the function shoud be        *
 * repeated until connection is established. SCILRemoveMsgQueue() must be called *
 * to remove the connection and deallocate the message queue identifier.         *
 *                                                                               *
 * sci_msq_queue_t       *msq : Message queue identifier                         *
 *                              The function must be called with a null pointer  *
 *                              the first time.                                  *
 * unsigned int localAdapterNo: Local Adapter Number                             *
 * unsigned int remoteNodeId  : Remote nodeId                                    *
 * unsigned int msqId		  : Message queue number                             * 
 * unsigned int maxMsgCount   : The maximum count of messages in queue           *
 * unsigned int maxMsgSize,   : The maximum size of each messages in queue       * 
 * unsigned int timeout       : Time to wait for successful connection           *
 * unsigned int flags         : Flags.                                           *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  None. Normal SISIC error codes.                                              *
 *                                                                               *
 *********************************************************************************/
#define SCILCreateMsgQueue           _SCIL_EXPANDE_FUNCTION_NAME(SCILCreateMsgQueue)
DLL sci_error_t SCILCreateMsgQueue(sci_msq_queue_t 	   *msq,		    
                                   unsigned int 	    localAdapterNo, 
                                   unsigned int 	    remoteNodeId,   
                                   unsigned int 	    msqId,			
                                   unsigned int         maxMsgCount,    
                                   unsigned int         maxMsgSize,  
                                   unsigned int         timeout,
                                   unsigned int 	    flags);


/*********************************************************************************
 *                                                                               *
 * S C I L R e c e i v e M s g                                                   *
 *                                                                               *
 *                                                                               *
 * Receives a message from the queue.                                            *
 *                                                                               *
 * Paremeters                                                                    *
 *                                                                               *
 *  sci_msq_queue_t   msq : message queue identifier                             *   
 *  void             *msg : Location to store received data                      * 
 *  unsigned int     size : Size of message to read                              *
 *  unsigned int *sizeLeft: Bytes left in buffer, after current receive. This is *
 *                          just a hint. There may be more.                      *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCIL_FLAG_ERROR_CHECK_PROT: The internal buffer management is done using full*
 *                              error checking.  
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *   SCI_ERR_EWOULD_BLOCK     : There is not enough data in the message buffer   *
 *                              to read the specified number of bytes.           *
 *                                    .                                          *
 *   SCI_ERR_NOT_CONNECTED    : The connection is not established.               *
 *                                                                               *
 *********************************************************************************/
#define SCILReceiveMsg           _SCIL_EXPANDE_FUNCTION_NAME(SCILReceiveMsg)
DLL sci_error_t SCILReceiveMsg(		
                           sci_msq_queue_t  msq,  
                           void	           *msg,  
                           unsigned int     size, 
                           unsigned int    *sizeLeft,
                           unsigned int     flags);                         



/*********************************************************************************
 *                                                                               *
 * S C I L S e n d M s g                                                         *
 *                                                                               *
 *                                                                               *
 * Sends a message to the queue.                                                 *
 *                                                                               *
 * Paremeters                                                                    *
 *                                                                               *
 *  sci_msq_queue_t   msq : Message queue identifier                             *   
 *  void             *msg : Send data                                            * 
 *  unsigned int     size : Size of message to send                              *
 *  unsigned int *sizeFree: Bytes free in buffer, after current send. This is    *
 *                          just a hint. There may be more.                      *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  SCIL_FLAG_ERROR_CHECK_DATA: The data is transmitted using full error checking*                        
 *  SCIL_FLAG_ERROR_CHECK_PROT: The internal buffer management is done using full*
 *                              error checking.                                  *
 *  SCIL_FLSG_FULL_ERROR_CHECK: This flag is an combination of both above.       *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *   SCI_ERR_EWOULD_BLOCK     : There is not enough data in the message buffer   *
 *                              to send the specified number of bytes.           *
 *                                    .                                          *
 *   SCI_ERR_NOT_CONNECTED    : The connection is not established.               *
 *                                                                               *
 *********************************************************************************/
#define SCILSendMsg           _SCIL_EXPANDE_FUNCTION_NAME(SCILSendMsg)
DLL sci_error_t SCILSendMsg(		
                        sci_msq_queue_t     msq,    
                        void               *msg,    
                        unsigned int        size,   
                        unsigned int       *sizeFree,
                        unsigned int        flags);                              


/*********************************************************************************
 *                                                                               *
 * S C I L R e m o v e M s g Q u e ue                                            *
 *                                                                               *
 *                                                                               *
 * Removes a message queue.                                                      *
 *                                                                               *
 *  sci_msq_queue_t   msq : Message queue identifier                             *   
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *********************************************************************************/
#define SCILRemoveMsgQueue           _SCIL_EXPANDE_FUNCTION_NAME(SCILRemoveMsgQueue)
DLL sci_error_t SCILRemoveMsgQueue(	
                               sci_msq_queue_t *msq,           
                               unsigned int     flags);           



/*********************************************************************************
 *                                                                               *
 * S C I L I n i t                                                               *
 *                                                                               *
 *                                                                               *
 * Initializes the SCI library. This function must be called before any other    *
 * function in the library.                                                      *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *	None                                                                     *
 *                                    .                                          *
 *********************************************************************************/
#define SCILInit           _SCIL_EXPANDE_FUNCTION_NAME(SCILInit)
DLL sci_error_t SCILInit(unsigned int flags);



/*********************************************************************************
 *                                                                               *
 * S C I L D e s t r o y                                                         *
 *                                                                               *
 *                                                                               *
 * Removes internal resources allocated by the SCI Library. No other library     *
 * function should be called after this function.                                *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *	None                                                                     *
 *                                                                               *
 *********************************************************************************/
#define SCILDestroy           _SCIL_EXPANDE_FUNCTION_NAME(SCILDestroy)
DLL sci_error_t SCILDestroy(unsigned int flags);



/*********************************************************************************
 *                                                                               *
 * S C I L C o n n e c t M s g Q u e u e                                         *
 *                                                                               *
 *                                                                               *
 * Makes a connection to a remote message queue. This must be done before        * 
 * SCILSendMsg() is called.                                                      *
 *                                                                               *
 * Parameters:                                                                   *
 *                                                                               *
 * sci_msq_queue_t       *msq : Message queue identifier                         *
 * unsigned int localAdapterNo: Local Adapter Number                             *
 * unsigned int remoteNodeId  : Remote nodeId                                    *
 * unsigned int msqId         : Message queue number                             * 
 * unsigned int maxMsgCount   : The maximum count of messages in queue           *
 * unsigned int maxMsgSize,   : The maximum size of each messages in queue       *
 * unsigned int timeout       : Time to wait for successful connection           *
 * unsigned int flags         : Flags.                                           *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *  None. Normal SISIC error codes.                                              *
 *                                                                               *
 *********************************************************************************/
#define SCILConnectMsgQueue           _SCIL_EXPANDE_FUNCTION_NAME(SCILConnectMsgQueue)
DLL sci_error_t SCILConnectMsgQueue(sci_msq_queue_t     *msq,              
                                    unsigned int         localAdapterNo,
                                    unsigned int         remoteNodeId,     
                                    unsigned int         rmsgId,           
                                    unsigned int         maxMsgCount,      
                                    unsigned int         maxMsgSize,       
                                    unsigned int         timeout,          
                                    unsigned int         flags);             
  
  
  
  
/*********************************************************************************
 *                                                                               *
 * S C I L D i s c o n n e c t M s g Q u e u e                                   *
 *                                                                               *
 *                                                                               *
 * Disconnects from a remote message queue.                                      *
 *                                                                               *
 * Parameters:                                                                   *
 *                                                                               *
 * sci_msq_queue_t       *msq : Message queue identifier                         *
 *                                                                               *
 *  Flags                                                                        *
 *                                                                               *
 *  None                                                                         *
 *                                                                               *
 *  Specific error codes for this function:                                      *
 *                                                                               *
 *	None                                                                     *
 *                                                                               *
 *********************************************************************************/
#define SCILDisconnectMsgQueue           _SCIL_EXPANDE_FUNCTION_NAME(SCILDisconnectMsgQueue)
DLL sci_error_t SCILDisconnectMsgQueue(sci_msq_queue_t *msq,  
				   unsigned int     flags);             



#if defined(CPLUSPLUS) || defined(__cplusplus)
}
#endif










