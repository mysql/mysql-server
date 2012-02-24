/***********************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#include "innodb_engine.h"
#include "innodb_engine_private.h"
#include "innodb_api.h"
#include "hash_item_util.h"
#include "innodb_cb_api.h"

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
	ENGINE_ERROR_CODE	err;
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
	err = create_my_default_instance(interface, get_server_api,
				       &(innodb_eng->default_engine));

	if (err != ENGINE_SUCCESS) {
		free(innodb_eng);
		return(err);
	}

	innodb_eng->initialized = true;

	*handle = (ENGINE_HANDLE*) &innodb_eng->engine;

	return(ENGINE_SUCCESS);
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

/** static variables for creating MySQL "FAKE" THD and "TABLE" structures
for MDL locking */
static void*	mysql_thd;
static void*	mysql_table;

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
	int			api_cfg_status;

	my_eng_config = (eng_config_info_t*) config_str;

	/* Register the call back function */
	register_innodb_cb((void*) my_eng_config->cb_ptr);

	innodb_eng->read_batch_size = (my_eng_config->eng_read_batch_size
					? my_eng_config->eng_read_batch_size
					: CONN_NUM_READ_COMMIT);

	innodb_eng->write_batch_size = (my_eng_config->eng_write_batch_size
					? my_eng_config->eng_write_batch_size
					: CONN_NUM_WRITE_COMMIT);

	innodb_eng->enable_binlog = my_eng_config->eng_enable_binlog;

	api_cfg_status = innodb_cb_get_cfg();

	/* If binlog is not enabled by InnoDB memcached plugin, let's
	check whether innodb_direct_access_enable_binlog is turned on */
	if (!innodb_eng->enable_binlog) {
		innodb_eng->enable_binlog = api_cfg_status
					    & IB_CFG_BINLOG_ENABLED;
	}

	innodb_eng->enable_mdl = api_cfg_status & IB_CFG_MDL_ENABLED;

	/* Set the default write batch size to 1 if binlog is turned on */
	if (innodb_eng->enable_binlog) {
		innodb_eng->write_batch_size = 1;
	}

	innodb_eng->trx_level = ib_cb_cfg_trx_level();

	UT_LIST_INIT(innodb_eng->conn_data);
	pthread_mutex_init(&innodb_eng->conn_mutex, NULL);
	pthread_mutex_init(&innodb_eng->cas_mutex, NULL);

	/* Fetch InnoDB specific settings */
	if (!innodb_config(&innodb_eng->meta_info)) {
		return(ENGINE_TMPFAIL);
	}

	if (innodb_eng->default_engine) {
		return_status = def_eng->engine.initialize(
			innodb_eng->default_engine,
			my_eng_config->option_string);
	}

	if (return_status == ENGINE_SUCCESS && innodb_eng->enable_mdl) {
		mysql_thd = handler_create_thd(false);
		mysql_table = handler_open_table(
			mysql_thd,
			innodb_eng->meta_info.col_info[CONTAINER_DB].col_name,
			innodb_eng->meta_info.col_info[CONTAINER_TABLE].col_name,
			HDL_READ);
	}

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
	innodb_conn_data_t*	conn_data)
{
	if (!conn_data) {
		return;
	}

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
	}

	if (conn_data->mysql_tbl) {
		assert(conn_data->thd);
		handler_unlock_table(conn_data->thd,
				     conn_data->mysql_tbl,
				     HDL_READ);
	}

	if (conn_data->thd) {
		handler_close_thd(conn_data->thd);
		conn_data->thd = NULL;
	}

	free(conn_data);
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

	LOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
	conn_data = UT_LIST_GET_FIRST(engine->conn_data);

	while (conn_data) {
		bool	stale_data = false;
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
				stale_data = true;
			}
		}

		/* Either we are clearing all conn_data or this conn_data is
		not in use */
		if (clear_all || stale_data) {
			UT_LIST_REMOVE(conn_list, engine->conn_data, conn_data);

			innodb_conn_clean_data(conn_data);

			if (clear_all) {
				engine->server.cookie->store_engine_specific(
					cookie, NULL);
			}

			num_freed++;
		}

		conn_data = next_conn_data;
	}

	assert(!clear_all || engine->conn_data.count == 0);

	UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

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

	if (innodb_eng->enable_mdl && mysql_thd) {
		handler_unlock_table(mysql_thd, mysql_table, HDL_READ);
		handler_close_thd(mysql_thd);
	}

	innodb_conn_clean(innodb_eng, true, false);

	pthread_mutex_destroy(&innodb_eng->conn_mutex);
	pthread_mutex_destroy(&innodb_eng->cas_mutex);

	if (innodb_eng->default_engine) {
		def_eng->engine.destroy(innodb_eng->default_engine, force);
	}

	innodb_config_free(&innodb_eng->meta_info);

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

