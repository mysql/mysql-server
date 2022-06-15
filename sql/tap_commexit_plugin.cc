#include "sql/tap_commexit_plugin.h"
#include "sql/sql_class.h"
#include "sql/protocol_classic.h"
#include "violite.h"

#ifndef WIN32
#include <dlfcn.h>
#endif

#include <iostream>

namespace tap_commexit {
namespace config {
char *library_path = nullptr;
}

void* plugin_handle = nullptr;
const uint64_t feature_flags = (TAPCOMMEXIT_FEATURE_NO_QRW | TAPCOMMEXIT_FEATURE_SCRUB_ALLOC_BUF);
struct tap_commexit_db_support_fns db_support_functions = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};

namespace func_ptrs {
namespace types {
typedef void* (*allocate_context_t)(void);
typedef void (*free_context_t)(void* context);
typedef TAPCOMMEXIT_RC (*init_context_t)(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo,
		uint64_t         feature_flags,
		const struct tap_commexit_db_support_fns *support_fns
		);
typedef TAPCOMMEXIT_RC (*init_shmem_t)(
		char       *errormsg,
		uint32_t   *errormsglen,
		uint32_t   *errormsgavail,
		const char *db_str,
		int         force_init
		);
typedef void (*shutdown_shmem_t)(
		char     *logmsg,
		uint32_t *logmsglen,
		uint32_t *logmsgavail
		);
typedef void (*init_t)(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail
		);
typedef TAPCOMMEXIT_RC (*send_server_data_t)(
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
typedef TAPCOMMEXIT_RC (*send_client_data_t)(
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
typedef void (*send_open_t)(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );
typedef void (*send_close_t)(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        );
typedef void *(*get_context_opaque_t)(
		const void            *_pContext
		);
typedef void (*set_context_opaque_t)(
		void            *_pContext,
		void            *opaque
		);
} // namespace types
types::allocate_context_t allocate_context = nullptr;
types::free_context_t free_context = nullptr;
types::init_context_t init_context = nullptr;
types::init_shmem_t init_shmem = nullptr;
types::shutdown_shmem_t shutdown_shmem = nullptr;
types::init_t init = nullptr;
types::send_server_data_t send_server_data = nullptr;
types::send_client_data_t send_client_data = nullptr;
types::send_open_t send_open = nullptr;
types::send_close_t send_close = nullptr;
types::get_context_opaque_t get_context_opaque = nullptr;
types::set_context_opaque_t set_context_opaque = nullptr;
} //namespace func_ptrs

namespace support_funcs {
void* db_malloc(size_t size) {
	return malloc(size);
}

void db_free(void* ptr) {
	free(ptr);
}

int get_pid(const void* context, const void* comm_info) {
	(void)context; (void)comm_info;
	return getpid();
}

int get_protocol(const void* context, const void* comm_info) {
	const THD* thd = reinterpret_cast<const THD*>(comm_info);
	(void)context;

	if (thd == nullptr ||
			thd->get_protocol_classic() == nullptr ||
			thd->get_protocol_classic()->get_vio() == nullptr) {
		return TAPCOMMEXIT_PROTOCOL_UNKNOWN;
	}

	int conn_fd = vio_fd(const_cast<Vio *>(thd->get_protocol_classic()->get_vio()));
	struct sockaddr_storage local_sock = {0, 0, 0};
	socklen_t local_sock_len = sizeof(local_sock);
	getsockname(conn_fd, reinterpret_cast<struct sockaddr *>(&local_sock), &local_sock_len);
	switch(local_sock.ss_family) {
		case AF_INET:
			return TAPCOMMEXIT_PROTOCOL_TCPIP4;
		case AF_INET6:
			return TAPCOMMEXIT_PROTOCOL_TCPIP6;
		case AF_UNIX:
			return TAPCOMMEXIT_PROTOCOL_LOCAL;
		default:
			return TAPCOMMEXIT_PROTOCOL_UNKNOWN;
	}
	return TAPCOMMEXIT_PROTOCOL_UNKNOWN;
}

struct sockaddr_storage*
get_sockaddr(
		const void* context,
		const void* comm_info,
		socket_type which) {
	const THD* thd = reinterpret_cast<const THD*>(comm_info);
	int protocol = get_protocol(context, comm_info);

	if (protocol == TAPCOMMEXIT_PROTOCOL_LOCAL ||
			protocol == TAPCOMMEXIT_PROTOCOL_TCPIP4 ||
			protocol == TAPCOMMEXIT_PROTOCOL_TCPIP6) {
		if (which == LOCAL_SOCKET) {
			int conn_fd = vio_fd(const_cast<Vio *>(thd->get_protocol_classic()->get_vio()));
			struct sockaddr_storage local_sock = {0, 0, 0};
			socklen_t local_sock_len = sizeof(local_sock);
			getsockname(conn_fd, reinterpret_cast<struct sockaddr *>(&local_sock), &local_sock_len);
			memcpy(const_cast<void *>(reinterpret_cast<const void *>(&thd->get_protocol_classic()->get_vio()->local)), &local_sock, sizeof(local_sock));
			return const_cast<struct sockaddr_storage*>(&thd->get_protocol_classic()->get_vio()->local);
		} else {
			int conn_fd = vio_fd(const_cast<Vio *>(thd->get_protocol_classic()->get_vio()));
			struct sockaddr_storage remote_sock = {0, 0, 0};
			socklen_t remote_sock_len = sizeof(remote_sock);
			getpeername(conn_fd, reinterpret_cast<struct sockaddr *>(&remote_sock), &remote_sock_len);
			memcpy(const_cast<void *>(reinterpret_cast<const void *>(&thd->get_protocol_classic()->get_vio()->remote)), &remote_sock, sizeof(remote_sock));
			return const_cast<struct sockaddr_storage*>(&thd->get_protocol_classic()->get_vio()->remote);
		}
	}
	return NULL;
}

int is_encrypted(const void* context, const void* comm_info) {
	const THD* thd = reinterpret_cast<const THD*>(comm_info);
	(void)context;

	if (thd == nullptr)
		return TAPCOMMEXIT_SESSION_UNENCRYPTED;

	if (thd->get_ssl() != nullptr)
		return TAPCOMMEXIT_SESSION_ENCRYPTED;

	return TAPCOMMEXIT_SESSION_UNENCRYPTED;

}

int get_session_debug(const void* context, const void* comm_info) {
	(void)context; (void)comm_info;
	return 0;
}

int get_session_trace(const void* context, const void* comm_info) {
	(void)context; (void)comm_info;
	return 0;
}

void notify_new_config() {
}
} // namespace support_funcs

void init_db_functions() {
	db_support_functions.get_pid = support_funcs::get_pid;
	db_support_functions.get_protocol = support_funcs::get_protocol;
	db_support_functions.get_sockaddr = support_funcs::get_sockaddr;
	db_support_functions.get_session_debug = support_funcs::get_session_debug;
	db_support_functions.get_session_trace = support_funcs::get_session_trace;
	db_support_functions.is_encrypted = support_funcs::is_encrypted;
	db_support_functions.notify_new_config = support_funcs::notify_new_config;
	db_support_functions.db_malloc = support_funcs::db_malloc;
	db_support_functions.db_free = support_funcs::db_free;
}

void load_plugin() {
	if (plugin_handle != nullptr ||
			config::library_path == nullptr) {
		return;
	}
	plugin_handle = dlopen(config::library_path, RTLD_NOW);
}

bool plugin_loaded() {
	return (plugin_handle != nullptr);
}

void* allocate_context() {
	if (!plugin_loaded()) {
		return nullptr;
	}
	if (func_ptrs::allocate_context == nullptr) {
		func_ptrs::allocate_context = reinterpret_cast<func_ptrs::types::allocate_context_t>(dlsym(plugin_handle, "tap_commexit_allocate_context"));
	}
	if (func_ptrs::allocate_context != nullptr) {
		return (*func_ptrs::allocate_context)();
	}
	return nullptr;
}
void free_context(void* context) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::free_context == nullptr) {
		func_ptrs::free_context = reinterpret_cast<func_ptrs::types::free_context_t>(dlsym(plugin_handle, "tap_commexit_free_context"));
	}
	if (func_ptrs::free_context != nullptr) {
		return (*func_ptrs::free_context)(context);
	}
	return;
}
TAPCOMMEXIT_RC init_context(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail,
		void            *_pContext,
		const void      *pCommInfo
		) {
	if (!plugin_loaded()) {
		return TAPCOMMEXIT_FAIL;
	}
	if (func_ptrs::init_context == nullptr) {
		func_ptrs::init_context = reinterpret_cast<func_ptrs::types::init_context_t>(dlsym(plugin_handle, "tap_commexit_init_context"));
	}
	if (func_ptrs::init_context != nullptr) {
		return (*func_ptrs::init_context)(logmsg, logmsglen, logmsgavail, _pContext, pCommInfo, feature_flags, &db_support_functions);
	}
	return TAPCOMMEXIT_FAIL;
}
TAPCOMMEXIT_RC init_shmem(
		char       *errormsg,
		uint32_t   *errormsglen,
		uint32_t   *errormsgavail,
		const char *db_str,
		int         force_init
		) {
	if (!plugin_loaded()) {
		return TAPCOMMEXIT_FAIL;
	}
	if (func_ptrs::init_shmem == nullptr) {
		func_ptrs::init_shmem = reinterpret_cast<func_ptrs::types::init_shmem_t>(dlsym(plugin_handle, "tap_commexit_init_shmem"));
	}
	if (func_ptrs::init_shmem != nullptr) {
		return (*func_ptrs::init_shmem)(errormsg, errormsglen, errormsgavail, db_str, force_init);
	}
	return TAPCOMMEXIT_FAIL;
}
void shutdown_shmem(
		char     *logmsg,
		uint32_t *logmsglen,
		uint32_t *logmsgavail
		) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::shutdown_shmem == nullptr) {
		func_ptrs::shutdown_shmem = reinterpret_cast<func_ptrs::types::shutdown_shmem_t>(dlsym(plugin_handle, "tap_commexit_shutdown_shmem"));
	}
	if (func_ptrs::shutdown_shmem != nullptr) {
		(*func_ptrs::shutdown_shmem)(logmsg, logmsglen, logmsgavail);
	}
}
void init(
		char            *logmsg,
		uint32_t        *logmsglen,
		uint32_t        *logmsgavail
		) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::init == nullptr) {
		func_ptrs::init = reinterpret_cast<func_ptrs::types::init_t>(dlsym(plugin_handle, "tap_commexit_init"));
	}
	if (func_ptrs::init != nullptr) {
		(*func_ptrs::init)(logmsg, logmsglen, logmsgavail);
	}
	init_db_functions();
}
TAPCOMMEXIT_RC send_server_data(
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
		) {
	if (!plugin_loaded()) {
		return TAPCOMMEXIT_FAIL;
	}
	if (func_ptrs::send_server_data == nullptr) {
		func_ptrs::send_server_data = reinterpret_cast<func_ptrs::types::send_server_data_t>(dlsym(plugin_handle, "tap_commexit_send_server_data"));
	}
	if (func_ptrs::send_server_data != nullptr) {
		return (*func_ptrs::send_server_data)(logmsg, logmsglen, logmsgavail, _pContext, pCommInfo, pReservedFlags, pBuffer, buffer_len, pNewBuffer, new_buffer_len);
	}
	return TAPCOMMEXIT_FAIL;
}
TAPCOMMEXIT_RC send_client_data(
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
		) {
	if (!plugin_loaded()) {
		return TAPCOMMEXIT_FAIL;
	}
	if (func_ptrs::send_client_data == nullptr) {
		func_ptrs::send_client_data = reinterpret_cast<func_ptrs::types::send_client_data_t>(dlsym(plugin_handle, "tap_commexit_send_client_data"));
	}
	if (func_ptrs::send_client_data != nullptr) {
		return (*func_ptrs::send_client_data)(logmsg, logmsglen, logmsgavail, _pContext, pCommInfo, pReservedFlags, pBuffer, buffer_len, pNewBuffer, new_buffer_len);
	}
	return TAPCOMMEXIT_FAIL;
}
void send_open(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        ) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::send_open == nullptr) {
		func_ptrs::send_open = reinterpret_cast<func_ptrs::types::send_open_t>(dlsym(plugin_handle, "tap_commexit_send_open"));
	}
	if (func_ptrs::send_open != nullptr) {
		(*func_ptrs::send_open)(logmsg, logmsglen, logmsgavail, _pContext, pCommInfo);
	}
}
void send_close(
        char            *logmsg,
        uint32_t        *logmsglen,
        uint32_t        *logmsgavail,
        void            *_pContext,
        const void      *pCommInfo
        ) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::send_close == nullptr) {
		func_ptrs::send_close = reinterpret_cast<func_ptrs::types::send_close_t>(dlsym(plugin_handle, "tap_commexit_send_close"));
	}
	if (func_ptrs::send_close != nullptr) {
		(*func_ptrs::send_close)(logmsg, logmsglen, logmsgavail, _pContext, pCommInfo);
	}
}
void *
get_context_opaque(
		const void            *_pContext
		) {
	if (!plugin_loaded()) {
		return nullptr;
	}
	if (func_ptrs::get_context_opaque == nullptr) {
		func_ptrs::get_context_opaque = reinterpret_cast<func_ptrs::types::get_context_opaque_t>(dlsym(plugin_handle, "tap_commexit_get_context_opaque"));
	}
	if (func_ptrs::get_context_opaque != nullptr) {
		return (*func_ptrs::get_context_opaque)(_pContext);
	}
	return nullptr;
}
void
set_context_opaque(
		void            *_pContext,
		void            *opaque
		) {
	if (!plugin_loaded()) {
		return;
	}
	if (func_ptrs::set_context_opaque == nullptr) {
		func_ptrs::set_context_opaque = reinterpret_cast<func_ptrs::types::set_context_opaque_t>(dlsym(plugin_handle, "tap_commexit_set_context_opaque"));
	}
	if (func_ptrs::set_context_opaque != nullptr) {
		(*func_ptrs::set_context_opaque)(_pContext, opaque);
	}
}
} // namespace tap_commexit
