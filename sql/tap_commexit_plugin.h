#ifndef __TAP_COMMEXIT_PLUGIN_H__
#define __TAP_COMMEXIT_PLUGIN_H__

#include "tap_commexit.h"

namespace tap_commexit {
namespace config {
extern char *library_path;
}

void load_plugin();
bool plugin_loaded();

void* allocate_context();
void free_context(void* context);
TAPCOMMEXIT_RC init_context(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo
		);
TAPCOMMEXIT_RC init_shmem (
		char       *errormsg,
		uint32_t   *errormsglen,
		uint32_t   *errormsgavail,
		const char *db_str,
		int         force_init
		);
void shutdown_shmem(
		char     *logmsg,
		uint32_t *logmsglen,
		uint32_t *logmsgavail
		);
void init(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail
		);
TAPCOMMEXIT_RC
send_server_data(
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
TAPCOMMEXIT_RC
send_client_data(
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
void
send_open(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );
void
send_close(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );
void *
get_context_opaque(
		const void            *_pContext
		);
void
set_context_opaque(
		void            *_pContext,
		void            *opaque
		);
}
#endif