/******************************************************************//**
Function used to loop a thread (for debugging/instrumentation
purpose). */
static
void
srv_debug_loop(void)
/*================*/
{
        bool set = true;

        while (set) {
                pthread_yield();
        }
}

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
	bool			is_select,	/*!< in: Select only query */
	ib_lck_mode_t		lock_mode,	/*!< in: Table lock mode */
	bool			has_lock)	/*!< in: Has engine mutex */
{
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info = &engine->meta_info;
	meta_index_t*		meta_index = &meta_info->index_info;
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
	}

	assert(engine->conn_data.count > 0);
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
			return(NULL);
		}

		return(conn_data);
	}

	if (!is_select) {
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

				return(NULL);
			}

		} else if (!conn_data->crsr_trx) {

			/* There exists a cursor, just need update
			with a new transaction */
			conn_data->crsr_trx = innodb_cb_trx_begin(
				engine->trx_level);

			innodb_cb_cursor_new_trx(crsr, conn_data->crsr_trx);
			err = innodb_cb_cursor_lock(crsr, lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->crsr);
				conn_data->crsr = NULL;
				conn_data->crsr_trx = NULL;
				conn_data->in_use = false;
				return(NULL);
			}

			if (meta_index->srch_use_idx == META_USE_SECONDARY) {

				idx_crsr = conn_data->idx_crsr;
				innodb_cb_cursor_new_trx(
					idx_crsr, conn_data->crsr_trx);
				innodb_cb_cursor_lock(
					idx_crsr, lock_mode);
			}
		} else {
			err = innodb_cb_cursor_lock(crsr, lock_mode);

			if (err != DB_SUCCESS) {
				innodb_cb_cursor_close(
					conn_data->crsr);
				conn_data->crsr = NULL;
				conn_data->crsr_trx = NULL;
				conn_data->in_use = false;
				return(NULL);
			}
		}
		if (conn_data->read_crsr && trx_updated) {
			innodb_cb_cursor_new_trx(
				conn_data->read_crsr,
				conn_data->crsr_trx);
		}
			
	} else {
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

				return(NULL);
			}

			if (conn_data->crsr && trx_updated) {
				innodb_cb_cursor_new_trx(
					conn_data->crsr,
					conn_data->crsr_trx);
			}

		} else if (!conn_data->crsr_trx) {
			conn_data->crsr_trx = innodb_cb_trx_begin(
				engine->trx_level);

			innodb_cb_cursor_new_trx(
				conn_data->read_crsr,
				conn_data->crsr_trx);

			if (conn_data->crsr) {
				innodb_cb_cursor_new_trx(
					conn_data->crsr,
					conn_data->crsr_trx);
			}
				
			innodb_cb_cursor_lock(
				conn_data->read_crsr, lock_mode); 

			if (meta_index->srch_use_idx == META_USE_SECONDARY) {
				ib_crsr_t idx_crsr = conn_data->idx_read_crsr;

				innodb_cb_cursor_new_trx(
					idx_crsr, conn_data->crsr_trx);
				innodb_cb_cursor_lock(
					idx_crsr, lock_mode);
			}
		}
	}

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
	uint64_t		cas,		/*!< in: cas */
	uint16_t		vbucket)	/*!< in: bucket, used by default
						engine only */
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	struct default_engine*	def_eng = default_handle(innodb_eng);
	ENGINE_ERROR_CODE	err = ENGINE_SUCCESS;
	innodb_conn_data_t*	conn_data;
	meta_cfg_info_t*	meta_info = &innodb_eng->meta_info;

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		hash_item*	item = item_get(def_eng, key, nkey);

		if (item != NULL) {
			item_unlink(def_eng, item);
			item_release(def_eng, item);
		}

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(ENGINE_SUCCESS);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie,
				     false, IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_TMPFAIL);
	}

	/* In the binary protocol there is such a thing as a CAS delete.
	This is the CAS check. If we will also be deleting from the database,
	there are two possibilities:
	  1: The CAS matches; perform the delete.
	  2: The CAS doesn't match; delete the item because it's stale.
	Therefore we skip the check altogether if(do_db_delete) */

	err = innodb_api_delete(innodb_eng, conn_data, key, nkey);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_DELETE);

	return(err);
}


