/***********************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file innodb_engine.c
InnoDB Memcached Engine code

Extracted and modified from NDB memcached project
04/12/2011 Jimmy Yang
*******************************************************/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "default_engine.h"
#include <memcached/util.h>
#include <memcached/config_parser.h>
#include <unistd.h>

#include "innodb_engine.h"
#include "innodb_engine_private.h"
#include "innodb_api.h"
#include "hash_item_util.h"
#include "innodb_cb_api.h"

/** Define also present in daemon/memcached.h */
#define KEY_MAX_LENGTH	250

/** Time (in seconds) that background thread sleeps before it wakes
up and commit idle connection transactions */
#define BK_COMMIT_THREAD_SLEEP_INTERVAL		5

/** Maximum number of connections that background thread processes each
time */
#define	BK_MAX_PROCESS_COMMIT			5

/** Minimum time (in seconds) that a connection has been idle, that makes
it candidate for background thread to commit it */
#define CONN_IDLE_TIME_TO_BK_COMMIT		5

/** Tells whether memcached plugin is being shutdown */
static bool	memcached_shutdown	= false;

/** Tells whether the background thread is exited */
static bool	bk_thd_exited		= true;

/** InnoDB Memcached engine configuration info */
typedef struct eng_config_info {
	char*		option_string;		/*!< memcached config option
						string */
	void*		cb_ptr;			/*!< call back function ptr */
	unsigned int	eng_read_batch_size;	/*!< read batch size */
	unsigned int	eng_write_batch_size;	/*!< write batch size */
	bool		eng_enable_binlog;	/*!< whether binlog is
						enabled specifically for
						this memcached engine */
} eng_config_info_t;

extern option_t config_option_names[];

/** Check the input key name implies a table mapping switch. The name
would start with "@@", and in the format of "@@new_table_mapping.key"
or simply "@@new_table_mapping" */


/**********************************************************************//**
Unlock a table and commit the transaction
return 0 if fail to commit the transaction */
extern
int
handler_unlock_table(
/*=================*/
	void*	my_thd,			/*!< in: thread */
	void*	my_table,		/*!< in: Table metadata */
	int	my_lock_mode);		/*!< in: lock mode */

/*******************************************************************//**
Get InnoDB Memcached engine handle
@return InnoDB Memcached engine handle */
static inline
struct innodb_engine*
innodb_handle(
/*==========*/
	ENGINE_HANDLE*	handle)		/*!< in: Generic engine handle */
{
	return((struct innodb_engine*) handle);
}

/*******************************************************************//**
Cleanup idle connections if "clear_all" is false, and clean up all
connections if "clear_all" is true.
@return number of connection cleaned */
static
void
innodb_conn_clean_data(
/*===================*/
	innodb_conn_data_t*	conn_data,
	bool			has_lock,
	bool			free_all);

/*******************************************************************//**
Get default Memcached engine handle
@return default Memcached engine handle */
static inline
struct default_engine*
default_handle(
/*===========*/
	struct innodb_engine*	eng)
{
	return((struct default_engine*) eng->default_engine);
}

/****** Gateway to the default_engine's create_instance() function */
ENGINE_ERROR_CODE
create_my_default_instance(
/*=======================*/
	uint64_t,
	GET_SERVER_API,
	ENGINE_HANDLE **);

/*********** FUNCTIONS IMPLEMENTING THE PUBLISHED API BEGIN HERE ********/

/*******************************************************************//**
Create InnoDB Memcached Engine.
@return ENGINE_SUCCESS if successful, otherwise, error code */
ENGINE_ERROR_CODE
create_instance(
/*============*/
	uint64_t		interface,	/*!< in: protocol version,
						currently always 1 */
	GET_SERVER_API		get_server_api,	/*!< in: Callback the engines
						may call to get the public
						server interface */
	ENGINE_HANDLE**		handle )	/*!< out: Engine handle */
{
	ENGINE_ERROR_CODE	err_ret;
	struct innodb_engine*	innodb_eng;

	SERVER_HANDLE_V1 *api = get_server_api();

	if (interface != 1 || api == NULL) {
		return(ENGINE_ENOTSUP);
	}

	innodb_eng = malloc(sizeof(struct innodb_engine));
	memset(innodb_eng, 0, sizeof(*innodb_eng));

	if (innodb_eng == NULL) {
		return(ENGINE_ENOMEM);
	}

	innodb_eng->engine.interface.interface = 1;
	innodb_eng->engine.get_info = innodb_get_info;
	innodb_eng->engine.initialize = innodb_initialize;
	innodb_eng->engine.destroy = innodb_destroy;
	innodb_eng->engine.allocate = innodb_allocate;
	innodb_eng->engine.remove = innodb_remove;
	innodb_eng->engine.release = innodb_release;
	innodb_eng->engine.clean_engine= innodb_clean_engine;
	innodb_eng->engine.get = innodb_get;
	innodb_eng->engine.get_stats = innodb_get_stats;
	innodb_eng->engine.reset_stats = innodb_reset_stats;
	innodb_eng->engine.store = innodb_store;
	innodb_eng->engine.arithmetic = innodb_arithmetic;
	innodb_eng->engine.flush = innodb_flush;
	innodb_eng->engine.unknown_command = innodb_unknown_command;
	innodb_eng->engine.item_set_cas = item_set_cas;
	innodb_eng->engine.get_item_info = innodb_get_item_info;
	innodb_eng->engine.get_stats_struct = NULL;
	innodb_eng->engine.errinfo = NULL;
	innodb_eng->engine.bind = innodb_bind;

	innodb_eng->server = *api;
	innodb_eng->get_server_api = get_server_api;

	/* configuration, with default values*/
	innodb_eng->info.info.description = "InnoDB Memcache " VERSION;
	innodb_eng->info.info.num_features = 3;
	innodb_eng->info.info.features[0].feature = ENGINE_FEATURE_CAS;
	innodb_eng->info.info.features[1].feature =
		ENGINE_FEATURE_PERSISTENT_STORAGE;
	innodb_eng->info.info.features[0].feature = ENGINE_FEATURE_LRU;

	/* Now call create_instace() for the default engine */
	err_ret = create_my_default_instance(interface, get_server_api,
				       &(innodb_eng->default_engine));

	if (err_ret != ENGINE_SUCCESS) {
		free(innodb_eng);
		return(err_ret);
	}

	innodb_eng->clean_stale_conn = false;
	innodb_eng->initialized = true;

	*handle = (ENGINE_HANDLE*) &innodb_eng->engine;

	return(ENGINE_SUCCESS);
}

