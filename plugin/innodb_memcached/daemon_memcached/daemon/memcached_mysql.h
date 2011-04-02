/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef MEMCACHED_MYSQL_H
#define MEMCACHED_MYSQL_H


/** \file
 * The main memcached header holding commonly used data
 * structures and function prototypes.
 */

struct memcached_context
{
	char*		m_engine_library;
	char*		m_address;
	char*		m_tcp_port;
	char*		m_max_conn;
	void*		m_innodb_api_cb;
}; 

typedef struct memcached_context        memcached_context_t;

#ifdef __cplusplus
extern "C" {
#endif
void* daemon_memcached_main(void *p);

void shutdown_server();
#ifdef __cplusplus
}
#endif

#endif    /* MEMCACHED_MYSQL_H */