/*******************************************************************//**
Release the connection, free resource allocated in innodb_allocate */
static
void
innodb_release(
/*===========*/
	ENGINE_HANDLE*		handle,		/*!< in: Engine handle */
	const void*		cookie,		/*!< in: connection cookie */
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
	uint16_t		vbucket)	/*!< in: bucket, used by default
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
	meta_cfg_info_t*	meta_info = &innodb_eng->meta_info;

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		*item = item_get(default_handle(innodb_eng), key, nkey);

		if (*item != NULL) {
			return(ENGINE_SUCCESS);
		}

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(ENGINE_KEY_ENOENT);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie, true,
				     IB_LOCK_S, false);

	if (!conn_data) {
		return(ENGINE_TMPFAIL);
	}

	err = innodb_api_search(innodb_eng, conn_data, &crsr, key,
				nkey, &result, NULL, true);

	if (err != DB_SUCCESS) {
		err_ret = ENGINE_KEY_ENOENT;
		goto func_exit;
	}

	/* Only if expiration field is enabled, and the value is not zero,
	we will check whether the item is expired */
	if (result.col_value[MCI_COL_EXP].is_valid
	    && result.col_value[MCI_COL_EXP].value_int) {
		uint64_t		time;
		time = mci_get_time();

		if (time > result.col_value[MCI_COL_EXP].value_int) {
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
		for (i = 0; i < result.n_extra_col; i++) {

			total_len += (result.extra_col_value[i].value_len
				      + meta_info->sep_len);
		}

		/* No need to add the last separator */
		total_len -= meta_info->sep_len;
	} else {
		total_len = result.col_value[MCI_COL_VALUE].value_len;
	}

	innodb_allocate(handle, cookie, item, key, nkey, total_len, flags, exp);

        it = *item;

	if (it->iflag & ITEM_WITH_CAS) {
		hash_item_set_cas(it, cas);
	}

	if (result.extra_col_value) {
		int	i;
		char*	c_value = hash_item_get_data(it);

		for (i = 0; i < result.n_extra_col; i++) {

			if (result.extra_col_value[i].value_len != 0) {
				memcpy(c_value,
				       result.extra_col_value[i].value_str,
				       result.extra_col_value[i].value_len);

				c_value += result.extra_col_value[i].value_len;
			}

			memcpy(c_value, meta_info->separator,
			       meta_info->sep_len);
			c_value += meta_info->sep_len;
		}
	} else {
		assert(result.col_value[MCI_COL_VALUE].value_len >= it->nbytes);
		memcpy(hash_item_get_data(it),
		       result.col_value[MCI_COL_VALUE].value_str, it->nbytes);

		if (result.col_value[MCI_COL_VALUE].allocated) {
			free(result.col_value[MCI_COL_VALUE].value_str);
			result.col_value[MCI_COL_VALUE].allocated = false;
		}
	}

func_exit:

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_READ);

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
	uint16_t		vbucket)	/*!< in: bucket, used by default
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
	meta_cfg_info_t*	meta_info = &innodb_eng->meta_info;
	uint32_t		val_len = ((hash_item*)item)->nbytes;

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		result = store_item(default_handle(innodb_eng), item, cas,
				    op, cookie);

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(result);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie, false,
				     IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_NOT_STORED);
	}

	input_cas = hash_item_get_cas(item);

	result = innodb_api_store(innodb_eng, conn_data, value, len, val_len,
				  exptime, cas, input_cas, flags, op);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE);

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
	meta_cfg_info_t*	meta_info = &innodb_eng->meta_info;
	ENGINE_ERROR_CODE	err;

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		/* For cache-only, forward this to the
		default engine */
		err = def_eng->engine.arithmetic(
			innodb_eng->default_engine, cookie, key, nkey,
			increment, create, delta, initial, exptime, cas,
			result, vbucket);

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(err);
		}
	}

	conn_data = innodb_conn_init(innodb_eng, cookie, false,
				     IB_LOCK_X, false);

	if (!conn_data) {
		return(ENGINE_NOT_STORED);
	}

	innodb_api_arithmetic(innodb_eng, conn_data, key, nkey, delta,
			      increment, cas, exptime, create, initial,
			      result);

	innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE);

	return(ENGINE_SUCCESS);
}

