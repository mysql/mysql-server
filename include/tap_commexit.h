/***************************************************************************
 * Licensed Materials - Property of IBM
 *
 * Restricted Materials of IBM
 *
 * (C) COPYRIGHT International Business Machines Corp. 2016,2017
 * All Rights Reserved.
 *
 * US Government Users Restricted Rights -
 * Use, duplication or disclosure restricted by
 * GSA ADP Schedule Contract with IBM Corp
 *
 */

#ifndef _TAPCOMMEXIT_H
#define _TAPCOMMEXIT_H

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>

#include <netinet/in.h>
// #include <ipv6_fix.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <tap_commexit_defines.h>

#define TAPCOMMEXIT_LOG_NONE      0
#define TAPCOMMEXIT_LOG_CRITICAL  1
#define TAPCOMMEXIT_LOG_ERROR     2
#define TAPCOMMEXIT_LOG_WARNING   3
#define TAPCOMMEXIT_LOG_INFO      4

#define TAPCOMMEXIT_LOG_ALWAYS    1

#define TAPCOMMEXIT_NONE  0
#define TAPCOMMEXIT_TRACE 1
#define TAPCOMMEXIT_DEBUG 2

// Return values for TAPCOMMEXIT_RC
#define TAPCOMMEXIT_SUCCESS 0
#define TAPCOMMEXIT_FAIL    1
#define TAPCOMMEXIT_SCRUB   2
#define TAPCOMMEXIT_REWRITE 3
#define TAPCOMMEXIT_KILL    4
#define TAPCOMMEXIT_PENDING 5
#define TAPCOMMEXIT_MORE    6

#define TAPCOMMEXIT_FEATURE_NO_SCRUB                   0x01 // do not allow scrub at all
#define TAPCOMMEXIT_FEATURE_SCRUB_ALLOC_BUF            0x02 // does not imply scrub is supported, requires allocation
#define TAPCOMMEXIT_FEATURE_NO_QRW                     0x04 // do not allow qrw at all (qrw requires allocation)
#define TAPCOMMEXIT_FEATURE_NO_FW                      0x08 // do not allow firewall at all
#define TAPCOMMEXIT_FEATURE_SOCKET_PORTS_IN_HOST_ORDER 0x10 // do not need to use ntohs for socket ports
#define TAPCOMMEXIT_FEATURE_NON_BLOCKING_VERDICTS      0x20

// Protocol type definitions
#define TAPCOMMEXIT_PROTOCOL_UNKNOWN 0
#define TAPCOMMEXIT_PROTOCOL_TCPIP4  1
#define TAPCOMMEXIT_PROTOCOL_TCPIP6  2
#define TAPCOMMEXIT_PROTOCOL_LOCAL   3

#define TAPCOMMEXIT_SESSION_UNENCRYPTED 0
#define TAPCOMMEXIT_SESSION_ENCRYPTED   1

#define TAPCOMMEXIT_LOCAL_TYPE_SHMEM 1
#define TAPCOMMEXIT_LOCAL_TYPE_OTHER 2

typedef int TAPCOMMEXIT_RC;

#define TAPCOMMEXIT_MAX_FILTER_COUNT 20
typedef struct tap_commexit_filter_struct {
	int    filter_count;
	struct {
		struct sockaddr_storage ip;
		struct sockaddr_storage netmask;
	} filters[TAPCOMMEXIT_MAX_FILTER_COUNT];
} tap_commexit_filter_t;

typedef struct tap_commexit_config_ie_struct {
	int                   port_range_start;
	int                   port_range_end;
	int                   db_type;
	tap_commexit_filter_t include_filter;
	tap_commexit_filter_t exclude_filter;
} tap_commexit_config_ie_t;

// Item allocated will have ie_count elements of ies
typedef struct tap_commexit_config_struct {
	int                      ie_count;
	tap_commexit_config_ie_t ies[1];
} tap_commexit_config_t;

tap_commexit_config_t *tap_commexit_get_config(void);
void tap_commexit_release_config(tap_commexit_config_t *config);

typedef enum logging_types {
		EXTERNAL,	// We'll be provided an external logging function of type tapcommexitLogMessage
		STRING		// We'll pass the log message back in buffers provided
} logging_t;

// Prototype for an external logging function we can register to the interface
typedef TAPCOMMEXIT_RC (tapcommexitLogMessage) (
		int         reserved,
		void       *pContext,
		const void *pCommInfo, 
		int         when,
		int32_t     level,
		char       *logmsg,
		int32_t     logmsglen
		);

typedef enum {
    LOCAL_SOCKET,
    REMOTE_SOCKET
} socket_type;

