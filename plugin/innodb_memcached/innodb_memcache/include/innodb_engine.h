/***********************************************************************

Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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

/** Range search mode, whether it is bound by one end, such as
LOW_BOUND (> key) or (>= key), UPPER_BOUND (< key) or (<= key),
or RANGE BOUND (key1 <[=] value <[=] key2) */
#define LOW_BOUND	0x1
#define UPPER_BOUND	0x2
#define RANGE_BOUND	0x4

/** structure to carry search keys */
typedef struct innodb_range_key {
	char*	start;			/* Low end key */
	int	start_len;		/* Low end key length */
	int	start_mode;		/* whether it is > or >= */
	char*	end;			/* high end key */
	int	end_len;		/* high end key len */
	int	end_mode;		/* whether it is < or <= */
	int	bound;			/* range search mode (LOW_BOUND etc.) */
} innodb_range_key_t;

/** Pre-allocated memory structure used to cache result */
typedef struct mem_buf_struct	mem_buf_t;

/** Result caching memory structure list */
struct mem_buf_struct {
	void*				mem;		/* memory buffer */
	UT_LIST_NODE_T(mem_buf_t)	mem_list;	/* list to next buf */
};

/** memory buffer list */
typedef UT_LIST_BASE_NODE_T(mem_buf_t)		mem_list_t;

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
	void**		row_buf;	/*!< row buffer to cache row read,
					it is array of 16k pages */
	ib_ulint_t	row_buf_slot;	/*!< row buffer pages used so far */
	ib_ulint_t	row_buf_used;	/*!< row buffer used (for multi-get)
					in a 16k buffer */
	bool		range;		/*!< range search */
	innodb_range_key_t* range_key;	/*!< range search key */
	bool		multi_get;	/*!< multiple get */
	void*		cmd_buf;	/*!< buffer for incoming command */
	ib_ulint_t	cmd_buf_len;	/*!< cmd buffer len */
#ifdef UNIV_MEMCACHED_SDI
	void*		sdi_buf;
	uint64_t	sdi_buf_len;
#endif /* UNIV_MEMCACHED_SDI */
	bool		result_in_use;	/*!< result set or above row_buf
					contain active result set */
	bool		use_default_mem;/*!<  whether to use default engine
					(memcached) memory */
	char*		mul_col_buf;	/*!< buffer to construct final result
					from multiple mapped column */
	ib_ulint_t	mul_col_buf_len;/*!< mul_col_buf len */
	ib_ulint_t	mul_col_buf_used;/*!< used length for multi-col
					buffer */
	mem_list_t	mul_used_buf;	/*!< list of multi-result buffer that
					can no longer fit additional result */
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