/*******************************************************************//**
Cleanup idle connections if "clear_all" is false, and clean up all
connections if "clear_all" is true.
@return number of connection cleaned */
static
bool
innodb_flush_clean_conn(
/*===================*/
	innodb_engine_t*	engine,		/*!< in/out: InnoDB memcached
						engine */
	const void*		cookie)		/*!< in: connection cookie */
{
	innodb_conn_data_t*	conn_data;
	int			retry = 0;

	/* Clean up current connection */
	conn_data = engine->server.cookie->get_engine_specific(cookie);

	UT_LIST_REMOVE(conn_list, engine->conn_data, conn_data);
	innodb_conn_clean_data(conn_data);

	engine->server.cookie->store_engine_specific(cookie, NULL);

	while (conn_data = UT_LIST_GET_FIRST(engine->conn_data)
	       && retry < 100) {
		innodb_conn_clean(engine, false, true);
		retry++;
	}

	return(!UT_LIST_GET_FIRST(engine->conn_data));
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
	meta_cfg_info_t*	meta_info = &innodb_eng->meta_info;
	ib_err_t		ib_err = DB_SUCCESS;
	innodb_conn_data_t*	conn_data;

	if (meta_info->set_option == META_CACHE_OPT_DEFAULT
	    || meta_info->set_option == META_CACHE_OPT_MIX) {
		/* default engine flush */
		err = def_eng->engine.flush(innodb_eng->default_engine,
					    cookie, when);

		if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
			return(err);
		}
	}

        pthread_mutex_lock(&innodb_eng->conn_mutex);

	conn_data = innodb_eng->server.cookie->get_engine_specific(cookie);

	if (conn_data) {
		innodb_api_cursor_reset(innodb_eng, conn_data,
					CONN_OP_FLUSH);
	}

	innodb_conn_clean(innodb_eng, false, true);

        conn_data = innodb_conn_init(innodb_eng, cookie, false,
				     IB_LOCK_TABLE_X, true);


	if (!conn_data) {
		pthread_mutex_unlock(&innodb_eng->conn_mutex);
		return(ENGINE_SUCCESS);
	}

	/* Clean up sessions before doing flush. Table needs to be
	re-opened */
	if (!innodb_flush_clean_conn(innodb_eng, cookie)) {
		pthread_mutex_unlock(&innodb_eng->conn_mutex);
		return(ENGINE_SUCCESS);
	}

	ib_err = innodb_api_flush(innodb_eng,
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
	ENGINE_HANDLE*		handle,		/*!< in: Engine Handle */
	const void*		cookie,		/*!< in: connection cookie */
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