/*******************************************************************//**
background thread to commit trx.
@return dummy parameter */
static
void*
innodb_bk_thread(
/*=============*/
	void*   arg)
{
	ENGINE_HANDLE*		handle;
	struct innodb_engine*	innodb_eng;
	innodb_conn_data_t*	conn_data;
	void*			thd = NULL;

	bk_thd_exited = false;

	handle = (ENGINE_HANDLE*) (arg);
	innodb_eng = innodb_handle(handle);

	if (innodb_eng->enable_binlog) {
		/* This thread will commit the transactions
		on behalf of the other threads. It will "pretend"
		to be each connection thread while doing it. */
		thd = handler_create_thd(true);
	}

	conn_data = UT_LIST_GET_FIRST(innodb_eng->conn_data);

	while(!memcached_shutdown) {
		innodb_conn_data_t*	next_conn_data;
		uint64_t                time;
		uint64_t		trx_start = 0;
		uint64_t		processed_count = 0;

		/* Do the cleanup every innodb_eng->bk_commit_interval
		seconds. We also check if the plugin is being shutdown
		every second */
		for (uint i = 0; i < innodb_eng->bk_commit_interval; i++) {
			sleep(1);

			/* If memcached is being shutdown, break */
			if (memcached_shutdown) {
				break;
			}
		}

		time = mci_get_time();

		if (UT_LIST_GET_LEN(innodb_eng->conn_data) == 0) {
			continue;
		}

		/* Set the clean_stale_conn to prevent force clean in
		innodb_conn_clean. */
		LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
		innodb_eng->clean_stale_conn = true;
		UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);

		if (!conn_data) {
			conn_data = UT_LIST_GET_FIRST(innodb_eng->conn_data);
		}

		if (conn_data) {
			next_conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
		} else {
			next_conn_data = NULL;
		}

		while (conn_data) {
			LOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);

			if (conn_data->is_stale) {
				UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
					false, conn_data);
				LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
				UT_LIST_REMOVE(conn_list, innodb_eng->conn_data,
					       conn_data);
				UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
				innodb_conn_clean_data(conn_data, false, true);
				goto next_item;
			}

			if (conn_data->crsr_trx) {
				trx_start = ib_cb_trx_get_start_time(
						conn_data->crsr_trx);
			}

			/* Check the trx, if it is qualified for
			reset and commit */
			if ((conn_data->n_writes_since_commit > 0
			     || conn_data->n_reads_since_commit > 0)
			    && trx_start
			    && (time - trx_start > CONN_IDLE_TIME_TO_BK_COMMIT)
			    && !conn_data->in_use) {
				/* binlog is running, make the thread
				attach to conn_data->thd for binlog
				committing */
				if (thd) {
					handler_thd_attach(
						conn_data->thd, NULL);
				}

				innodb_reset_conn(conn_data, true, true,
						  innodb_eng->enable_binlog);
				processed_count++;
			}

			UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);

next_item:
			conn_data = next_conn_data;

			/* Process BK_MAX_PROCESS_COMMIT (5) trx at a time */
			if (processed_count > BK_MAX_PROCESS_COMMIT) {
				break;
			}

			if (conn_data) {
				next_conn_data = UT_LIST_GET_NEXT(
					conn_list, conn_data);
			}
		}
		/* Set the clean_stale_conn back. */
		LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
		innodb_eng->clean_stale_conn = false;
		UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
	}

	bk_thd_exited = true;

	/* Change to its original state before close the MySQL THD */
	if (thd) {
		handler_thd_attach(thd, NULL);
		handler_close_thd(thd);
	}

	pthread_detach(pthread_self());
        pthread_exit(NULL);

	return((void*) 0);
}

/*******************************************************************//**
Get engine info.
@return engine info */
static
const engine_info*
innodb_get_info(
/*============*/
	ENGINE_HANDLE*	handle)		/*!< in: Engine handle */
{
	return(&innodb_handle(handle)->info.info);
}

/*******************************************************************//**
Initialize InnoDB Memcached Engine.
@return ENGINE_SUCCESS if successful */
static
ENGINE_ERROR_CODE
innodb_initialize(
/*==============*/
	ENGINE_HANDLE*	handle,		/*!< in/out: InnoDB memcached
					engine */
	const char*	config_str)	/*!< in: configure string */
{
	ENGINE_ERROR_CODE	return_status = ENGINE_SUCCESS;
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);
	eng_config_info_t*	my_eng_config;
	pthread_attr_t          attr;

	my_eng_config = (eng_config_info_t*) config_str;

	/* If no call back function registered (InnoDB engine failed to load),
	load InnoDB Memcached engine should fail too */
	if (!my_eng_config->cb_ptr) {
		return(ENGINE_TMPFAIL);
	}

	/* Register the call back function */
	register_innodb_cb((void*) my_eng_config->cb_ptr);

	innodb_eng->read_batch_size = (my_eng_config->eng_read_batch_size
					? my_eng_config->eng_read_batch_size
					: CONN_NUM_READ_COMMIT);

	innodb_eng->write_batch_size = (my_eng_config->eng_write_batch_size
					? my_eng_config->eng_write_batch_size
					: CONN_NUM_WRITE_COMMIT);

	innodb_eng->enable_binlog = my_eng_config->eng_enable_binlog;

	innodb_eng->cfg_status = innodb_cb_get_cfg();

	/* If binlog is not enabled by InnoDB memcached plugin, let's
	check whether innodb_direct_access_enable_binlog is turned on */
	if (!innodb_eng->enable_binlog) {
		innodb_eng->enable_binlog = innodb_eng->cfg_status
					    & IB_CFG_BINLOG_ENABLED;
	}

	innodb_eng->enable_mdl = innodb_eng->cfg_status & IB_CFG_MDL_ENABLED;
	innodb_eng->trx_level = ib_cb_cfg_trx_level();
	innodb_eng->bk_commit_interval = ib_cb_cfg_bk_commit_interval();

	UT_LIST_INIT(innodb_eng->conn_data);
	pthread_mutex_init(&innodb_eng->conn_mutex, NULL);
	pthread_mutex_init(&innodb_eng->cas_mutex, NULL);

	/* Fetch InnoDB specific settings */
	innodb_eng->meta_info = innodb_config(
		NULL, 0, &innodb_eng->meta_hash);

	if (!innodb_eng->meta_info) {
		return(ENGINE_TMPFAIL);
	}

	if (innodb_eng->default_engine) {
		return_status = def_eng->engine.initialize(
			innodb_eng->default_engine,
			my_eng_config->option_string);
	}

	memcached_shutdown = false;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&innodb_eng->bk_thd_for_commit, &attr, innodb_bk_thread,
		       handle);

	return(return_status);
}

