#ifndef NDBMEMCACHE_NDB_ENGINE_H
#define NDBMEMCACHE_NDB_ENGINE_H

#include "config.h"

#include <pthread.h>
#include <stdbool.h>

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>
#include <innodb_config.h>

#include "atomics.h"

/** The InnoDB engine global data.

Inside memcached, a pointer to this is treated as simply an ENGINE_HANDLE_V1 
pointer.  But inside the NDB engine that pointer is cast up to point to the
whole private structure. 

This structure also contains a pointer to the default engine's 
private structure, since all caching is delegated to the default 
engine. */

 
/** structure contains the cursor information for each connection */
typedef struct innodb_conn_data		innodb_conn_data_t;

/* Connection specific data */
struct innodb_conn_data {
	ib_crsr_t       c_crsr;         /*!< data cursor */
	ib_crsr_t       c_idx_crsr;     /*!< index cursor */
	bool            c_in_use;       /*!< whether they are in use */
	void*		c_cookie;	/*!< connection cookie */
	UT_LIST_NODE_T(innodb_conn_data_t) c_list; /*!< list ptr */
};

typedef UT_LIST_BASE_NODE_T(innodb_conn_data_t)	conn_base_t;

typedef struct innodb_engine {
	ENGINE_HANDLE_V1	engine;
	SERVER_HANDLE_V1 	server;
	GET_SERVER_API 		get_server_api;
	ENGINE_HANDLE*		m_default_engine;

	struct {
		size_t nthreads;
		bool cas_enabled;  
	} server_options;

	union {
	engine_info info;
	char buffer[sizeof(engine_info) * (LAST_REGISTERED_ENGINE_FEATURE + 1)];
	} info;

	bool		initialized;
	bool		connected;

	unsigned int	cas_hi;
	ndbmc_atomic32_t cas_lo;
	//ib_crsr_t	g_crsr;

	meta_info_t	meta_info;

	conn_base_t	conn_data;
	pthread_mutex_t conn_mutex;
	ib_cb_t*	innodb_cb;
	
} innodb_engine_t;


#endif
