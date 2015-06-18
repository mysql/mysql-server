/***********************************************************************

Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/
/**************************************************//**
@file innodb_engine.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef INNODB_ENGINE_H
#define INNODB_ENGINE_H

#include "config.h"

#include <pthread.h>

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>
#include <innodb_utility.h>
#include <innodb_config.h>

/** Default settings that determine the number of write operation for
a connection before committing the transaction */
#define	CONN_NUM_WRITE_COMMIT	1

/** Default settings that determine the number of read operation for
a connection before committing the transaction */
#define CONN_NUM_READ_COMMIT	1048510

/** Structure contains the cursor information for each connection */
typedef struct innodb_conn_data_struct		innodb_conn_data_t;

/** Connection specific data */
struct innodb_conn_data_struct {
	ib_crsr_t	read_crsr;	/*!< read only cursor for the
					connection */
	ib_crsr_t	idx_read_crsr;	/*!< index cursor for read */
	ib_trx_t	crsr_trx;	/*!< transaction for write cursor */
	ib_crsr_t       crsr;		/*!< data cursor */
	ib_crsr_t       idx_crsr;	/*!< index cursor */
	ib_tpl_t	read_tpl;	/*!< read tuple */
	ib_tpl_t	sel_tpl;	/*!< read tuple */
	ib_tpl_t	tpl;		/*!< read tuple */
	ib_tpl_t	idx_tpl;	/*!< read tuple */
	void*		result;		/*!< result info */
	void*		row_buf;	/*!< row buffer to cache row read */
	ib_ulint_t	row_buf_len;	/*!< row buffer len */
	void*		cmd_buf;	/*!< buffer for incoming command */
	ib_ulint_t	cmd_buf_len;	/*!< cmd buffer len */
	bool		result_in_use;	/*!< result set or above row_buf
					contain active result set */
	bool		use_default_mem;/*!<  whether to use default engine
					(memcached) memory */
	void*		mul_col_buf;	/*!< buffer to construct final result
					from multiple mapped column */
	ib_ulint_t	mul_col_buf_len;/*!< mul_col_buf len */
	bool            in_use;		/*!< whether the connection
					is processing a request */
	bool		is_stale;	/*!< connection closed, this is
					stale */
	bool		is_flushing;	/*!< if flush is running. */
	bool            is_waiting_for_mdl;
					/*!< Used to detrmine if the connection is
					locked and waiting on MDL */
	void*		conn_cookie;	/*!< connection cookie */
	uint64_t	n_total_reads;	/*!< number of reads */
	uint64_t	n_reads_since_commit;
					/*!< number of reads since
					last commit */
	uint64_t        n_total_writes;	/*!< number of updates, including
					write/update/delete */
	uint64_t	n_writes_since_commit;
					/*!< number of updates since
					last commit */
	void*		thd;		/*!< MySQL THD, used for binlog */
	void*		mysql_tbl;	/*!< MySQL TABLE, used for binlog */
	meta_cfg_info_t*conn_meta;	/*!< metadata info for this
					connection */
	pthread_mutex_t	curr_conn_mutex;/*!< mutex protect current connection */
	UT_LIST_NODE_T(innodb_conn_data_t)
			conn_list;	/*!< list ptr */
};

typedef UT_LIST_BASE_NODE_T(innodb_conn_data_t)		conn_list_t;

/** The InnoDB engine global data. Some layout are common to NDB memcached
engine and InnoDB memcached engine */
typedef struct innodb_engine {
	/* members all common to Memcached Engines */
	ENGINE_HANDLE_V1	engine;		/*!< this InnoDB Memcached
						engine */
	SERVER_HANDLE_V1	server;		/*!< Memcached server */
	GET_SERVER_API		get_server_api;	/*!< call back to get Memcached
						server common functions */
	ENGINE_HANDLE*		default_engine;	/*!< default memcached engine */

	struct {
		size_t		nthreads;	/*!< number of threads handling
						connections */
		bool		cas_enabled;	/*!< whether cas is enabled */
	} server_options;

	union {
		engine_info	info;		/*!< engine specific info */
		char		buffer[sizeof(engine_info)
				       * (LAST_REGISTERED_ENGINE_FEATURE + 1)];
						/*!< buffer to store customized
						engine info */
	} info;

	bool			initialized;	/*!< whether engine data
						initialized */
	bool			connected;	/*!< whether connection
						established */
	bool			clean_stale_conn;
						/*!< whether bk thread is
						cleaning stale connections. */

	/* following are InnoDB specific variables */
	bool			enable_binlog;	/*!< whether binlog is enabled
						for InnoDB Memcached */
	bool			enable_mdl;	/*!< whether MDL is enabled
						for InnoDB Memcached */
	ib_trx_level_t		trx_level;	/*!< transaction isolation
						level */
	ib_ulint_t		bk_commit_interval;
						/*!< background commit
						interval in seconds */
	int			cfg_status;	/*!< configure status */
	meta_cfg_info_t*	meta_info;	/*!< default metadata info from
						configuration */
	conn_list_t		conn_data;	/*!< list of data specific for
						connections */
	pthread_mutex_t		conn_mutex;	/*!< mutex synchronizes
						connection specific data */
	pthread_mutex_t		cas_mutex;	/*!< mutex synchronizes
						CAS */
	pthread_mutex_t		flush_mutex;	/*!< mutex synchronizes
						flush and DMLs. */
	pthread_t		bk_thd_for_commit;/*!< background thread for
						committing long running
						transactions */
	ib_cb_t*		innodb_cb;	/*!< pointer to callback
						functions */
	uint64_t		read_batch_size;/*!< configured read batch
						size */
	uint64_t		write_batch_size;/*!< configured write batch
						size */
	hash_table_t*		meta_hash;	/*!< hash table for metadata */
} innodb_engine_t;

#endif /* INNODB_ENGINE_H */