extern void handler_close_thd(void*);

/*******************************************************************//**
Cleanup idle connections if "clear_all" is false, and clean up all
connections if "clear_all" is true.
@return number of connection cleaned */
static
void
innodb_conn_clean_data(
/*===================*/
	innodb_conn_data_t*	conn_data,
	bool			has_lock,
	bool			free_all)
{
	if (!conn_data) {
		return;
	}

	LOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

	if (conn_data->idx_crsr) {
		innodb_cb_cursor_close(conn_data->idx_crsr);
		conn_data->idx_crsr = NULL;
	}

	if (conn_data->idx_read_crsr) {
		innodb_cb_cursor_close(conn_data->idx_read_crsr);
		conn_data->idx_read_crsr = NULL;
	}

	if (conn_data->crsr) {
		innodb_cb_cursor_close(conn_data->crsr);
		conn_data->crsr = NULL;
	}

	if (conn_data->read_crsr) {
		innodb_cb_cursor_close(conn_data->read_crsr);
		conn_data->read_crsr = NULL;
	}

	if (conn_data->crsr_trx) {
		innodb_cb_trx_commit(conn_data->crsr_trx);
		conn_data->crsr_trx = NULL;
	}

	if (conn_data->mysql_tbl) {
		assert(conn_data->thd);
		handler_unlock_table(conn_data->thd,
				     conn_data->mysql_tbl,
				     HDL_READ);
		conn_data->mysql_tbl = NULL;
	}

	if (conn_data->thd) {
		handler_close_thd(conn_data->thd);
		conn_data->thd = NULL;
	}

	UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

	if (free_all) {
		pthread_mutex_destroy(&conn_data->curr_conn_mutex);
		free(conn_data);
	}
}

/*******************************************************************//**
Cleanup idle connections if "clear_all" is false, and clean up all
connections if "clear_all" is true.
@return number of connection cleaned */
static
int
innodb_conn_clean(
/*==============*/
	innodb_engine_t*	engine,		/*!< in/out: InnoDB memcached
						engine */
	bool			clear_all,	/*!< in: Clear all connection */
	bool			has_lock)	/*!< in: Has engine mutext */
{
	innodb_conn_data_t*	conn_data;
	innodb_conn_data_t*	next_conn_data;
	int			num_freed = 0;
	void*			thd = NULL;

	if (engine->enable_binlog && clear_all) {
		thd = handler_create_thd(true);
	}

	LOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

	conn_data = UT_LIST_GET_FIRST(engine->conn_data);

	while (conn_data) {
		void*	cookie = conn_data->conn_cookie;

		next_conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);

		if (!clear_all && !conn_data->in_use) {
			innodb_conn_data_t*	check_data;
			check_data = engine->server.cookie->get_engine_specific(
				cookie);

			/* The check data is the original conn_data stored
			in connection "cookie", it can be set to NULL if
			connection closed, or to a new conn_data if it is
			closed and reopened. So verify and see if our
			current conn_data is stale */
			if (!check_data || check_data != conn_data) {
				assert(conn_data->is_stale);
			}
		}

		/* If current conn is stale or clear_all is true,
		clean up it.*/
		if (conn_data->is_stale) {
			/* If bk thread is doing the same thing, stop
			the loop to avoid confliction.*/
			if (engine->clean_stale_conn)
				break;

			UT_LIST_REMOVE(conn_list, engine->conn_data,
				       conn_data);
			innodb_conn_clean_data(conn_data, false, true);
			num_freed++;
		} else {
			if (clear_all) {
				UT_LIST_REMOVE(conn_list, engine->conn_data,
					       conn_data);

				if (thd) {
					handler_thd_attach(conn_data->thd,
							   NULL);
				}

				innodb_reset_conn(conn_data, false, true,
						  engine->enable_binlog);
				if (conn_data->thd) {
					handler_thd_attach(
						conn_data->thd, NULL);
				}
				innodb_conn_clean_data(conn_data, false, true);

				engine->server.cookie->store_engine_specific(
					cookie, NULL);
				num_freed++;
			}
		}

		conn_data = next_conn_data;
	}

	assert(!clear_all || engine->conn_data.count == 0);

	UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

	if (thd) {
		handler_thd_attach(thd, NULL);
		handler_close_thd(thd);
	}

	return(num_freed);
}

/*******************************************************************//**
Destroy and Free InnoDB Memcached engine */
static
void
innodb_destroy(
/*===========*/
	ENGINE_HANDLE*	handle,		/*!< in: Destroy the engine instance */
	bool		force)		/*!< in: Force to destroy */
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);

	memcached_shutdown = true;

	/* Wait for the background thread to exit */
	while (!bk_thd_exited) {
		sleep(1);
	}

	innodb_conn_clean(innodb_eng, true, false);

	if (innodb_eng->meta_hash) {
		HASH_CLEANUP(innodb_eng->meta_hash, meta_cfg_info_t*);
	}

	pthread_mutex_destroy(&innodb_eng->conn_mutex);
	pthread_mutex_destroy(&innodb_eng->cas_mutex);

	if (innodb_eng->default_engine) {
		def_eng->engine.destroy(innodb_eng->default_engine, force);
	}

	free(innodb_eng);
}


/*** allocate ***/