struct tap_commexit_db_support_fns {
	// Returns the PID for a given connection
	int              (*get_pid)(const void *pContext, const void *pCommInfo);
	// Returns the protocol type for a given connection
	int              (*get_protocol)(const void *pContext, const void *pCommInfo);
	// Returns the socket specified by 'which' for a given connection
	struct sockaddr_storage *(*get_sockaddr)(const void *pContext, const void *pCommInfo, socket_type which);
	// Returns true if debugging is enabled specifically for this connection
	int              (*get_session_debug)(const void *pContext, const void *pCommInfo);
	// Returns true if tracing is enabled specifically for this connection
	int              (*get_session_trace)(const void *pContext, const void *pCommInfo);
	// Used to allocate memory which can be freed by the DB (optional)
	void            *(*db_malloc)(size_t size);
	// Used to free memory internally if it wasn't needed, or is replaced, by the above function (optional)
	void             (*db_free)(void *p);
	// Returns a special marker to differentiate local connection types (optional)
	int              (*get_local_conn_type)(const void *pContext, const void *pCommInfo);
	// Unique address which can be used to differentiate sessions (optional)
	void             (*get_conn_unique_addr)(const void *pContext, const void *pCommInfo, uint32_t *unique_addr);
	// Notify the DB that the config has changed
	void             (*notify_new_config)(void);
	// Check is this session is encrypted
	int              (*is_encrypted)(const void *pContext, const void *pCommInfo);
	// Get the DB thread identifier for this connection (optional)
	uint32_t         (*get_db_thread_id)(const void *pContext, const void *pCommInfo);
	// Get the DB identifier for this connection (optional)
	uint64_t         (*get_db_conn_id)(const void *pContext, const void *pCommInfo);
};

// Register an external logging function to the interface
void
tap_commexit_set_external_log_function(int (*fn)(
		int         reserved,
		void       *pContext,
		const void *pCommInfo, 
		int         when,
		int32_t     level,
		char       *logmsg,
		int32_t     logmsglen
		));

// Returns true if we should print to the debug log
int
tap_commexit_should_log(void * pContext, const void *pCommInfo, int level);
// If inside an external logging function, must call the internal call
int
tap_commexit_should_log_internal(int reserved, void * pContext, const void *pCommInfo, int level);

// Send the UID chain request to STAP for this session
void
tap_commexit_request_uid_chain(
        char                 *logmsg,
        uint32_t             *logmsglen,
        uint32_t             *logmsgavail,
        void                 *pContext,
        const void           *pCommInfo
        );

// Send the username to STAP for this session
void
tap_commexit_send_username_packet(
        char                 *logmsg,
        uint32_t             *logmsglen,
        uint32_t             *logmsgavail,
        void                 *pContext,
        const void           *pCommInfo,
        void                 *packet,
        uint32_t              packet_len
        );

// Returns true if we can send data to STAP
int tap_commexit_should_send_data(void *_pContext);

// Send S2C traffic to STAP
TAPCOMMEXIT_RC
tap_commexit_send_server_data(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo,
		int64_t         *pReservedFlags,
		const char      *pBuffer,
		int              buffer_len,
		char           **pNewBuffer,
		int             *new_buffer_len
		);

// Send C2S traffic to STAP
TAPCOMMEXIT_RC
tap_commexit_send_client_data(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo,
		int64_t         *pReservedFlags,
		const char      *pBuffer,
		int              buffer_len,
		char           **pNewBuffer,
		int             *new_buffer_len
		);

// Returns true if no room in buffer to send data
int
tap_commexit_full(void);

// Initialize the shmem channel to STAP
TAPCOMMEXIT_RC
tap_commexit_init_shmem (
		char       *errormsg,
		uint32_t   *errormsglen,
		uint32_t   *errormsgavail,
		const char *db_str,
		int         force_init
		);

// Shutdown the shmem channel to STAP
void
tap_commexit_shutdown_shmem(
		char     *logmsg,
		uint32_t *logmsglen,
		uint32_t *logmsgavail
		);

// Allocate a context for a new session
void *
tap_commexit_allocate_context(void);

// Free a context when the session closes
void
tap_commexit_free_context(void *ptr);

// Initialize the context
TAPCOMMEXIT_RC
tap_commexit_init_context(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo,
		uint64_t         feature_flags,
		const struct tap_commexit_db_support_fns *support_fns
		);

// Get a previously saved opaque pointer stored in the context
void *
tap_commexit_get_context_opaque(
		const void            *_pContext
		);

// Store an opaque pointer to the context
void
tap_commexit_set_context_opaque(
		void            *_pContext,
		void            *opaque
		);

// Send OPEN marker for session to STAP
void
tap_commexit_send_open(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );

// Send CLOSE marker for session to STAP
void
tap_commexit_send_close(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );

// Private implementation specific initialization
void tap_commexit_init(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail
		);

// Get the pointer for the connection info which is stored in the context (stored during init)
const void *
tap_commexit_get_conninfo(
		void *context);
void
tap_commexit_log_event(
		const char* exit_name,
		int id,
		int level,
		size_t msg_len,
		const char* message
		);
uint32_t tap_commexit_has_verdict(void *context);
uint32_t tap_commexit_get_verdict(void *context);
int tap_commexit_verdict_request_expired(void *context);
TAPCOMMEXIT_RC tap_commexit_handle_rewrite_message(
		char                 *logmsg,       // Head of log message string
		uint32_t             *logmsglen,    // Current number of bytes in log message string
		uint32_t             *logmsgavail,  // Amount of space left in message string
		void                 *pContext,
		const void           *pCommInfo,
		const char           *buffer,
		int                   size,
		char                **newBuffer,
		int                  *newSize
		);
#ifdef __cplusplus
}
#endif

#endif