/*******************************************************************//**
Allocate gets a struct item from the slab allocator, and fills in
everything but the value.  It seems like we can just pass this on to
the default engine; we'll intercept it later in store(). */
static
ENGINE_ERROR_CODE
innodb_allocate(
/*============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine handle */
	const void*	cookie,		/*!< in: connection cookie */
	item **		item,		/*!< out: item to allocate */
	const void*	key,		/*!< in: key */
	const size_t	nkey,		/*!< in: key length */
	const size_t	nbytes,		/*!< in: estimated value length */
	const int	flags,		/*!< in: flag */
	const rel_time_t exptime)	/*!< in: expiration time */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);

	/* We use default engine's memory allocator to allocate memory
	for item */
	return(def_eng->engine.allocate(innodb_eng->default_engine,
					cookie, item, key, nkey, nbytes,
					flags, exptime));
}

/** Defines for connection initialization to indicate if we will
do a read or write operation, or in the case of CONN_MODE_NONE, just get
the connection's conn_data structure */
enum conn_mode {
	CONN_MODE_READ,
	CONN_MODE_WRITE,
	CONN_MODE_NONE
};

/*******************************************************************//**
Cleanup connections
@return number of connection cleaned */
/* Initialize a connection's cursor and transactions
@return the connection's conn_data structure */
static
innodb_conn_data_t*
innodb_conn_init(
/*=============*/
	innodb_engine_t*	engine,		/*!< in/out: InnoDB memcached
						engine */
	const void*		cookie,		/*!< in: This connection's
						cookie */
	int			conn_option,	/*!< in: whether it is
						for read or write operation*/
	ib_lck_mode_t		lock_mode,	/*!< in: Table lock mode */
	bool			has_lock)	/*!< in: Has engine mutex */
{
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info;
	meta_index_t*		meta_index;
	ib_err_t		err = DB_SUCCESS;
	ib_crsr_t		crsr;
	ib_crsr_t		read_crsr;
	ib_crsr_t		idx_crsr;
	bool			trx_updated = false;

	LOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

	/* Get this connection's conn_data */
	conn_data = engine->server.cookie->get_engine_specific(cookie);

	assert(!conn_data || !conn_data->in_use);

	if (!conn_data) {
		if (UT_LIST_GET_LEN(engine->conn_data) > 2048) {
			/* Some of conn_data can be stale, recycle them */
			innodb_conn_clean(engine, false, true);
		}

		conn_data = malloc(sizeof(*conn_data));

		if (!conn_data) {
			return(NULL);
		}

		memset(conn_data, 0, sizeof(*conn_data));
		conn_data->conn_cookie = (void*) cookie;
		UT_LIST_ADD_LAST(conn_list, engine->conn_data, conn_data);
		engine->server.cookie->store_engine_specific(
			cookie, conn_data);
		conn_data->conn_meta = engine->meta_info;
		pthread_mutex_init(&conn_data->curr_conn_mutex, NULL);
	}

	meta_info = conn_data->conn_meta;
	meta_index = &meta_info->index_info;

	assert(engine->conn_data.count > 0);

	if (conn_option == CONN_MODE_NONE) {
		UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
		return(conn_data);
	}

	LOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
	conn_data->in_use = true;

	UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

	crsr = conn_data->crsr;
	read_crsr = conn_data->read_crsr;

	if (lock_mode == IB_LOCK_TABLE_X) {
		assert(!conn_data->crsr_trx);

		conn_data->crsr_trx = innodb_cb_trx_begin(
			engine->trx_level);

		err = innodb_api_begin(
			engine,
			meta_info->col_info[CONTAINER_DB].col_name,
			meta_info->col_info[CONTAINER_TABLE].col_name,
			conn_data, conn_data->crsr_trx,
			&conn_data->crsr, &conn_data->idx_crsr,
			lock_mode);

		if (err != DB_SUCCESS) {
			innodb_cb_cursor_close(
				conn_data->crsr);
			conn_data->crsr = NULL;
			innodb_cb_trx_commit(
				conn_data->crsr_trx);
			conn_data->crsr_trx = NULL;
			conn_data->in_use = false;
			UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
				has_lock, conn_data);
			return(NULL);
		}

		UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
		return(conn_data);
	}

	/* Write operation */
	if (conn_option == CONN_MODE_WRITE) {
		if (!crsr) {
			if (!conn_data->crsr_trx) {
				conn_data->crsr_trx = innodb_cb_trx_begin(
					engine->trx_level);
				trx_updated = true;
			}

			err = innodb_api_begin(
				engine,
				meta_info->col_info[CONTAINER_DB].col_name,
				meta_info->col_info[CONTAINER_TABLE].col_name,
				conn_data, conn_data->crsr_trx,
				&conn_data->crsr, &conn_data->idx_crsr,
				lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->crsr);
				conn_data->crsr = NULL;
				innodb_cb_trx_commit(
					conn_data->crsr_trx);
				conn_data->crsr_trx = NULL;
				conn_data->in_use = false;

				UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
					has_lock, conn_data);
				return(NULL);
			}

		} else if (!conn_data->crsr_trx) {

			/* There exists a cursor, just need update
			with a new transaction */
			conn_data->crsr_trx = innodb_cb_trx_begin(
				engine->trx_level);

			innodb_cb_cursor_new_trx(crsr, conn_data->crsr_trx);
			trx_updated = true;

			err = innodb_cb_cursor_lock(engine, crsr, lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->crsr);
				conn_data->crsr = NULL;
				conn_data->crsr_trx = NULL;
				conn_data->in_use = false;
				UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
					has_lock, conn_data);
				return(NULL);
			}

			if (meta_index->srch_use_idx == META_USE_SECONDARY) {

				idx_crsr = conn_data->idx_crsr;
				innodb_cb_cursor_new_trx(
					idx_crsr, conn_data->crsr_trx);
				innodb_cb_cursor_lock(
					engine, idx_crsr, lock_mode);
			}
		} else {
			err = innodb_cb_cursor_lock(engine, crsr, lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->crsr);
				conn_data->crsr = NULL;
				conn_data->crsr_trx = NULL;
				conn_data->in_use = false;
				UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
					has_lock, conn_data);
				return(NULL);
			}
		}

		if (trx_updated) {
			if (conn_data->read_crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->read_crsr,
					conn_data->crsr_trx);
			}

			if (conn_data->idx_read_crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->idx_read_crsr,
					conn_data->crsr_trx);
			}
		}
	} else {
		assert(conn_option == CONN_MODE_READ);

		if (!read_crsr) {
			if (!conn_data->crsr_trx) {
				conn_data->crsr_trx = innodb_cb_trx_begin(
					engine->trx_level);
				trx_updated = true;
			}

			err = innodb_api_begin(
				engine,
				meta_info->col_info[CONTAINER_DB].col_name,
				meta_info->col_info[CONTAINER_TABLE].col_name,
				conn_data,
				conn_data->crsr_trx,
				&conn_data->read_crsr,
				&conn_data->idx_read_crsr,
				lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->read_crsr);
				innodb_cb_trx_commit(
					conn_data->crsr_trx);
				conn_data->crsr_trx = NULL;
				conn_data->read_crsr = NULL;
				conn_data->in_use = false;
				UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(
					has_lock, conn_data);

				return(NULL);
			}

		} else if (!conn_data->crsr_trx) {
			conn_data->crsr_trx = innodb_cb_trx_begin(
				engine->trx_level);

			trx_updated = true;

			innodb_cb_cursor_new_trx(
				conn_data->read_crsr,
				conn_data->crsr_trx);

			if (conn_data->crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->crsr,
					conn_data->crsr_trx);
			}

			innodb_cb_cursor_lock(
				engine, conn_data->read_crsr, lock_mode);

			if (meta_index->srch_use_idx == META_USE_SECONDARY) {
				ib_crsr_t idx_crsr = conn_data->idx_read_crsr;

				innodb_cb_cursor_new_trx(
					idx_crsr, conn_data->crsr_trx);
				innodb_cb_cursor_lock(
					engine, idx_crsr, lock_mode);
			}
		}

		if (trx_updated) {
			if (conn_data->crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->crsr,
					conn_data->crsr_trx);
			}

			if (conn_data->idx_crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->idx_crsr,
					conn_data->crsr_trx);
			}
		}
	}

	UNLOCK_CURRENT_CONN_IF_NOT_LOCKED( has_lock, conn_data);

	return(conn_data);
}

/*******************************************************************//**
Cleanup connections
@return number of connection cleaned */
static
ENGINE_ERROR_CODE
innodb_remove(
/*==========*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie,		/*!< in: connection cookie */
	const void*		key,		/*!< in: key */
	const size_t		nkey,		/*!< in: key length */
	uint64_t		cas __attribute__((unused)),
						/*!< in: cas */
	uint16_t		vbucket __attribute__((unused)))
						/*!< in: bucket, used by default
						engine only */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	ENGINE_ERROR_CODE	cacher_err = ENGINE_KEY_ENOENT;

	if (meta_info->del_option == META_CACHE_OPT_DISABLE) {
		return(ENGINE_SUCCESS);
	}

	if (meta_info->del_option == META_CACHE_OPT_DEFAULT
	    || meta_info->del_option == META_CACHE_OPT_MIX) {
		hash_item*	item = item_get(def_eng, key, nkey);

		if (item != NULL) {
			item_unlink(def_eng, item);
			item_release(def_eng, item);
			cacher_err = ENGINE_SUCCESS;
		}

		if (meta_info->del_option == META_CACHE_OPT_DEFAULT) {
			return(cacher_err);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie,
				     CONN_MODE_WRITE, IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_TMPFAIL);
	}

	/* In the binary protocol there is such a thing as a CAS delete.
	This is the CAS check. If we will also be deleting from the database,
	there are two possibilities:
	  1: The CAS matches; perform the delete.
	  2: The CAS doesn't match; delete the item because it's stale.
	Therefore we skip the check altogether if(do_db_delete) */

	err_ret = innodb_api_delete(innodb_eng, conn_data, key, nkey);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_DELETE,
				err_ret == ENGINE_SUCCESS);

	return((cacher_err == ENGINE_SUCCESS) ? ENGINE_SUCCESS : err_ret);
}

/*******************************************************************//**
Switch the table mapping. Open the new table specified in "@@new_table_map.key"
string.
@return ENGINE_SUCCESS if successful, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_switch_mapping(
/*==================*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie,		/*!< in: connection cookie */
	const char*		name,		/*!< in: full name contains
						table map name, and possible
						key value */
	size_t*			name_len,	/*!< in/out: name length,
						out with length excludes
						the table map name */
	bool			has_prefix)	/*!< in: whether the name has
						"@@" prefix */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	innodb_conn_data_t*	conn_data;
	char			new_name[KEY_MAX_LENGTH];
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	char*			new_map_name;
	unsigned int		new_map_name_len = 0;
	char*			last;
	meta_cfg_info_t*	new_meta_info;
	int			sep_len = 0;

	if (has_prefix) {
		char*		sep = NULL;

		assert(*name_len > 2 && name[0] == '@' && name[1] == '@');
		assert(*name_len < KEY_MAX_LENGTH);

		memcpy(new_name, &name[2], (*name_len) - 2);

		new_name[*name_len - 2] = 0;

		GET_OPTION(meta_info, OPTION_ID_TBL_MAP_SEP, sep, sep_len);

		assert(sep_len > 0);

		new_map_name = strtok_r(new_name, sep, &last);

		if (new_map_name == NULL) {
			return(ENGINE_KEY_ENOENT);
		}

		new_map_name_len = strlen(new_map_name);
	} else {
		/* This is used in the "bind" command, and without the
		"@@" prefix. */
		if (name == NULL) {
			return(ENGINE_KEY_ENOENT);
		}

		new_map_name = (char*) name;
		new_map_name_len = *name_len;
	}

	conn_data = innodb_eng->server.cookie->get_engine_specific(cookie);

	/* Check if we are getting the same configure setting as existing one */
	if (conn_data && conn_data->conn_meta
	    && (new_map_name_len
		== conn_data->conn_meta->col_info[CONTAINER_NAME].col_name_len)
	    && (strcmp(
		new_map_name,
		conn_data->conn_meta->col_info[CONTAINER_NAME].col_name) == 0)) {
		goto get_key_name;
	}

	new_meta_info = innodb_config(
		new_map_name, new_map_name_len, &innodb_eng->meta_hash);

	if (!new_meta_info) {
		return(ENGINE_KEY_ENOENT);
	}

	/* Clean up the existing connection metadata if exists */
	if (conn_data) {
		innodb_conn_clean_data(conn_data, false, false);
	}

	conn_data = innodb_conn_init(innodb_eng, cookie,
				     CONN_MODE_NONE, 0, false);

	/* Point to the new metadata */
	conn_data->conn_meta = new_meta_info;

get_key_name:
	/* Now calculate name length exclude the table mapping name,
	this is the length for the remaining key portion */
	if (has_prefix) {
		assert(*name_len >= strlen(new_map_name) + 2);

		if (*name_len >= strlen(new_map_name) + 2 + sep_len) {
			*name_len -= strlen(new_map_name) + 2 + sep_len;
		} else {
			/* the name does not even contain a delimiter,
			so there will be no keys either */
			*name_len  = 0;
		}
	}

	return(ENGINE_SUCCESS);
}

/*******************************************************************//**
check whether a table mapping switch is needed, if so, switch the table
mapping
@return ENGINE_SUCCESS if successful otherwise error code */
static inline
ENGINE_ERROR_CODE
check_key_name_for_map_switch(
/*==========================*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie,		/*!< in: connection cookie */
	const void*		key,		/*!< in: search key */
	size_t*			nkey)		/*!< in/out: key length */
{
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;

	if ((*nkey) > 3 && ((char*)key)[0] == '@'
	    && ((char*)key)[1] == '@') {
		err_ret = innodb_switch_mapping(handle, cookie, key, nkey, true);
	}

	return(err_ret);
}

/*******************************************************************//**
Function to support the "bind" command, bind the connection to a new
table mapping.
@return ENGINE_SUCCESS if successful, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_bind(
/*========*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie,		/*!< in: connection cookie */
	const void*		name,		/*!< in: table ID name */
	size_t			name_len)	/*!< in: name length */
{
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;

	err_ret = innodb_switch_mapping(handle, cookie, name, &name_len, false);

	return(err_ret);
}

/*******************************************************************//**
Release the connection, free resource allocated in innodb_allocate */
static
void
innodb_clean_engine(
/*================*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie __attribute__((unused)),
						/*!< in: connection cookie */
	void*			conn)		/*!< in: item to free */
{
	innodb_conn_data_t*	conn_data = (innodb_conn_data_t*)conn;
	struct innodb_engine*	engine = innodb_handle(handle);
	void*			orignal_thd;

	LOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
	if (conn_data->thd) {
		handler_thd_attach(conn_data->thd, &orignal_thd);
	}
	innodb_reset_conn(conn_data, true, true, engine->enable_binlog);
	innodb_conn_clean_data(conn_data, true, false);
	conn_data->is_stale = true;
	UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
}

/*******************************************************************//**
Release the connection, free resource allocated in innodb_allocate */
static
void
innodb_release(
/*===========*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie __attribute__((unused)),
						/*!< in: connection cookie */
	item*			item)		/*!< in: item to free */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);

	if (item) {
		item_release(def_eng, (hash_item *) item);
	}

	return;
}

/*******************************************************************//**
Support memcached "GET" command, fetch the value according to key
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_get(
/*=======*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie,		/*!< in: connection cookie */
	item**			item,		/*!< out: item to fill */
	const void*		key,		/*!< in: search key */
	const int		nkey,		/*!< in: key length */
	uint16_t		vbucket __attribute__((unused)))
						/*!< in: bucket, used by default
						engine only */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	hash_item*		it = NULL;
	ib_crsr_t		crsr;
	ib_err_t		err = DB_SUCCESS;
	mci_item_t		result;
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;
	uint64_t		cas = 0;
	uint64_t		exp = 0;
	uint64_t		flags = 0;
	innodb_conn_data_t*	conn_data;
	int			total_len = 0;
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	int			option_length;
	const char*		option_delimiter;
	size_t			key_len = nkey;
	int			lock_mode;
	char			table_name[MAX_TABLE_NAME_LEN
					   + MAX_DATABASE_NAME_LEN];
	bool			report_table_switch = false;

	if (meta_info->get_option == META_CACHE_OPT_DISABLE) {
		return(ENGINE_KEY_ENOENT);
	}

	if (meta_info->get_option == META_CACHE_OPT_DEFAULT
	    || meta_info->get_option == META_CACHE_OPT_MIX) {
		*item = item_get(default_handle(innodb_eng), key, nkey);

		if (*item != NULL) {
			return(ENGINE_SUCCESS);
		}

		if (meta_info->get_option == META_CACHE_OPT_DEFAULT) {
			return(ENGINE_KEY_ENOENT);
		}
	}

	/* Check if we need to switch table mapping */
	err_ret = check_key_name_for_map_switch(handle, cookie, key, &key_len);

	/* If specified new table map does not exist, or table does not
	qualify for InnoDB memcached, return error */
	if (err_ret != ENGINE_SUCCESS) {
		goto err_exit;
	}

	/* If only the new mapping name is provided, and no key value,
	return here */
	if (key_len <= 0) {
		/* If this is a command in the form of "get @@new_table_map",
		for the purpose of switching to the specified table with
		the table map name, if the switch is successful, we will
		return the table name as result */
		if (nkey > 0) {
			char*	name = meta_info->col_info[
					CONTAINER_TABLE].col_name;
			char*	dbname = meta_info->col_info[
					CONTAINER_DB].col_name;
#ifdef __WIN__
			sprintf(table_name, "%s\%s", dbname, name);
#else
			snprintf(table_name, sizeof(table_name),
				 "%s/%s", dbname, name);
#endif
			memset(&result, 0, sizeof(result));

			result.col_value[MCI_COL_VALUE].value_str = table_name;
			result.col_value[MCI_COL_VALUE].value_len = strlen(table_name);
			report_table_switch = true;

			goto search_done;
		}

		err_ret = ENGINE_KEY_ENOENT;
		goto err_exit;
	}

	lock_mode = (innodb_eng->trx_level == IB_TRX_SERIALIZABLE
		     && innodb_eng->read_batch_size == 1)
			? IB_LOCK_S
			: IB_LOCK_NONE;

	conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_READ,
				     lock_mode, false);

	if (!conn_data) {
		return(ENGINE_TMPFAIL);
	}

	err = innodb_api_search(conn_data, &crsr, key + nkey - key_len,
				key_len, &result, NULL, true);

	if (err != DB_SUCCESS) {
		err_ret = ENGINE_KEY_ENOENT;
		goto func_exit;
	}

search_done:

	/* Only if expiration field is enabled, and the value is not zero,
	we will check whether the item is expired */
	if (result.col_value[MCI_COL_EXP].is_valid
	    && result.col_value[MCI_COL_EXP].value_int) {
		uint64_t		time;
		time = mci_get_time();

		if (time > result.col_value[MCI_COL_EXP].value_int) {
			/* Free allocated memory. */
			if (result.extra_col_value) {
				for (int i = 0; i < result.n_extra_col; i++) {
					free(result.extra_col_value[i].value_str);
				}

				free(result.extra_col_value);
			}
			if (result.col_value[MCI_COL_VALUE].allocated) {
				free(result.col_value[MCI_COL_VALUE].value_str);
				result.col_value[MCI_COL_VALUE].allocated =
					false;
			}

			err_ret = ENGINE_KEY_ENOENT;
			goto func_exit;
		}
	}

	if (result.col_value[MCI_COL_FLAG].is_valid) {
		flags = ntohl(result.col_value[MCI_COL_FLAG].value_int);
	}

	if (result.col_value[MCI_COL_CAS].is_valid) {
		cas = result.col_value[MCI_COL_CAS].value_int;
	}

	if (result.col_value[MCI_COL_EXP].is_valid) {
		exp = result.col_value[MCI_COL_EXP].value_int;
	}

	if (result.extra_col_value) {
		int	i;

		GET_OPTION(meta_info, OPTION_ID_COL_SEP, option_delimiter,
			   option_length);

		for (i = 0; i < result.n_extra_col; i++) {

			total_len += (result.extra_col_value[i].value_len
				      + option_length);
		}

		/* No need to add the last separator */
		total_len -= option_length;
	} else {
		total_len = result.col_value[MCI_COL_VALUE].value_len;
	}

	innodb_allocate(handle, cookie, item, key, nkey, total_len, flags, exp);

        it = *item;

	if (it->iflag & ITEM_WITH_CAS) {
		hash_item_set_cas(it, cas);
	}

	if (result.extra_col_value) {
		int		i;
		char*		c_value = hash_item_get_data(it);
		char*		value_end = c_value + total_len;

		assert(option_length > 0 && option_delimiter);

		for (i = 0; i < result.n_extra_col; i++) {
			mci_column_t*	col_value;

			col_value = &result.extra_col_value[i];

			if (col_value->value_len != 0) {
				memcpy(c_value,
				       col_value->value_str,
				       col_value->value_len);
				c_value += col_value->value_len;
			}

			if (i < result.n_extra_col - 1 ) {
				memcpy(c_value, option_delimiter, option_length);
				c_value += option_length;
			}

			assert(c_value <= value_end);
			free(result.extra_col_value[i].value_str);
		}

		free(result.extra_col_value);
	} else {
		assert(result.col_value[MCI_COL_VALUE].value_len
		       >= (int) it->nbytes);

		memcpy(hash_item_get_data(it),
		       result.col_value[MCI_COL_VALUE].value_str,
		       it->nbytes);

		if (result.col_value[MCI_COL_VALUE].allocated) {
			free(result.col_value[MCI_COL_VALUE].value_str);
			result.col_value[MCI_COL_VALUE].allocated = false;
		}
	}

func_exit:

	if (!report_table_switch) {
		innodb_api_cursor_reset(innodb_eng, conn_data,
					CONN_OP_READ, true);
	}

err_exit:
	return(err_ret);
}

/*******************************************************************//**
Get statistics info
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_get_stats(
/*=============*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie,		/*!< in: connection cookie */
	const char*		stat_key,	/*!< in: statistics key */
	int			nkey,		/*!< in: key length */
	ADD_STAT		add_stat)	/*!< out: stats to fill */
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
	return(def_eng->engine.get_stats(innodb_eng->default_engine, cookie,
					 stat_key, nkey, add_stat));
}

/*******************************************************************//**
reset statistics
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
void
innodb_reset_stats(
/*===============*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie)		/*!< in: connection cookie */
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
	def_eng->engine.reset_stats(innodb_eng->default_engine, cookie);
}

/*******************************************************************//**
API interface for memcached's "SET", "ADD", "REPLACE", "APPEND"
"PREPENT" and "CAS" commands
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_store(
/*=========*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie,		/*!< in: connection cookie */
	item*			item,		/*!< out: result to fill */
	uint64_t*		cas,		/*!< in: cas value */
	ENGINE_STORE_OPERATION	op,		/*!< in: type of operation */
	uint16_t		vbucket	__attribute__((unused)))
						/*!< in: bucket, used by default
						engine only */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	uint16_t		len = hash_item_get_key_len(item);
	char*			value = hash_item_get_key(item);
	uint64_t		exptime = hash_item_get_exp(item);
	uint64_t		flags = hash_item_get_flag(item);
	ENGINE_ERROR_CODE	result;
	uint64_t		input_cas;
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	uint32_t		val_len = ((hash_item*)item)->nbytes;
	size_t			key_len = len;
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;

	if (meta_info->set_option == META_CACHE_OPT_DISABLE) {
		return(ENGINE_SUCCESS);
	}

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		result = store_item(default_handle(innodb_eng), item, cas,
				    op, cookie);

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(result);
		}
	}

	err_ret = check_key_name_for_map_switch(handle, cookie,
						value, &key_len);

	if (err_ret != ENGINE_SUCCESS) {
		return(err_ret);
	}

	/* If no key is provided, return here */
	if (key_len <= 0) {
		return(ENGINE_NOT_STORED);
	}

	conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE,
				     IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_NOT_STORED);
	}

	input_cas = hash_item_get_cas(item);

	result = innodb_api_store(innodb_eng, conn_data, value + len - key_len,
				  key_len, val_len, exptime, cas, input_cas,
				  flags, op);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE,
				result == ENGINE_SUCCESS);
	return(result);
}

/*******************************************************************//**
Support memcached "INCR" and "DECR" command, add or subtract a "delta"
value from an integer key value
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_arithmetic(
/*==============*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	const void*	key,		/*!< in: key for the value to add */
	const int	nkey,		/*!< in: key length */
	const bool	increment,	/*!< in: whether to increment
					or decrement */
	const bool	create,		/*!< in: whether to create the key
					value pair if can't find */
	const uint64_t	delta,		/*!< in: value to add/substract */
	const uint64_t	initial,	/*!< in: initial */
	const rel_time_t exptime,	/*!< in: expiration time */
	uint64_t*	cas,		/*!< out: new cas value */
	uint64_t*	result,		/*!< out: result value */
	uint16_t	vbucket)	/*!< in: bucket, used by default
					engine only */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	ENGINE_ERROR_CODE	err_ret;

	if (meta_info->set_option == META_CACHE_OPT_DISABLE) {
		return(ENGINE_SUCCESS);
	}

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		/* For cache-only, forward this to the
		default engine */
		err_ret = def_eng->engine.arithmetic(
			innodb_eng->default_engine, cookie, key, nkey,
			increment, create, delta, initial, exptime, cas,
			result, vbucket);

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(err_ret);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE,
				     IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_NOT_STORED);
	}

	innodb_api_arithmetic(innodb_eng, conn_data, key, nkey, delta,
			      increment, cas, exptime, create, initial,
			      result);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE,
				true);

	return(ENGINE_SUCCESS);
}

/*******************************************************************//**
Cleanup idle connections if "clear_all" is false, and clean up all
connections if "clear_all" is true.
@return number of connection cleaned */
static
bool
innodb_flush_clean_conn(
/*====================*/
	innodb_engine_t*	engine,		/*!< in/out: InnoDB memcached
						engine */
	const void*		cookie)		/*!< in: connection cookie */
{
	innodb_conn_data_t*	conn_data = NULL;
	innodb_conn_data_t*	curr_conn_data;

	curr_conn_data = engine->server.cookie->get_engine_specific(cookie);
	assert(curr_conn_data);
	assert(!engine->enable_binlog || curr_conn_data->thd);

	conn_data = UT_LIST_GET_FIRST(engine->conn_data);

	while (conn_data) {
		if (conn_data != curr_conn_data && (!conn_data->is_stale)) {
			if (curr_conn_data->thd) {
				handler_thd_attach(conn_data->thd, NULL);
			}
			innodb_reset_conn(conn_data, false, true,
					  engine->enable_binlog);
		}
		conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
	}

	if (curr_conn_data->thd) {
		handler_thd_attach(curr_conn_data->thd, NULL);
	}
	return(true);
}

/*******************************************************************//**
Support memcached "FLUSH_ALL" command, clean up storage (trunate InnoDB Table)
@return ENGINE_SUCCESS if successfully, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_flush(
/*=========*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	time_t		when)		/*!< in: when to flush, not used by
					InnoDB */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);
	ENGINE_ERROR_CODE	err = ENGINE_SUCCESS;
	meta_cfg_info_t*	meta_info = innodb_eng->meta_info;
	ib_err_t		ib_err = DB_SUCCESS;
	innodb_conn_data_t*	conn_data;

	if (meta_info->flush_option == META_CACHE_OPT_DISABLE) {
		return(ENGINE_SUCCESS);
	}

	if (meta_info->flush_option == META_CACHE_OPT_DEFAULT
	    || meta_info->flush_option == META_CACHE_OPT_MIX) {
		/* default engine flush */
		err = def_eng->engine.flush(innodb_eng->default_engine,
					    cookie, when);

		if (meta_info->flush_option == META_CACHE_OPT_DEFAULT) {
			return(err);
		}
	}

	/* Lock the whole engine, so no other connection can start
	new opeartion */
        pthread_mutex_lock(&innodb_eng->conn_mutex);

	conn_data = innodb_eng->server.cookie->get_engine_specific(cookie);

	if (conn_data) {
		/* Commit any work on this connection */
		innodb_api_cursor_reset(innodb_eng, conn_data,
					CONN_OP_FLUSH, true);
	}

        conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE,
				     IB_LOCK_TABLE_X, true);

	if (!conn_data) {
		pthread_mutex_unlock(&innodb_eng->conn_mutex);
		return(ENGINE_SUCCESS);
	}

	innodb_flush_clean_conn(innodb_eng, cookie);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_FLUSH, true);
	meta_info = conn_data->conn_meta;

	ib_err = innodb_api_flush(innodb_eng, conn_data,
				  meta_info->col_info[CONTAINER_DB].col_name,
			          meta_info->col_info[CONTAINER_TABLE].col_name);

        pthread_mutex_unlock(&innodb_eng->conn_mutex);

	return((ib_err == DB_SUCCESS) ? ENGINE_SUCCESS : ENGINE_TMPFAIL);
}

/*******************************************************************//**
Deal with unknown command. Currently not used
@return ENGINE_SUCCESS if successfully processed, otherwise error code */
static
ENGINE_ERROR_CODE
innodb_unknown_command(
/*===================*/
	ENGINE_HANDLE*	handle,		/*!< in: Engine Handle */
	const void*	cookie,		/*!< in: connection cookie */
	protocol_binary_request_header *request, /*!< in: request */
	ADD_RESPONSE	response)	/*!< out: respondse */
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);

	return(def_eng->engine.unknown_command(innodb_eng->default_engine,
					       cookie, request, response));
}

/*******************************************************************//**
Callback functions used by Memcached's process_command() function
to get the result key/value information
@return true if info fetched */
static
bool
innodb_get_item_info(
/*=================*/
	ENGINE_HANDLE*		handle __attribute__((unused)),
						/*!< in: Engine Handle */
	const void*		cookie __attribute__((unused)),
						/*!< in: connection cookie */
	const item*		item,		/*!< in: item in question */
	item_info*		item_info)	/*!< out: item info got */
{
	hash_item*	it;

	if (item_info->nvalue < 1) {
		return(false);
	}

	/* Use a hash item */
	it = (hash_item*) item;
	item_info->cas = hash_item_get_cas(it);
	item_info->exptime = it->exptime;
	item_info->nbytes = it->nbytes;
	item_info->flags = it->flags;
	item_info->clsid = it->slabs_clsid;
	item_info->nkey = it->nkey;
	item_info->nvalue = 1;
	item_info->key = hash_item_get_key(it);
	item_info->value[0].iov_base = hash_item_get_data(it);
	item_info->value[0].iov_len = it->nbytes;
	return(true);
}
