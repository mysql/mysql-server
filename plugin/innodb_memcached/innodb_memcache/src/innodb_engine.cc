/***********************************************************************

Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**************************************************/ /**
 @file
 InnoDB Memcached Engine code

 Extracted and modified from NDB memcached project
 04/12/2011 Jimmy Yang
 *******************************************************/

// Work around a bug in the memcached C++ headers with GCC 4.x.
#ifndef bool
#define bool bool
#endif

#include <arpa/inet.h>
#include <assert.h>
#include <memcached/config_parser.h>
#include <memcached/util.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "default_engine.h"

#include "hash_item_util.h"
#include "innodb_api.h"
#include "innodb_cb_api.h"
#include "innodb_engine.h"
#include "innodb_engine_private.h"
#include "my_compiler.h"
#include "my_thread.h"

/** Define also present in daemon/memcached.h */
#define KEY_MAX_LENGTH 250

/** Time (in seconds) that background thread sleeps before it wakes
up and commit idle connection transactions */
#define BK_COMMIT_THREAD_SLEEP_INTERVAL 5

/** Maximum number of connections that background thread processes each
time */
#define BK_MAX_PROCESS_COMMIT 5

/** Minimum time (in seconds) that a connection has been idle, that makes
it candidate for background thread to commit it */
#define CONN_IDLE_TIME_TO_BK_COMMIT 5

#ifdef UNIV_MEMCACHED_SDI
static const char SDI_PREFIX[] = "sdi_";
static const char SDI_CREATE_PREFIX[] = "sdi_create_";
static const char SDI_DROP_PREFIX[] = "sdi_drop_";
static const char SDI_LIST_PREFIX[] = "sdi_list_";
#endif /* UNIV_MEMCACHED_SDI */

/** Tells whether memcached plugin is being shutdown */
static bool memcached_shutdown = false;

/** Tells whether the background thread is exited */
static bool bk_thd_exited = true;

/** The SDI buffer length for storing list of SDI keys. Example output
looks like "1:2|2:2|3:4|..". So SDI list of key retrieval has this limit of
characters from memcached plugin. This is sufficent for testing. */
const uint32_t SDI_LIST_BUF_MAX_LEN = 10000;

/** Tells whether all connections need to release MDL locks */
bool release_mdl_lock = false;

/** InnoDB Memcached engine configuration info */
typedef struct eng_config_info {
  char *option_string;               /*!< memcached config option
                                     string */
  void *cb_ptr;                      /*!< call back function ptr */
  unsigned int eng_read_batch_size;  /*!< read batch size */
  unsigned int eng_write_batch_size; /*!< write batch size */
  bool eng_enable_binlog;            /*!< whether binlog is
                                     enabled specifically for
                                     this memcached engine */
} eng_config_info_t;

extern option_t config_option_names[];

/** Check the input key name implies a table mapping switch. The name
would start with "@@", and in the format of "@@new_table_mapping.key"
or simply "@@new_table_mapping" */

/**********************************************************************/ /**
 Unlock a table and commit the transaction
 return 0 if fail to commit the transaction */
extern int handler_unlock_table(
    /*=================*/
    void *my_thd,      /*!< in: thread */
    void *my_table,    /*!< in: Table metadata */
    int my_lock_mode); /*!< in: lock mode */

/*******************************************************************/ /**
 Get InnoDB Memcached engine handle
 @return InnoDB Memcached engine handle */
static inline struct innodb_engine *innodb_handle(
    /*==========*/
    ENGINE_HANDLE *handle) /*!< in: Generic engine handle */
{
  return ((struct innodb_engine *)handle);
}

/*******************************************************************/ /**
 Cleanup idle connections if "clear_all" is false, and clean up all
 connections if "clear_all" is true.
 @return number of connection cleaned */
static void innodb_conn_clean_data(
    /*===================*/
    innodb_conn_data_t *conn_data, bool has_lock, bool free_all);

/*******************************************************************/ /**
 Destroy and Free InnoDB Memcached engine */
static void innodb_destroy(
    /*===========*/
    ENGINE_HANDLE *handle, /*!< in: Destroy the engine instance */
    bool force);           /*!< in: Force to destroy */

/*******************************************************************/ /**
 Support memcached "INCR" and "DECR" command, add or subtract a "delta"
 value from an integer key value
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_arithmetic(
    /*==============*/
    ENGINE_HANDLE *handle,    /*!< in: Engine Handle */
    const void *cookie,       /*!< in: connection cookie */
    const void *key,          /*!< in: key for the value to add */
    const int nkey,           /*!< in: key length */
    const bool increment,     /*!< in: whether to increment
                              or decrement */
    const bool create,        /*!< in: whether to create the key
                              value pair if can't find */
    const uint64_t delta,     /*!< in: value to add/substract */
    const uint64_t initial,   /*!< in: initial */
    const rel_time_t exptime, /*!< in: expiration time */
    uint64_t *cas,            /*!< out: new cas value */
    uint64_t *result,         /*!< out: result out */
    uint16_t vbucket);        /*!< in: bucket, used by default
                              engine only */

/*******************************************************************/ /**
 Callback functions used by Memcached's process_command() function
 to get the result key/value information
 @return TRUE if info fetched */
static bool innodb_get_item_info(
    /*=================*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie,    /*!< in: connection cookie */
    const item *item,      /*!< in: item in question */
    item_info *item_info); /*!< out: item info got */

/*******************************************************************/ /**
 Get default Memcached engine handle
 @return default Memcached engine handle */
static inline struct default_engine *default_handle(
    /*===========*/
    struct innodb_engine *eng) {
  return ((struct default_engine *)eng->default_engine);
}

#ifdef UNIV_MEMCACHED_SDI
/** Remove SDI entry from tablespace
@param[in,out]	innodb_eng	innodb engine structure
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	key		memcached key
@param[in]	nkey		memcached key length
@return true if key is SDI key else false */
static bool innodb_sdi_remove(struct innodb_engine *innodb_eng,
                              innodb_conn_data_t *conn_data,
                              ENGINE_ERROR_CODE *err_ret, const void *key,
                              const size_t nkey);

/** Retrieve SDI for a given SDI key from tablespace
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	key		memcached key
@param[in]	nkey		memcached key length
@param[in,out]	item		memcached item to fill
@return true if key is SDI key else false */
static bool innodb_sdi_get(innodb_conn_data_t *conn_data,
                           ENGINE_ERROR_CODE *err_ret, const void *key,
                           const size_t nkey, item ***item);

/** Store SDI entry into a tablespace
@param[in,out]	innodb_eng	innodb engine structure
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	value		memcached value
@param[in]	value_len	memcached value length
@param[in]	nkey		memcached key length
@return true if key is SDI key else false */
static bool innodb_sdi_store(struct innodb_engine *innodb_eng,
                             innodb_conn_data_t *conn_data,
                             ENGINE_ERROR_CODE *err_ret, char *value,
                             uint32_t val_len, const size_t nkey);
#endif /* UNIV_MEMCACHED_SDI */

/****** Gateway to the default_engine's create_instance() function */
extern "C" ENGINE_ERROR_CODE create_my_default_instance(
    /*=======================*/
    uint64_t, GET_SERVER_API, ENGINE_HANDLE **);

/*********** FUNCTIONS IMPLEMENTING THE PUBLISHED API BEGIN HERE ********/

/*******************************************************************/ /**
 Create InnoDB Memcached Engine.
 @return ENGINE_SUCCESS if successful, otherwise, error code */
ENGINE_ERROR_CODE
create_instance(
    /*============*/
    uint64_t interface,            /*!< in: protocol version,
                                   currently always 1 */
    GET_SERVER_API get_server_api, /*!< in: Callback the engines
                                   may call to get the public
                                   server interface */
    ENGINE_HANDLE **handle)        /*!< out: Engine handle */
{
  ENGINE_ERROR_CODE err_ret;
  struct innodb_engine *innodb_eng;

  SERVER_HANDLE_V1 *api = get_server_api();

  if (interface != 1 || api == NULL) {
    return (ENGINE_ENOTSUP);
  }

  innodb_eng = (innodb_engine *)malloc(sizeof(struct innodb_engine));

  if (innodb_eng == NULL) {
    return (ENGINE_ENOMEM);
  }

  memset(innodb_eng, 0, sizeof(*innodb_eng));
  innodb_eng->engine.interface.interface = 1;
  innodb_eng->engine.get_info = innodb_get_info;
  innodb_eng->engine.initialize = innodb_initialize;
  innodb_eng->engine.destroy = innodb_destroy;
  innodb_eng->engine.allocate = innodb_allocate;
  innodb_eng->engine.remove = innodb_remove;
  innodb_eng->engine.release = innodb_release;
  innodb_eng->engine.clean_engine = innodb_clean_engine;
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
  innodb_eng->info.info.features[1].feature = ENGINE_FEATURE_PERSISTENT_STORAGE;
  innodb_eng->info.info.features[2].feature = ENGINE_FEATURE_LRU;

  /* Now call create_instace() for the default engine */
  err_ret = create_my_default_instance(interface, get_server_api,
                                       &(innodb_eng->default_engine));

  if (err_ret != ENGINE_SUCCESS) {
    free(innodb_eng);
    return (err_ret);
  }

  innodb_eng->clean_stale_conn = false;
  innodb_eng->initialized = true;

  *handle = (ENGINE_HANDLE *)&innodb_eng->engine;

  return (ENGINE_SUCCESS);
}

/*******************************************************************/ /**
 background thread to commit trx.
 @return dummy parameter */
static void *innodb_bk_thread(
    /*=============*/
    void *arg) {
  ENGINE_HANDLE *handle;
  struct innodb_engine *innodb_eng;
  innodb_conn_data_t *conn_data;

  bk_thd_exited = false;

  handle = (ENGINE_HANDLE *)(arg);
  innodb_eng = innodb_handle(handle);

  my_thread_init();

  /* While we commit transactions on behalf of the other
  threads, we will "pretend" to be each connection. */
  void *thd = handler_create_thd(innodb_eng->enable_binlog);

  conn_data = UT_LIST_GET_FIRST(innodb_eng->conn_data);

  while (!memcached_shutdown) {
    innodb_conn_data_t *next_conn_data;
    uint64_t time;
    uint64_t trx_start = 0;
    uint64_t processed_count = 0;

    if (handler_check_global_read_lock_active()) {
      release_mdl_lock = true;
    } else {
      release_mdl_lock = false;
    }

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

    if (!conn_data) {
      conn_data = UT_LIST_GET_FIRST(innodb_eng->conn_data);
    }

    if (conn_data) {
      next_conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
    } else {
      next_conn_data = NULL;
    }

    /* Set the clean_stale_conn to prevent force clean in
    innodb_conn_clean. */
    LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
    innodb_eng->clean_stale_conn = true;
    UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);

    while (conn_data) {
      if (release_mdl_lock && !conn_data->is_stale) {
        int err;

        if (conn_data->is_waiting_for_mdl) {
          goto next_item;
        }

        err = LOCK_CURRENT_CONN_TRYLOCK(conn_data);
        if (err != 0) {
          goto next_item;
        }
        /* We have got the lock here */
      } else {
        LOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
      }

      if (conn_data->is_stale) {
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
        LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
        UT_LIST_REMOVE(conn_list, innodb_eng->conn_data, conn_data);
        UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
        innodb_conn_clean_data(conn_data, false, true);
        goto next_item;
      }

      if (release_mdl_lock) {
        if (conn_data->thd) {
          handler_thd_attach(conn_data->thd, NULL);
        }

        if (conn_data->in_use) {
          UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
          goto next_item;
        }

        innodb_reset_conn(conn_data, true, true, innodb_eng->enable_binlog);
        if (conn_data->mysql_tbl) {
          handler_unlock_table(conn_data->thd, conn_data->mysql_tbl, HDL_READ);
          conn_data->mysql_tbl = NULL;
        }

        /*Close the data cursor */
        if (conn_data->crsr) {
          innodb_cb_cursor_close(conn_data->crsr);
          conn_data->crsr = NULL;
        }
        if (conn_data->crsr_trx != NULL) {
          ib_cb_trx_release(conn_data->crsr_trx);
          conn_data->crsr_trx = NULL;
        }

        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
        goto next_item;
      }

      if (conn_data->crsr_trx) {
        trx_start = ib_cb_trx_get_start_time(conn_data->crsr_trx);
      }

      /* Check the trx, if it is qualified for
      reset and commit */
      if ((conn_data->n_writes_since_commit > 0 ||
           conn_data->n_reads_since_commit > 0) &&
          trx_start && (time - trx_start > CONN_IDLE_TIME_TO_BK_COMMIT) &&
          !conn_data->in_use) {
        /* binlog is running, make the thread
        attach to conn_data->thd for binlog
        committing */
        if (thd && conn_data->thd) {
          handler_thd_attach(conn_data->thd, NULL);
        }

        innodb_reset_conn(conn_data, true, true, innodb_eng->enable_binlog);
        processed_count++;
      }

      UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);

    next_item:
      conn_data = next_conn_data;

      /* Process BK_MAX_PROCESS_COMMIT (5) trx at a time */
      if (!release_mdl_lock && processed_count > BK_MAX_PROCESS_COMMIT) {
        break;
      }

      if (conn_data) {
        next_conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
      }
    }

    /* Set the clean_stale_conn back. */
    LOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
    innodb_eng->clean_stale_conn = false;
    UNLOCK_CONN_IF_NOT_LOCKED(false, innodb_eng);
  }

  bk_thd_exited = true;

  /* Change to its original state before close the MySQL THD */
  handler_thd_attach(thd, NULL);
  handler_close_thd(thd);

  my_thread_end();
  pthread_detach(pthread_self());
  pthread_exit(NULL);

  return ((void *)0);
}

/*******************************************************************/ /**
 Get engine info.
 @return engine info */
static const engine_info *innodb_get_info(
    /*============*/
    ENGINE_HANDLE *handle) /*!< in: Engine handle */
{
  return (&innodb_handle(handle)->info.info);
}

/*******************************************************************/ /**
 Initialize InnoDB Memcached Engine.
 @return ENGINE_SUCCESS if successful */
static ENGINE_ERROR_CODE innodb_initialize(
    /*==============*/
    ENGINE_HANDLE *handle,  /*!< in/out: InnoDB memcached
                            engine */
    const char *config_str) /*!< in: configure string */
{
  ENGINE_ERROR_CODE return_status = ENGINE_SUCCESS;
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  eng_config_info_t *my_eng_config;
  pthread_attr_t attr;

  my_eng_config = (eng_config_info_t *)config_str;

  /* If no call back function registered (InnoDB engine failed to load),
  load InnoDB Memcached engine should fail too */
  if (!my_eng_config->cb_ptr) {
    return (ENGINE_TMPFAIL);
  }

  /* Register the call back function */
  register_innodb_cb((void *)my_eng_config->cb_ptr);

  innodb_eng->read_batch_size =
      (my_eng_config->eng_read_batch_size ? my_eng_config->eng_read_batch_size
                                          : CONN_NUM_READ_COMMIT);

  innodb_eng->write_batch_size =
      (my_eng_config->eng_write_batch_size ? my_eng_config->eng_write_batch_size
                                           : CONN_NUM_WRITE_COMMIT);

  innodb_eng->enable_binlog = my_eng_config->eng_enable_binlog;

  innodb_eng->cfg_status = innodb_cb_get_cfg();

  /* If binlog is not enabled by InnoDB memcached plugin, let's
  check whether innodb_direct_access_enable_binlog is turned on */
  if (!innodb_eng->enable_binlog) {
    innodb_eng->enable_binlog = innodb_eng->cfg_status & IB_CFG_BINLOG_ENABLED;
  }

  innodb_eng->enable_mdl = innodb_eng->cfg_status & IB_CFG_MDL_ENABLED;
  innodb_eng->trx_level = ib_cb_cfg_trx_level();
  innodb_eng->bk_commit_interval = ib_cb_cfg_bk_commit_interval();

  UT_LIST_INIT(innodb_eng->conn_data);
  pthread_mutex_init(&innodb_eng->conn_mutex, NULL);
  pthread_mutex_init(&innodb_eng->cas_mutex, NULL);
  pthread_mutex_init(&innodb_eng->flush_mutex, NULL);

  /* Fetch InnoDB specific settings */
  innodb_eng->meta_info = innodb_config(NULL, 0, &innodb_eng->meta_hash);

  if (!innodb_eng->meta_info) {
    return (ENGINE_TMPFAIL);
  }

  if (innodb_eng->default_engine) {
    return_status = def_eng->engine.initialize(innodb_eng->default_engine,
                                               my_eng_config->option_string);
  }

  memcached_shutdown = false;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&innodb_eng->bk_thd_for_commit, &attr, innodb_bk_thread,
                 handle);

  return (return_status);
}

extern void handler_close_thd(void *);

/*******************************************************************/ /**
 Close table using handler functions.
 @param conn_data	cursor information of connection */
void innodb_close_mysql_table(
    /*=====================*/
    innodb_conn_data_t *conn_data) /*!< in: connection
                                            cursor*/
{
  if (conn_data->mysql_tbl) {
    assert(conn_data->thd);
    handler_unlock_table(conn_data->thd, conn_data->mysql_tbl, HDL_READ);
    conn_data->mysql_tbl = NULL;
  }
}

#define NUM_MAX_MEM_SLOT 1024

/*******************************************************************/ /**
 Cleanup idle connections if "clear_all" is false, and clean up all
 connections if "clear_all" is true.
 @return number of connection cleaned */
static void innodb_conn_clean_data(
    /*===================*/
    innodb_conn_data_t *conn_data, bool has_lock, bool free_all) {
  mem_buf_t *mem_buf;

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
    ib_err_t err MY_ATTRIBUTE((unused));
    innodb_cb_trx_commit(conn_data->crsr_trx);
    err = ib_cb_trx_release(conn_data->crsr_trx);
    assert(err == DB_SUCCESS);
    conn_data->crsr_trx = NULL;
  }

  innodb_close_mysql_table(conn_data);

  if (conn_data->tpl) {
    ib_cb_tuple_delete(conn_data->tpl);
    conn_data->tpl = NULL;
  }

  if (conn_data->idx_tpl) {
    ib_cb_tuple_delete(conn_data->idx_tpl);
    conn_data->idx_tpl = NULL;
  }

  if (conn_data->read_tpl) {
    ib_cb_tuple_delete(conn_data->read_tpl);
    conn_data->read_tpl = NULL;
  }

  if (conn_data->sel_tpl) {
    ib_cb_tuple_delete(conn_data->sel_tpl);
    conn_data->sel_tpl = NULL;
  }

  UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

  if (free_all) {
    if (conn_data->thd) {
      handler_close_thd(conn_data->thd);
      conn_data->thd = NULL;
    }

    conn_data->is_stale = false;

    if (conn_data->result) {
      free(conn_data->result);
      conn_data->result = NULL;
    }

    if (conn_data->row_buf) {
      for (int i = 0; i < NUM_MAX_MEM_SLOT; i++) {
        if (conn_data->row_buf[i]) {
          free(conn_data->row_buf[i]);
          conn_data->row_buf[i] = NULL;
        }
      }

      free(conn_data->row_buf);
      conn_data->row_buf = NULL;
      conn_data->row_buf_slot = 0;
    }
#ifdef UNIV_MEMCACHED_SDI
    free(conn_data->sdi_buf);
    conn_data->sdi_buf = NULL;
#endif /* UNIV_MEMCACHED_SDI */

    if (conn_data->cmd_buf) {
      free(conn_data->cmd_buf);
      conn_data->cmd_buf = NULL;
      conn_data->cmd_buf_len = 0;
    }

    if (conn_data->mul_col_buf) {
      free(conn_data->mul_col_buf);
      conn_data->mul_col_buf = NULL;
      conn_data->mul_col_buf_len = 0;
    }

    mem_buf = UT_LIST_GET_FIRST(conn_data->mul_used_buf);

    while (mem_buf) {
      UT_LIST_REMOVE(mem_list, conn_data->mul_used_buf, mem_buf);
      free(mem_buf->mem);
      mem_buf = UT_LIST_GET_FIRST(conn_data->mul_used_buf);
    }

    pthread_mutex_destroy(&conn_data->curr_conn_mutex);
    free(conn_data);
  }
}

/*******************************************************************/ /**
 Cleanup idle connections if "clear_all" is false, and clean up all
 connections if "clear_all" is true.
 @return number of connection cleaned */
static int innodb_conn_clean(
    /*==============*/
    innodb_engine_t *engine, /*!< in/out: InnoDB memcached
                             engine */
    bool clear_all,          /*!< in: Clear all connection */
    bool has_lock)           /*!< in: Has engine mutext */
{
  innodb_conn_data_t *conn_data;
  innodb_conn_data_t *next_conn_data;
  int num_freed = 0;
  void *thd = NULL;

  if (clear_all) {
    my_thread_init();
    thd = handler_create_thd(engine->enable_binlog);
  }

  LOCK_CONN_IF_NOT_LOCKED(has_lock, engine);

  conn_data = UT_LIST_GET_FIRST(engine->conn_data);

  while (conn_data) {
    void *cookie = conn_data->conn_cookie;

    next_conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);

    if (!clear_all && !conn_data->in_use) {
      innodb_conn_data_t *check_data;
      check_data =
          (innodb_conn_data_t *)engine->server.cookie->get_engine_specific(
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
      if (engine->clean_stale_conn) break;

      UT_LIST_REMOVE(conn_list, engine->conn_data, conn_data);
      innodb_conn_clean_data(conn_data, false, true);
      num_freed++;
    } else {
      if (clear_all) {
        UT_LIST_REMOVE(conn_list, engine->conn_data, conn_data);

        if (thd && conn_data->thd) {
          handler_thd_attach(conn_data->thd, NULL);
        }

        innodb_reset_conn(conn_data, false, true, engine->enable_binlog);
        if (conn_data->thd) {
          handler_thd_attach(conn_data->thd, NULL);
        }
        innodb_conn_clean_data(conn_data, false, true);

        engine->server.cookie->store_engine_specific(cookie, NULL);
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
    my_thread_end();
  }

  return (num_freed);
}

/*******************************************************************/ /**
 Destroy and Free InnoDB Memcached engine */
static void innodb_destroy(
    /*===========*/
    ENGINE_HANDLE *handle, /*!< in: Destroy the engine instance */
    bool force)            /*!< in: Force to destroy */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);

  memcached_shutdown = true;

  /* Wait for the background thread to exit */
  while (!bk_thd_exited) {
    sleep(1);
  }

  innodb_conn_clean(innodb_eng, true, false);

  if (innodb_eng->meta_hash) {
    HASH_CLEANUP(innodb_eng->meta_hash, meta_cfg_info_t *);
  }

  pthread_mutex_destroy(&innodb_eng->conn_mutex);
  pthread_mutex_destroy(&innodb_eng->cas_mutex);
  pthread_mutex_destroy(&innodb_eng->flush_mutex);

  if (innodb_eng->default_engine) {
    def_eng->engine.destroy(innodb_eng->default_engine, force);
  }

  free(innodb_eng);
}

/** Defines for connection initialization to indicate if we will
do a read or write operation, or in the case of CONN_MODE_NONE, just get
the connection's conn_data structure */
enum conn_mode { CONN_MODE_READ, CONN_MODE_WRITE, CONN_MODE_NONE };

/*******************************************************************/ /**
 Opens mysql table if enable_binlog or enable_mdl is set
 @param conn_data	connection cursor data
 @param conn_option	read or write operation
 @param engine		Innodb memcached engine
 @returns DB_SUCCESS on success and DB_ERROR on failure */
ib_err_t innodb_open_mysql_table(
    /*====================*/
    innodb_conn_data_t *conn_data, /*!< in/out:Connection cursor data */
    int conn_option,               /*!< in: Read or write operation */
    innodb_engine_t *engine)       /*!< in: InnoDB memcached engine */
{
  meta_cfg_info_t *meta_info;
  meta_info = conn_data->conn_meta;
  conn_data->is_waiting_for_mdl = true;

  /* Close the table before opening it again */
  innodb_close_mysql_table(conn_data);

  if (conn_option == CONN_MODE_READ) {
    conn_data->is_waiting_for_mdl = false;
    return (DB_SUCCESS);
  }

  if (!conn_data->thd) {
    conn_data->thd = handler_create_thd(engine->enable_binlog);
    if (!conn_data->thd) {
      return (DB_ERROR);
    }
  }

  if (!conn_data->mysql_tbl) {
    conn_data->mysql_tbl = handler_open_table(
        conn_data->thd, meta_info->col_info[CONTAINER_DB].col_name,
        meta_info->col_info[CONTAINER_TABLE].col_name, HDL_WRITE);
  }
  conn_data->is_waiting_for_mdl = false;

  if (!conn_data->mysql_tbl) {
    return (DB_LOCK_WAIT);
  }

  return (DB_SUCCESS);
}

/*******************************************************************/ /**
 Cleanup connections
 @return number of connection cleaned */
/* Initialize a connection's cursor and transactions
@return the connection's conn_data structure */
static innodb_conn_data_t *innodb_conn_init(
    /*=============*/
    innodb_engine_t *engine,        /*!< in/out: InnoDB memcached
                                    engine */
    const void *cookie,             /*!< in: This connection's
                                    cookie */
    int conn_option,                /*!< in: whether it is
                                    for read or write operation*/
    ib_lck_mode_t lock_mode,        /*!< in: Table lock mode */
    bool has_lock,                  /*!< in: Has engine mutex */
    meta_cfg_info_t *new_meta_info) /*!< in: meta info for
                                    table to open or NULL */
{
  innodb_conn_data_t *conn_data;
  meta_cfg_info_t *meta_info;
  meta_index_t *meta_index;
  ib_err_t err = DB_SUCCESS;
  ib_crsr_t crsr;
  ib_crsr_t read_crsr;
  ib_crsr_t idx_crsr;
  bool trx_updated = false;

  /* Get this connection's conn_data */
  conn_data =
      (innodb_conn_data_t *)engine->server.cookie->get_engine_specific(cookie);

  assert(!conn_data || !conn_data->in_use || conn_data->range ||
         conn_data->multi_get);

  if (!conn_data) {
    LOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
    conn_data =
        (innodb_conn_data_t *)engine->server.cookie->get_engine_specific(
            cookie);

    if (conn_data) {
      UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
      goto have_conn;
    }

    if (UT_LIST_GET_LEN(engine->conn_data) > 2048) {
      /* Some of conn_data can be stale, recycle them */
      innodb_conn_clean(engine, false, true);
    }

    conn_data = (innodb_conn_data_t *)malloc(sizeof(*conn_data));

    if (!conn_data) {
      UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
      return (NULL);
    }

    memset(conn_data, 0, sizeof(*conn_data));
    conn_data->result = malloc(sizeof(mci_item_t));
    if (!conn_data->result) {
      UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
      free(conn_data);
      conn_data = NULL;
      return (NULL);
    }
    conn_data->conn_meta = new_meta_info ? new_meta_info : engine->meta_info;

    /* FIX_ME: to make this dynamic extensible */
    conn_data->row_buf = (void **)malloc(NUM_MAX_MEM_SLOT * sizeof(void *));
    memset(conn_data->row_buf, 0, NUM_MAX_MEM_SLOT * sizeof(void *));

    conn_data->row_buf[0] = (void *)malloc(16384);

    if (conn_data->row_buf[0] == NULL) {
      UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
      free(conn_data->result);
      free(conn_data);
      conn_data = NULL;
      return (NULL);
    }

    conn_data->row_buf_slot = 0;

    conn_data->cmd_buf = malloc(1024);
    if (!conn_data->cmd_buf) {
      UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
      free(conn_data->row_buf);
      free(conn_data->result);
      free(conn_data);
      conn_data = NULL;
      return (NULL);
    }
    conn_data->cmd_buf_len = 1024;

    conn_data->thd = handler_create_thd(engine->enable_binlog);
#ifdef UNIV_MEMCACHED_SDI
    conn_data->sdi_buf = NULL;
#endif /* UNIV_MEMCACHED_SDI */
    conn_data->is_flushing = false;

    conn_data->conn_cookie = (void *)cookie;

    /* Add connection to the list after all memory allocations */
    UT_LIST_ADD_LAST(conn_list, engine->conn_data, conn_data);
    engine->server.cookie->store_engine_specific(cookie, conn_data);

    pthread_mutex_init(&conn_data->curr_conn_mutex, NULL);
    UT_LIST_INIT(conn_data->mul_used_buf);
    UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine);
  }
have_conn:
  if (memcached_shutdown) {
    return (NULL);
  }

  meta_info = conn_data->conn_meta;
  meta_index = &meta_info->index_info;

  assert(engine->conn_data.count > 0);

  if (conn_option == CONN_MODE_NONE) {
    return (conn_data);
  }

  /* If this is a range search or multi-key search, we do not
  need to reset search cursor, continue with the one being used */
  if (conn_data->range || conn_data->multi_get) {
    return (conn_data);
  }

  LOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

  /* If flush is running, then wait for it complete. */
  if (conn_data->is_flushing) {
    /* Request flush_mutex for waiting for flush completed. */
    pthread_mutex_lock(&engine->flush_mutex);
    pthread_mutex_unlock(&engine->flush_mutex);
  }

  /* This special case added to facilitate unlocking
     of MDL lock during FLUSH TABLE WITH READ LOCK */
  if (engine && release_mdl_lock &&
      (engine->enable_binlog || engine->enable_mdl)) {
    if (DB_SUCCESS != innodb_open_mysql_table(conn_data, conn_option, engine)) {
      UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
      return NULL;
    }
  }

  conn_data->in_use = true;

  crsr = conn_data->crsr;
  read_crsr = conn_data->read_crsr;

  if (lock_mode == IB_LOCK_TABLE_X) {
    if (!conn_data->crsr_trx) {
      conn_data->crsr_trx =
          ib_cb_trx_begin(engine->trx_level, true, false, conn_data->thd);
    } else {
      /* Write cursor transaction exists.
         Reuse this transaction.*/
      if (ib_cb_trx_read_only(conn_data->crsr_trx)) {
        innodb_cb_trx_commit(conn_data->crsr_trx);
      }

      err = ib_cb_trx_start(conn_data->crsr_trx, engine->trx_level, true, false,
                            conn_data->thd);
      assert(err == DB_SUCCESS);
    }

    err = innodb_api_begin(engine, meta_info->col_info[CONTAINER_DB].col_name,
                           meta_info->col_info[CONTAINER_TABLE].col_name,
                           conn_data, conn_data->crsr_trx, &conn_data->crsr,
                           &conn_data->idx_crsr, lock_mode);

    if (err != DB_SUCCESS) {
      innodb_cb_cursor_close(conn_data->crsr);
      conn_data->crsr = NULL;
      innodb_cb_trx_commit(conn_data->crsr_trx);
      err = ib_cb_trx_release(conn_data->crsr_trx);
      assert(err == DB_SUCCESS);
      conn_data->crsr_trx = NULL;
      conn_data->in_use = false;
      UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
      return (NULL);
    }

    UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
    return (conn_data);
  }

  /* Write operation */
  if (conn_option == CONN_MODE_WRITE) {
    if (!crsr) {
      if (!conn_data->crsr_trx) {
        conn_data->crsr_trx =
            ib_cb_trx_begin(engine->trx_level, true, false, conn_data->thd);
        trx_updated = true;
      } else {
        if (ib_cb_trx_read_only(conn_data->crsr_trx)) {
          innodb_cb_trx_commit(conn_data->crsr_trx);
        }

        ib_cb_trx_start(conn_data->crsr_trx, engine->trx_level, true, false,
                        conn_data->thd);
      }

      err = innodb_api_begin(engine, meta_info->col_info[CONTAINER_DB].col_name,
                             meta_info->col_info[CONTAINER_TABLE].col_name,
                             conn_data, conn_data->crsr_trx, &conn_data->crsr,
                             &conn_data->idx_crsr, lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->crsr);
        conn_data->crsr = NULL;
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->in_use = false;

        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
        return (NULL);
      }

    } else if (!conn_data->crsr_trx) {
      /* There exists a cursor, just need update
      with a new transaction */
      conn_data->crsr_trx =
          ib_cb_trx_begin(engine->trx_level, true, false, conn_data->thd);

      innodb_cb_cursor_new_trx(crsr, conn_data->crsr_trx);
      trx_updated = true;

      err = innodb_cb_cursor_lock(engine, crsr, lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->crsr);
        conn_data->crsr = NULL;
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->in_use = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
        return (NULL);
      }

      if (meta_index->srch_use_idx == META_USE_SECONDARY) {
        idx_crsr = conn_data->idx_crsr;
        innodb_cb_cursor_new_trx(idx_crsr, conn_data->crsr_trx);
        innodb_cb_cursor_lock(engine, idx_crsr, lock_mode);
      }
    } else {
      if (ib_cb_trx_read_only(conn_data->crsr_trx)) {
        innodb_cb_trx_commit(conn_data->crsr_trx);
      }

      ib_cb_trx_start(conn_data->crsr_trx, engine->trx_level, true, false,
                      conn_data->thd);
      ib_cb_cursor_stmt_begin(crsr);
      err = innodb_cb_cursor_lock(engine, crsr, lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->crsr);
        conn_data->crsr = NULL;
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->in_use = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
        return (NULL);
      }
    }

    if (trx_updated) {
      if (conn_data->read_crsr) {
        innodb_cb_cursor_new_trx(conn_data->read_crsr, conn_data->crsr_trx);
      }

      if (conn_data->idx_read_crsr) {
        innodb_cb_cursor_new_trx(conn_data->idx_read_crsr, conn_data->crsr_trx);
      }
    }
  } else {
    bool auto_commit = (engine->read_batch_size == 1 &&
                        !(engine->cfg_status & IB_CFG_DISABLE_ROWLOCK))
                           ? true
                           : false;
    assert(conn_option == CONN_MODE_READ);

    if (!read_crsr) {
      if (!conn_data->crsr_trx) {
        /* This is read operation, start a trx
        with "read_write" parameter set to false */
        conn_data->crsr_trx = ib_cb_trx_begin(engine->trx_level, false,
                                              auto_commit, conn_data->thd);
        trx_updated = true;
      } else {
        ib_cb_trx_start(conn_data->crsr_trx, engine->trx_level, false,
                        auto_commit, conn_data->thd);
      }

      err = innodb_api_begin(engine, meta_info->col_info[CONTAINER_DB].col_name,
                             meta_info->col_info[CONTAINER_TABLE].col_name,
                             conn_data, conn_data->crsr_trx,
                             &conn_data->read_crsr, &conn_data->idx_read_crsr,
                             lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->read_crsr);
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->read_crsr = NULL;
        conn_data->in_use = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

        return (NULL);
      }

    } else if (!conn_data->crsr_trx) {
      /* This is read operation, start a trx
      with "read_write" parameter set to false */
      conn_data->crsr_trx = ib_cb_trx_begin(engine->trx_level, false,
                                            auto_commit, conn_data->thd);

      trx_updated = true;

      innodb_cb_cursor_new_trx(conn_data->read_crsr, conn_data->crsr_trx);

      if (conn_data->crsr) {
        innodb_cb_cursor_new_trx(conn_data->crsr, conn_data->crsr_trx);
      }

      err = innodb_cb_cursor_lock(engine, conn_data->read_crsr, lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->read_crsr);
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->read_crsr = NULL;
        conn_data->in_use = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

        return (NULL);
      }

      if (meta_index->srch_use_idx == META_USE_SECONDARY) {
        ib_crsr_t idx_crsr = conn_data->idx_read_crsr;

        innodb_cb_cursor_new_trx(idx_crsr, conn_data->crsr_trx);
        innodb_cb_cursor_lock(engine, idx_crsr, lock_mode);
      }
    } else {
      /* This is read operation, start a trx
      with "read_write" parameter set to false */
      ib_cb_trx_start(conn_data->crsr_trx, engine->trx_level, false,
                      auto_commit, conn_data->thd);

      ib_cb_cursor_stmt_begin(conn_data->read_crsr);

      err = innodb_cb_cursor_lock(engine, conn_data->read_crsr, lock_mode);

      if (err != DB_SUCCESS) {
        innodb_cb_cursor_close(conn_data->read_crsr);
        innodb_cb_trx_commit(conn_data->crsr_trx);
        err = ib_cb_trx_release(conn_data->crsr_trx);
        assert(err == DB_SUCCESS);
        conn_data->crsr_trx = NULL;
        conn_data->read_crsr = NULL;
        conn_data->in_use = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

        return (NULL);
      }

      if (meta_index->srch_use_idx == META_USE_SECONDARY) {
        ib_crsr_t idx_crsr = conn_data->idx_read_crsr;
        ib_cb_cursor_stmt_begin(idx_crsr);
        innodb_cb_cursor_lock(engine, idx_crsr, lock_mode);
      }
    }

    if (trx_updated) {
      if (conn_data->crsr) {
        innodb_cb_cursor_new_trx(conn_data->crsr, conn_data->crsr_trx);
      }

      if (conn_data->idx_crsr) {
        innodb_cb_cursor_new_trx(conn_data->idx_crsr, conn_data->crsr_trx);
      }
    }
  }

  UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

  return (conn_data);
}

/*** allocate ***/

/*******************************************************************/ /**
 Allocate gets a struct item from the slab allocator, and fills in
 everything but the value.  It seems like we can just pass this on to
 the default engine; we'll intercept it later in store(). */
static ENGINE_ERROR_CODE innodb_allocate(
    /*============*/
    ENGINE_HANDLE *handle,    /*!< in: Engine handle */
    const void *cookie,       /*!< in: connection cookie */
    item **item,              /*!< out: item to allocate */
    const void *key,          /*!< in: key */
    const size_t nkey,        /*!< in: key length */
    const size_t nbytes,      /*!< in: estimated value length */
    const int flags,          /*!< in: flag */
    const rel_time_t exptime) /*!< in: expiration time */
{
  size_t len;
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  innodb_conn_data_t *conn_data;
  hash_item *it = NULL;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;

  conn_data =
      (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
          cookie);

  if (!conn_data) {
    conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE, IB_LOCK_X,
                                 false, NULL);
    if (!conn_data) {
      return (ENGINE_TMPFAIL);
    }
  }

  /* If system configured to use Memcached default engine (instead
  of InnoDB engine), continue to use Memcached's default memory
  allocation */
  if (meta_info->set_option == META_CACHE_OPT_DEFAULT ||
      meta_info->set_option == META_CACHE_OPT_MIX) {
    conn_data->use_default_mem = true;
    conn_data->in_use = false;
    return (def_eng->engine.allocate(innodb_eng->default_engine, cookie, item,
                                     key, nkey, nbytes, flags, exptime));
  }

  conn_data->use_default_mem = false;
  len = sizeof(*it) + nkey + nbytes + sizeof(uint64_t);
  if (len > conn_data->cmd_buf_len) {
    free(conn_data->cmd_buf);
    conn_data->cmd_buf = malloc(len);
    conn_data->cmd_buf_len = len;
  }

  it = (hash_item *)conn_data->cmd_buf;

  it->next = it->prev = it->h_next = 0;
  it->refcount = 1;
  it->iflag = def_eng->config.use_cas ? ITEM_WITH_CAS : 0;
  it->nkey = nkey;
  it->nbytes = nbytes;
  it->flags = flags;
  it->slabs_clsid = 1;
  /* item_get_key() is a memcached code, here we cast away const return */
  memcpy((void *)item_get_key(it), key, nkey);
  it->exptime = exptime;

  *item = it;
  conn_data->in_use = false;

  return (ENGINE_SUCCESS);
}

#ifdef UNIV_MEMCACHED_SDI
#define check_key_name_for_sdi(key, nkey, pattern) \
  check_key_name_for_sdi_pattern(key, nkey, pattern, (sizeof pattern) - 1)

/** Checks memcached key for SDI prefix patterns(sdi_, sdi_create_,
sdi_list_). If the prefix exists, then operation is for SDI table
@param[in]	key		Memcached Key
@param[in]	nkey		Length of Memcached Key
@param[in]	pattern		SDI patterns (sdi_, sdi_create_, sdi_list_)
@param[in]	pattern_len	SDI pattern len
@return true if it has prefix, else false */
static bool check_key_name_for_sdi_pattern(const void *key, const size_t nkey,
                                           const char *pattern,
                                           const size_t pattern_len) {
  return (nkey >= pattern_len &&
          strncmp((const char *)key, pattern, pattern_len) == 0);
}
#endif /* UNIV_MEMCACHED_SDI */

/*******************************************************************/ /**
 Cleanup connections
 @return number of connection cleaned */
static ENGINE_ERROR_CODE innodb_remove(
    /*==========*/
    ENGINE_HANDLE *handle, /*!< in: Engine handle */
    const void *cookie,    /*!< in: connection cookie */
    const void *key,       /*!< in: key */
    const size_t nkey,     /*!< in: key length */
    uint64_t cas __attribute__((unused)),
    /*!< in: cas */
    uint16_t vbucket __attribute__((unused)))
/*!< in: bucket, used by default
engine only */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  ENGINE_ERROR_CODE err_ret = ENGINE_SUCCESS;
  innodb_conn_data_t *conn_data;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  ENGINE_ERROR_CODE cacher_err = ENGINE_KEY_ENOENT;

  if (meta_info->del_option == META_CACHE_OPT_DISABLE) {
    return (ENGINE_SUCCESS);
  }

  if (meta_info->del_option == META_CACHE_OPT_DEFAULT ||
      meta_info->del_option == META_CACHE_OPT_MIX) {
    hash_item *item = item_get(def_eng, key, nkey);

    if (item != NULL) {
      item_unlink(def_eng, item);
      item_release(def_eng, item);
      cacher_err = ENGINE_SUCCESS;
    }

    if (meta_info->del_option == META_CACHE_OPT_DEFAULT) {
      return (cacher_err);
    }
  }

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE, IB_LOCK_X,
                               false, NULL);

  if (!conn_data) {
    return (ENGINE_TMPFAIL);
  }

#ifdef UNIV_MEMCACHED_SDI
  if (innodb_sdi_remove(innodb_eng, conn_data, &err_ret, key, nkey)) {
    return (err_ret);
  }
#endif /* UNIV_MEMCACHED_SDI */

  /* In the binary protocol there is such a thing as a CAS delete.
  This is the CAS check. If we will also be deleting from the database,
  there are two possibilities:
    1: The CAS matches; perform the delete.
    2: The CAS doesn't match; delete the item because it's stale.
  Therefore we skip the check altogether if(do_db_delete) */

  err_ret = innodb_api_delete(innodb_eng, conn_data, (const char *)key, nkey);

  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_DELETE,
                          err_ret == ENGINE_SUCCESS);

  return ((cacher_err == ENGINE_SUCCESS) ? ENGINE_SUCCESS : err_ret);
}

/*******************************************************************/ /**
 Switch the table mapping. Open the new table specified in "@@new_table_map.key"
 string.
 @return ENGINE_SUCCESS if successful, otherwise error code */
static ENGINE_ERROR_CODE innodb_switch_mapping(
    /*==================*/
    ENGINE_HANDLE *handle, /*!< in: Engine handle */
    const void *cookie,    /*!< in: connection cookie */
    const char *name,      /*!< in: full name contains
                           table map name, and possible
                           key value */
    size_t *name_len,      /*!< in/out: name length,
                           out with length excludes
                           the table map name */
    bool has_prefix)       /*!< in: whether the name has
                           "@@" prefix */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  innodb_conn_data_t *conn_data;
  char new_name[KEY_MAX_LENGTH];
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  char *new_map_name;
  unsigned int new_map_name_len = 0;
  char *last;
  meta_cfg_info_t *new_meta_info;
  int sep_len = 0;

  if (has_prefix) {
    char *sep = NULL;

    assert(*name_len > 2 && name[0] == '@' && name[1] == '@');
    assert(*name_len < KEY_MAX_LENGTH);

    memcpy(new_name, &name[2], (*name_len) - 2);

    new_name[*name_len - 2] = 0;

    GET_OPTION(meta_info, OPTION_ID_TBL_MAP_SEP, sep, sep_len);

    assert(sep_len > 0);

    new_map_name = strtok_r(new_name, sep, &last);

    if (new_map_name == NULL) {
      return (ENGINE_KEY_ENOENT);
    }

    new_map_name_len = strlen(new_map_name);
  } else {
    /* This is used in the "bind" command, and without the
    "@@" prefix. */
    if (name == NULL) {
      return (ENGINE_KEY_ENOENT);
    }

    new_map_name = (char *)name;
    new_map_name_len = *name_len;
  }

  conn_data =
      (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
          cookie);

  /* Check if we are getting the same configure setting as existing one */
  if (conn_data && conn_data->conn_meta &&
      (new_map_name_len ==
       conn_data->conn_meta->col_info[CONTAINER_NAME].col_name_len) &&
      (strcmp(new_map_name,
              conn_data->conn_meta->col_info[CONTAINER_NAME].col_name) == 0)) {
    goto get_key_name;
  }

  if (conn_data && conn_data->multi_get) {
    goto get_key_name;
  }

  new_meta_info =
      innodb_config(new_map_name, new_map_name_len, &innodb_eng->meta_hash);

  if (!new_meta_info) {
    return (ENGINE_KEY_ENOENT);
  }

  /* Clean up the existing connection metadata if exists */
  if (conn_data) {
    innodb_conn_clean_data(conn_data, false, false);

    /* Point to the new metadata */
    conn_data->conn_meta = new_meta_info;
  }

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_NONE,
                               ib_lck_mode_t(0), false, new_meta_info);

  assert(conn_data->conn_meta == new_meta_info);

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
      *name_len = 0;
    }
  }

  return (ENGINE_SUCCESS);
}

/*******************************************************************/ /**
 check whether a table mapping switch is needed, if so, switch the table
 mapping
 @return ENGINE_SUCCESS if successful otherwise error code */
static inline ENGINE_ERROR_CODE check_key_name_for_map_switch(
    /*==========================*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie,    /*!< in: connection cookie */
    const void *key,       /*!< in: search key */
    size_t *nkey)          /*!< in/out: key length */
{
  ENGINE_ERROR_CODE err_ret = ENGINE_SUCCESS;

  if ((*nkey) > 3 && ((char *)key)[0] == '@' && ((char *)key)[1] == '@') {
    err_ret =
        innodb_switch_mapping(handle, cookie, (const char *)key, nkey, true);
  }

  return (err_ret);
}

/*******************************************************************/ /**
 Function to support the "bind" command, bind the connection to a new
 table mapping.
 @return ENGINE_SUCCESS if successful, otherwise error code */
static ENGINE_ERROR_CODE innodb_bind(
    /*========*/
    ENGINE_HANDLE *handle, /*!< in: Engine handle */
    const void *cookie,    /*!< in: connection cookie */
    const void *name,      /*!< in: table ID name */
    size_t name_len)       /*!< in: name length */
{
  ENGINE_ERROR_CODE err_ret = ENGINE_SUCCESS;

  err_ret = innodb_switch_mapping(handle, cookie, (const char *)name, &name_len,
                                  false);

  return (err_ret);
}

/*******************************************************************/ /**
 Release the connection, free resource allocated in innodb_allocate */
static void innodb_clean_engine(
    /*================*/
    ENGINE_HANDLE *handle, /*!< in: Engine handle */
    const void *cookie __attribute__((unused)),
    /*!< in: connection cookie */
    void *conn) /*!< in: item to free */
{
  innodb_conn_data_t *conn_data = (innodb_conn_data_t *)conn;
  struct innodb_engine *engine = innodb_handle(handle);
  void *orignal_thd;

  LOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
  if (conn_data->thd) {
    handler_thd_attach(conn_data->thd, &orignal_thd);
  }
  innodb_reset_conn(conn_data, true, true, engine->enable_binlog);
  innodb_conn_clean_data(conn_data, true, false);
  conn_data->is_stale = true;
  UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
}

/*******************************************************************/ /**
 Release the connection, free resource allocated in innodb_allocate */
static void innodb_release(
    /*===========*/
    ENGINE_HANDLE *handle, /*!< in: Engine handle */
    const void *cookie __attribute__((unused)),
    /*!< in: connection cookie */
    item *item __attribute__((unused)))
/*!< in: item to free */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  innodb_conn_data_t *conn_data;
  mem_buf_t *mem_buf;

  conn_data =
      (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
          cookie);

  if (!conn_data) {
    return;
  }

  conn_data->result_in_use = false;
  conn_data->row_buf_slot = 0;
  conn_data->row_buf_used = 0;
  conn_data->range = false;
  conn_data->multi_get = false;
  conn_data->mul_col_buf_used = 0;

  mem_buf = UT_LIST_GET_FIRST(conn_data->mul_used_buf);

  while (mem_buf) {
    UT_LIST_REMOVE(mem_list, conn_data->mul_used_buf, mem_buf);
    free(mem_buf->mem);
    mem_buf = UT_LIST_GET_FIRST(conn_data->mul_used_buf);
  }

  /* If item's memory comes from Memcached default engine, release it
  through Memcached APIs */
  if (conn_data->use_default_mem) {
    struct default_engine *def_eng = default_handle(innodb_eng);

    item_release(def_eng, (hash_item *)item);
    conn_data->use_default_mem = false;
  }

  if (conn_data->range_key) {
    free(conn_data->range_key);
    conn_data->range_key = NULL;
  }

  return;
}

/* maximum number of characters that an 8 bytes integer can convert to */
#define MAX_INT_CHAR_LEN 21

/*******************************************************************/ /**
 Convert an bit int to string
 @return length of string */
static int convert_to_char(
    /*============*/
    char *buf,        /*!< out: converted integer value */
    int buf_len,      /*!< in: buffer len */
    void *value,      /*!< in: int value */
    int value_len,    /*!< in: int len */
    bool is_unsigned) /*!< in: whether it is unsigned */
{
  assert(buf && buf_len);

  if (value_len == 8) {
    if (is_unsigned) {
      uint64_t int_val = *(uint64_t *)value;
      snprintf(buf, buf_len, "%" PRIu64, int_val);
    } else {
      int64_t int_val = *(int64_t *)value;
      snprintf(buf, buf_len, "%" PRIi64, int_val);
    }
  } else if (value_len == 4) {
    if (is_unsigned) {
      uint32_t int_val = *(uint32_t *)value;
      snprintf(buf, buf_len, "%" PRIu32, int_val);
    } else {
      int32_t int_val = *(int32_t *)value;
      snprintf(buf, buf_len, "%" PRIi32, int_val);
    }
  } else if (value_len == 2) {
    if (is_unsigned) {
      uint16_t int_val = *(uint16_t *)value;
      snprintf(buf, buf_len, "%" PRIu16, int_val);
    } else {
      int16_t int_val = *(int16_t *)value;
      snprintf(buf, buf_len, "%" PRIi16, int_val);
    }
  } else if (value_len == 1) {
    if (is_unsigned) {
      uint8_t int_val = *(uint8_t *)value;
      snprintf(buf, buf_len, "%" PRIu8, int_val);
    } else {
      int8_t int_val = *(int8_t *)value;
      snprintf(buf, buf_len, "%" PRIi8, int_val);
    }
  }

  return (strlen(buf));
}

/*******************************************************************/ /**
 Free value assocaited with key */
static void innodb_free_item(
    /*=====================*/
    void *item) /*!< in: Item to be freed */
{
  mci_item_t *result = (mci_item_t *)item;
  if (result->extra_col_value) {
    for (int i = 0; i < result->n_extra_col; i++) {
      if (result->extra_col_value[i].allocated)
        free(result->extra_col_value[i].value_str);
    }
    free(result->extra_col_value);
    result->extra_col_value = NULL;
  }
  if (result->col_value[MCI_COL_VALUE].allocated) {
    free(result->col_value[MCI_COL_VALUE].value_str);
    result->col_value[MCI_COL_VALUE].allocated = false;
  }
}
/*******************************************************************/ /**
 Support memcached "GET" command, fetch the value according to key
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_get(
    /*=======*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie,    /*!< in: connection cookie */
    item **item,           /*!< out: item to fill */
    const void *key,       /*!< in: search key */
    const int nkey,        /*!< in: key length */
    uint16_t next_get)     /*!< in: has more item to get */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  ib_crsr_t crsr;
  ib_err_t err = DB_SUCCESS;
  mci_item_t *result = NULL;
  ENGINE_ERROR_CODE err_ret = ENGINE_SUCCESS;
  innodb_conn_data_t *conn_data = NULL;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  int option_length;
  const char *option_delimiter;
  size_t key_len = nkey;
  int lock_mode;
  bool report_table_switch = false;
  void *newkey;
  bool is_range_srch = false;
  ;

  if (memcached_shutdown) {
    return (ENGINE_TMPFAIL);
  }

  if (meta_info->get_option == META_CACHE_OPT_DISABLE) {
    return (ENGINE_KEY_ENOENT);
  }

  if (meta_info->get_option == META_CACHE_OPT_DEFAULT ||
      meta_info->get_option == META_CACHE_OPT_MIX) {
    *item = item_get(default_handle(innodb_eng), key, nkey);

    if (*item != NULL) {
      return (ENGINE_SUCCESS);
    }

    if (meta_info->get_option == META_CACHE_OPT_DEFAULT) {
      return (ENGINE_KEY_ENOENT);
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
      report_table_switch = true;

      goto search_done;
    }

    err_ret = ENGINE_KEY_ENOENT;
    goto err_exit;
  }

  lock_mode = (innodb_eng->trx_level == IB_TRX_SERIALIZABLE &&
               innodb_eng->read_batch_size == 1)
                  ? IB_LOCK_S
                  : IB_LOCK_NONE;

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_READ,
                               ib_lck_mode_t(lock_mode), false, NULL);

  if (!conn_data) {
    return (ENGINE_TMPFAIL);
  }

  result = (mci_item_t *)(conn_data->result);

  newkey = (char *)key + nkey - key_len;

  /* This signifies a range search, so set up the range search info */
  if (*(char *)newkey == '@' && !conn_data->range) {
    assert(!conn_data->range_key);

    if (((char *)newkey)[1] == '<') {
      char *start_key;

      is_range_srch = true;

      conn_data->range_key =
          (innodb_range_key_t *)malloc(sizeof *(conn_data->range_key));

      if (((char *)newkey)[2] == '=') {
        conn_data->range_key->end_mode = IB_CUR_LE;
        key_len -= 3;
      } else {
        conn_data->range_key->end_mode = IB_CUR_L;
        key_len -= 2;
      }

      conn_data->range_key->end = (char *)key + nkey - key_len;
      conn_data->range_key->end_len = key_len;

      start_key = strstr((char *)newkey, "@>");
      if (start_key) {
        conn_data->range_key->bound = RANGE_BOUND;
        uint cmp_len = 2;

        if (start_key[2] == '=') {
          conn_data->range_key->start_mode = IB_CUR_GE;
          cmp_len = 3;
        } else {
          conn_data->range_key->start_mode = IB_CUR_G;
        }
        conn_data->range_key->end_len = start_key - conn_data->range_key->end;
        conn_data->range_key->start = &start_key[cmp_len];
        conn_data->range_key->start_len =
            key_len - conn_data->range_key->end_len - cmp_len;
      } else {
        conn_data->range_key->start = NULL;
        conn_data->range_key->start_len = 0;
        conn_data->range_key->start_mode = 0;
        conn_data->range_key->bound = UPPER_BOUND;
      }
    } else if (((char *)newkey)[1] == '>') {
      char *end_key;

      is_range_srch = true;

      conn_data->range_key =
          (innodb_range_key_t *)malloc(sizeof *(conn_data->range_key));

      if (((char *)newkey)[2] == '=') {
        conn_data->range_key->start_mode = IB_CUR_GE;
        key_len -= 3;
      } else {
        conn_data->range_key->start_mode = IB_CUR_G;
        key_len -= 2;
      }

      conn_data->range_key->start_len = key_len;
      conn_data->range_key->start = (char *)key + nkey - key_len;

      end_key = strstr((char *)newkey, "@<");
      if (end_key) {
        conn_data->range_key->bound = RANGE_BOUND;
        uint cmp_len = 2;

        if (end_key[2] == '=') {
          conn_data->range_key->end_mode = IB_CUR_LE;
          cmp_len = 3;
        } else {
          conn_data->range_key->end_mode = IB_CUR_L;
        }
        conn_data->range_key->start_len = end_key - conn_data->range_key->start;
        conn_data->range_key->end = &end_key[cmp_len];
        conn_data->range_key->end_len =
            key_len - conn_data->range_key->start_len - cmp_len;
      } else {
        conn_data->range_key->end = NULL;
        conn_data->range_key->end_len = 0;
        conn_data->range_key->end_mode = 0;
        conn_data->range_key->bound = LOW_BOUND;
      }
    }
  }

  if (conn_data->range) {
    is_range_srch = true;
  }

#ifdef UNIV_MEMCACHED_SDI
  if (innodb_sdi_get(conn_data, &err_ret, key, nkey, &item)) {
    goto func_exit;
  }
#endif /* UNIV_MEMCACHED_SDI */

  err = innodb_api_search(conn_data, &crsr, (const char *)key + nkey - key_len,
                          key_len, result, NULL, true,
                          is_range_srch ? conn_data->range_key : NULL);

  if (is_range_srch && err != DB_END_OF_INDEX) {
    /* we set it only after the first search. This is used to
    tell innodb_api_search() if it is the first search, which
    might need to do the intial position of cursor */
    conn_data->range = true;
  }

  if (next_get) {
    conn_data->multi_get = true;
  }

  if (conn_data->multi_get && next_get == 0) {
    conn_data->multi_get = false;
  }

  if (err != DB_SUCCESS) {
    err_ret = ENGINE_KEY_ENOENT;
    goto func_exit;
  }

search_done:
  if (report_table_switch) {
    char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];
    char *name;
    char *dbname;

    conn_data =
        (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
            cookie);
    assert(nkey > 0);

    name = conn_data->conn_meta->col_info[CONTAINER_TABLE].col_name;
    dbname = conn_data->conn_meta->col_info[CONTAINER_DB].col_name;
#ifdef __WIN__
    sprintf(table_name, "%s\%s", dbname, name);
#else
    snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif

    assert(!conn_data->result_in_use);
    conn_data->result_in_use = true;
    result = (mci_item_t *)(conn_data->result);

    memset(result, 0, sizeof(*result));

    memcpy((char *)(conn_data->row_buf[conn_data->row_buf_slot]) +
               conn_data->row_buf_used,
           table_name, strlen(table_name));

    result->col_value[MCI_COL_VALUE].value_str =
        ((char *)conn_data->row_buf[conn_data->row_buf_slot]) +
        conn_data->row_buf_used;
    result->col_value[MCI_COL_VALUE].value_len = strlen(table_name);
  }

  if (!conn_data->range) {
    result->col_value[MCI_COL_KEY].value_str = (char *)key;
    result->col_value[MCI_COL_KEY].value_len = nkey;
  }

  /* Only if expiration field is enabled, and the value is not zero,
  we will check whether the item is expired */
  if (result->col_value[MCI_COL_EXP].is_valid &&
      result->col_value[MCI_COL_EXP].value_int) {
    uint64_t time;
    time = mci_get_time();
    if (time > result->col_value[MCI_COL_EXP].value_int) {
      innodb_free_item(result);
      err_ret = ENGINE_KEY_ENOENT;
      goto func_exit;
    }
  }

  if (result->extra_col_value) {
    int i;
    char *c_value;
    char *value_end MY_ATTRIBUTE((unused));
    unsigned int total_len = 0;
    char int_buf[MAX_INT_CHAR_LEN];
    ib_ulint_t new_len;

    GET_OPTION(meta_info, OPTION_ID_COL_SEP, option_delimiter, option_length);

    assert(option_length > 0 && option_delimiter);

    for (i = 0; i < result->n_extra_col; i++) {
      mci_column_t *mci_item = &result->extra_col_value[i];

      if (mci_item->value_len == 0) {
        total_len += option_length;
        continue;
      }

      if (!mci_item->is_str) {
        memset(int_buf, 0, sizeof int_buf);
        assert(!mci_item->value_str);

        total_len +=
            convert_to_char(int_buf, sizeof int_buf, &mci_item->value_int,
                            mci_item->value_len, mci_item->is_unsigned);
      } else {
        total_len += result->extra_col_value[i].value_len;
      }

      total_len += option_length;
    }

    /* No need to add the last separator */
    total_len -= option_length;
    new_len = total_len + conn_data->mul_col_buf_used;

    if (new_len >= conn_data->mul_col_buf_len) {
      /* Need to keep the old result buffer, since its
      point is already registered with memcached output
      buffer. These result buffers will be release
      once results are all reported */
      if (conn_data->mul_col_buf) {
        mem_buf_t *new_temp = (mem_buf_t *)malloc(sizeof(mem_buf_t));
        new_temp->mem = conn_data->mul_col_buf;
        UT_LIST_ADD_LAST(mem_list, conn_data->mul_used_buf, new_temp);
      }

      conn_data->mul_col_buf = (char *)malloc(new_len + 1);
      conn_data->mul_col_buf_len = new_len + 1;
      conn_data->mul_col_buf_used = 0;
    }

    c_value = &conn_data->mul_col_buf[conn_data->mul_col_buf_used];
    value_end = &conn_data->mul_col_buf[new_len];

    for (i = 0; i < result->n_extra_col; i++) {
      mci_column_t *col_value;

      col_value = &result->extra_col_value[i];

      if (col_value->value_len != 0) {
        if (!col_value->is_str) {
          ib_ulint_t int_len;
          memset(int_buf, 0, sizeof int_buf);

          int_len =
              convert_to_char(int_buf, sizeof int_buf, &col_value->value_int,
                              col_value->value_len, col_value->is_unsigned);

          assert(int_len <= conn_data->mul_col_buf_len);

          memcpy(c_value, int_buf, int_len);
          c_value += int_len;
        } else {
          memcpy(c_value, col_value->value_str, col_value->value_len);
          c_value += col_value->value_len;
        }
      }

      if (i < result->n_extra_col - 1) {
        memcpy(c_value, option_delimiter, option_length);
        c_value += option_length;
      }

      assert(c_value <= value_end);

      if (col_value->allocated) {
        free(col_value->value_str);
      }
    }

    result->col_value[MCI_COL_VALUE].value_str =
        &conn_data->mul_col_buf[conn_data->mul_col_buf_used];
    result->col_value[MCI_COL_VALUE].value_len = total_len;
    conn_data->mul_col_buf_used += total_len;
    ((char *)result->col_value[MCI_COL_VALUE].value_str)[total_len] = 0;

    free(result->extra_col_value);
  } else if (!result->col_value[MCI_COL_VALUE].is_str &&
             result->col_value[MCI_COL_VALUE].value_len != 0) {
    unsigned int int_len;
    char int_buf[MAX_INT_CHAR_LEN];

    int_len = convert_to_char(int_buf, sizeof int_buf,
                              &result->col_value[MCI_COL_VALUE].value_int,
                              result->col_value[MCI_COL_VALUE].value_len,
                              result->col_value[MCI_COL_VALUE].is_unsigned);

    if (int_len > conn_data->mul_col_buf_len) {
      if (conn_data->mul_col_buf) {
        free(conn_data->mul_col_buf);
      }

      conn_data->mul_col_buf = (char *)malloc(int_len + 1);
      conn_data->mul_col_buf_len = int_len;
    }

    if (int_len > 0) memcpy(conn_data->mul_col_buf, int_buf, int_len);
    result->col_value[MCI_COL_VALUE].value_str = conn_data->mul_col_buf;

    result->col_value[MCI_COL_VALUE].value_len = int_len;
  }

  *item = result;

func_exit:

  if ((!report_table_switch && !is_range_srch && !next_get) ||
      err == DB_END_OF_INDEX || (conn_data->range && err != DB_SUCCESS)) {
    innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_READ, true);
  }

err_exit:

  /* If error return, memcached will not call InnoDB Memcached's
  callback function "innodb_release" to reset the result_in_use
  value. So we reset it here */
  if (err_ret != ENGINE_SUCCESS && conn_data) {
    if (conn_data->range_key) {
      free(conn_data->range_key);
      conn_data->range_key = NULL;
    }
    conn_data->range = false;

    conn_data->result_in_use = false;
  }
  return (err_ret);
}

/*******************************************************************/ /**
 Get statistics info
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_get_stats(
    /*=============*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie,    /*!< in: connection cookie */
    const char *stat_key,  /*!< in: statistics key */
    int nkey,              /*!< in: key length */
    ADD_STAT add_stat)     /*!< out: stats to fill */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  return (def_eng->engine.get_stats(innodb_eng->default_engine, cookie,
                                    stat_key, nkey, add_stat));
}

/*******************************************************************/ /**
 reset statistics
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static void innodb_reset_stats(
    /*===============*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie)    /*!< in: connection cookie */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  def_eng->engine.reset_stats(innodb_eng->default_engine, cookie);
}

/*******************************************************************/ /**
 API interface for memcached's "SET", "ADD", "REPLACE", "APPEND"
 "PREPENT" and "CAS" commands
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_store(
    /*=========*/
    ENGINE_HANDLE *handle,     /*!< in: Engine Handle */
    const void *cookie,        /*!< in: connection cookie */
    item *item,                /*!< out: result to fill */
    uint64_t *cas,             /*!< in: cas value */
    ENGINE_STORE_OPERATION op, /*!< in: type of operation */
    uint16_t vbucket __attribute__((unused)))
/*!< in: bucket, used by default
engine only */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  uint16_t len = hash_item_get_key_len((const hash_item *)item);
  char *value = hash_item_get_key((const hash_item *)item);
  uint64_t exptime = hash_item_get_exp((const hash_item *)item);
  uint64_t flags = hash_item_get_flag((const hash_item *)item);
  ENGINE_ERROR_CODE result;
  uint64_t input_cas;
  innodb_conn_data_t *conn_data;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  uint32_t val_len = ((hash_item *)item)->nbytes;
  size_t key_len = len;
  ENGINE_ERROR_CODE err_ret = ENGINE_SUCCESS;

  if (meta_info->set_option == META_CACHE_OPT_DISABLE) {
    return (ENGINE_SUCCESS);
  }

  if (meta_info->set_option == META_CACHE_OPT_DEFAULT ||
      meta_info->set_option == META_CACHE_OPT_MIX) {
    result = store_item(default_handle(innodb_eng), (hash_item *)item, cas, op,
                        cookie);

    if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
      return (result);
    }
  }

  err_ret = check_key_name_for_map_switch(handle, cookie, value, &key_len);

  if (err_ret != ENGINE_SUCCESS) {
    return (err_ret);
  }

  /* If no key is provided, return here */
  if (key_len <= 0) {
    return (ENGINE_NOT_STORED);
  }

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE, IB_LOCK_X,
                               false, NULL);

  if (!conn_data) {
    return (ENGINE_NOT_STORED);
  }

  input_cas = hash_item_get_cas((const hash_item *)item);

#ifdef UNIV_MEMCACHED_SDI
  if (innodb_sdi_store(innodb_eng, conn_data, &result, value, val_len,
                       key_len)) {
    return (result);
  }
#endif /* UNIV_MEMACHED_SDI */

  result =
      innodb_api_store(innodb_eng, conn_data, value + len - key_len, key_len,
                       val_len, exptime, cas, input_cas, flags, op);

  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE,
                          result == ENGINE_SUCCESS);
  return (result);
}

/*******************************************************************/ /**
 Support memcached "INCR" and "DECR" command, add or subtract a "delta"
 value from an integer key value
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_arithmetic(
    /*==============*/
    ENGINE_HANDLE *handle,    /*!< in: Engine Handle */
    const void *cookie,       /*!< in: connection cookie */
    const void *key,          /*!< in: key for the value to add */
    const int nkey,           /*!< in: key length */
    const bool increment,     /*!< in: whether to increment
                              or decrement */
    const bool create,        /*!< in: whether to create the key
                              value pair if can't find */
    const uint64_t delta,     /*!< in: value to add/substract */
    const uint64_t initial,   /*!< in: initial */
    const rel_time_t exptime, /*!< in: expiration time */
    uint64_t *cas,            /*!< out: new cas value */
    uint64_t *result,         /*!< out: result value */
    uint16_t vbucket)         /*!< in: bucket, used by default
                              engine only */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  innodb_conn_data_t *conn_data;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  ENGINE_ERROR_CODE err_ret;

  if (meta_info->set_option == META_CACHE_OPT_DISABLE) {
    return (ENGINE_SUCCESS);
  }

  if (meta_info->set_option == META_CACHE_OPT_DEFAULT ||
      meta_info->set_option == META_CACHE_OPT_MIX) {
    /* For cache-only, forward this to the
    default engine */
    err_ret = def_eng->engine.arithmetic(
        innodb_eng->default_engine, cookie, key, nkey, increment, create, delta,
        initial, exptime, cas, result, vbucket);

    if (meta_info->set_option == META_CACHE_OPT_DEFAULT) {
      return (err_ret);
    }
  }

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE, IB_LOCK_X,
                               false, NULL);

  if (!conn_data) {
    return (ENGINE_NOT_STORED);
  }

  err_ret = innodb_api_arithmetic(innodb_eng, conn_data, (const char *)key,
                                  nkey, delta, increment, cas, exptime, create,
                                  initial, result);

  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE, true);

  return (err_ret);
}

/*******************************************************************/ /**
 Cleanup idle connections if "clear_all" is false, and clean up all
 connections if "clear_all" is true.
 @return number of connection cleaned */
static bool innodb_flush_sync_conn(
    /*===================*/
    innodb_engine_t *engine, /*!< in/out: InnoDB memcached
                             engine */
    const void *cookie,      /*!< in: connection cookie */
    bool flush_flag)         /*!< in: flush is running or not */
{
  innodb_conn_data_t *conn_data = NULL;
  innodb_conn_data_t *curr_conn_data;
  bool ret = true;

  curr_conn_data =
      (innodb_conn_data_t *)engine->server.cookie->get_engine_specific(cookie);
  assert(curr_conn_data);

  conn_data = UT_LIST_GET_FIRST(engine->conn_data);

  while (conn_data) {
    if (conn_data != curr_conn_data && (!conn_data->is_stale)) {
      if (conn_data->thd) {
        handler_thd_attach(conn_data->thd, NULL);
      }
      LOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
      if (flush_flag == false) {
        conn_data->is_flushing = flush_flag;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
        conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
        continue;
      }
      if (!conn_data->in_use) {
        /* Set flushing flag to conn_data for preventing
        it is get by other request.  */
        conn_data->is_flushing = flush_flag;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
      } else {
        ret = false;
        UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(false, conn_data);
        break;
      }
    }
    conn_data = UT_LIST_GET_NEXT(conn_list, conn_data);
  }

  if (curr_conn_data->thd) {
    handler_thd_attach(curr_conn_data->thd, NULL);
  }

  return (ret);
}

/*******************************************************************/ /**
 Support memcached "FLUSH_ALL" command, clean up storage (trunate InnoDB Table)
 @return ENGINE_SUCCESS if successfully, otherwise error code */
static ENGINE_ERROR_CODE innodb_flush(
    /*=========*/
    ENGINE_HANDLE *handle, /*!< in: Engine Handle */
    const void *cookie,    /*!< in: connection cookie */
    time_t when)           /*!< in: when to flush, not used by
                           InnoDB */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);
  ENGINE_ERROR_CODE err = ENGINE_SUCCESS;
  meta_cfg_info_t *meta_info = innodb_eng->meta_info;
  ib_err_t ib_err = DB_SUCCESS;
  innodb_conn_data_t *conn_data;

  if (meta_info->flush_option == META_CACHE_OPT_DISABLE) {
    return (ENGINE_SUCCESS);
  }

  if (meta_info->flush_option == META_CACHE_OPT_DEFAULT ||
      meta_info->flush_option == META_CACHE_OPT_MIX) {
    /* default engine flush */
    err = def_eng->engine.flush(innodb_eng->default_engine, cookie, when);

    if (meta_info->flush_option == META_CACHE_OPT_DEFAULT) {
      return (err);
    }
  }

  /* Lock the whole engine, so no other connection can start
  new opeartion */
  pthread_mutex_lock(&innodb_eng->conn_mutex);

  /* Lock the flush_mutex for blocking other DMLs. */
  pthread_mutex_lock(&innodb_eng->flush_mutex);

  conn_data =
      (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
          cookie);

  if (conn_data) {
    /* Commit any work on this connection */
    innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_FLUSH, true);
  }

  conn_data = innodb_conn_init(innodb_eng, cookie, CONN_MODE_WRITE,
                               IB_LOCK_TABLE_X, true, NULL);

  if (!conn_data) {
    pthread_mutex_unlock(&innodb_eng->flush_mutex);
    pthread_mutex_unlock(&innodb_eng->conn_mutex);
    return (ENGINE_SUCCESS);
  }

  if (!innodb_flush_sync_conn(innodb_eng, cookie, true)) {
    pthread_mutex_unlock(&innodb_eng->flush_mutex);
    pthread_mutex_unlock(&innodb_eng->conn_mutex);
    innodb_flush_sync_conn(innodb_eng, cookie, false);
    return (ENGINE_TMPFAIL);
  }

  meta_info = conn_data->conn_meta;
  ib_err = ib_err_t(innodb_api_flush(
      innodb_eng, conn_data, meta_info->col_info[CONTAINER_DB].col_name,
      meta_info->col_info[CONTAINER_TABLE].col_name));

  /* Commit work and release the MDL table. */
  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_FLUSH, true);
  innodb_conn_clean_data(conn_data, false, false);

  pthread_mutex_unlock(&innodb_eng->flush_mutex);
  pthread_mutex_unlock(&innodb_eng->conn_mutex);

  innodb_flush_sync_conn(innodb_eng, cookie, false);

  return ((ib_err == DB_SUCCESS) ? ENGINE_SUCCESS : ENGINE_TMPFAIL);
}

/*******************************************************************/ /**
 Deal with unknown command. Currently not used
 @return ENGINE_SUCCESS if successfully processed, otherwise error code */
static ENGINE_ERROR_CODE innodb_unknown_command(
    /*===================*/
    ENGINE_HANDLE *handle,                   /*!< in: Engine Handle */
    const void *cookie,                      /*!< in: connection cookie */
    protocol_binary_request_header *request, /*!< in: request */
    ADD_RESPONSE response)                   /*!< out: respondse */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  struct default_engine *def_eng = default_handle(innodb_eng);

  return (def_eng->engine.unknown_command(innodb_eng->default_engine, cookie,
                                          request, response));
}

/*******************************************************************/ /**
 Callback functions used by Memcached's process_command() function
 to get the result key/value information
 @return true if info fetched */
static bool innodb_get_item_info(
    /*=================*/
    ENGINE_HANDLE *handle __attribute__((unused)),
    /*!< in: Engine Handle */
    const void *cookie __attribute__((unused)),
    /*!< in: connection cookie */
    const item *item,     /*!< in: item in question */
    item_info *item_info) /*!< out: item info got */
{
  struct innodb_engine *innodb_eng = innodb_handle(handle);
  innodb_conn_data_t *conn_data;

  conn_data =
      (innodb_conn_data_t *)innodb_eng->server.cookie->get_engine_specific(
          cookie);

  if (!conn_data || !conn_data->result_in_use) {
    hash_item *it;

    if (item_info->nvalue < 1) {
      return (false);
    }

    /* Use a hash item */
    it = (hash_item *)item;
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
  } else {
    mci_item_t *it;

    if (item_info->nvalue < 1) {
      return (false);
    }

    /* Use a hash item */
    it = (mci_item_t *)item;
    if (it->col_value[MCI_COL_CAS].is_valid) {
      item_info->cas = it->col_value[MCI_COL_CAS].value_int;
    } else {
      item_info->cas = 0;
    }

    if (it->col_value[MCI_COL_EXP].is_valid) {
      item_info->exptime = it->col_value[MCI_COL_EXP].value_int;
    } else {
      item_info->exptime = 0;
    }

    item_info->nbytes = it->col_value[MCI_COL_VALUE].value_len;

    if (it->col_value[MCI_COL_FLAG].is_valid) {
      item_info->flags = ntohl(it->col_value[MCI_COL_FLAG].value_int);
    } else {
      item_info->flags = 0;
    }

    item_info->clsid = 1;

    item_info->nkey = it->col_value[MCI_COL_KEY].value_len;

    item_info->nvalue = 1;

    item_info->key = it->col_value[MCI_COL_KEY].value_str;

    item_info->value[0].iov_base = it->col_value[MCI_COL_VALUE].value_str;
    ;

    item_info->value[0].iov_len = it->col_value[MCI_COL_VALUE].value_len;
  }

  return (true);
}

#ifdef UNIV_MEMCACHED_SDI
/** Remove SDI entry from tablespace
@param[in,out]	innodb_eng	innodb engine structure
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	key		memcached key
@param[in]	nkey		memcached key length
@return true if key is SDI key else false */
static bool innodb_sdi_remove(struct innodb_engine *innodb_eng,
                              innodb_conn_data_t *conn_data,
                              ENGINE_ERROR_CODE *err_ret, const void *key,
                              const size_t nkey) {
  if (!check_key_name_for_sdi(key, nkey, SDI_PREFIX)) {
    return (false);
  }

  ib_trx_t trx = conn_data->crsr_trx;
  ib_crsr_t crsr;
  /* +2 for the '/' and trailing '\0' */
  char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 2];
  char *name;
  char *dbname;
  ib_err_t err;

  assert(nkey > 0);

  name = conn_data->conn_meta->col_info[CONTAINER_TABLE].col_name;
  dbname = conn_data->conn_meta->col_info[CONTAINER_DB].col_name;

  snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);

  err = innodb_cb_open_table(table_name, trx, &crsr);

  /* Mapped InnoDB table must be able to open */
  if (err != DB_SUCCESS) {
    fprintf(stderr,
            "InnoDB_Memcached: failed to open table"
            " '%s' \n",
            table_name);
    err = DB_ERROR;
  } else {
    err = ib_cb_sdi_delete(crsr, (const char *)key, trx);
  }

  ib_cb_cursor_close(crsr);

  if (err != DB_SUCCESS) {
    *err_ret = ENGINE_KEY_ENOENT;
  } else {
    *err_ret = ENGINE_SUCCESS;
  }

  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_DELETE,
                          *err_ret == ENGINE_SUCCESS);

  return (true);
}

/** Retrieve SDI for a given SDI key from tablespace
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	key		memcached key
@param[in]	nkey		memcached key length
@param[in,out]	item		memcached item to fill
@return true if key is SDI key else false */
static bool innodb_sdi_get(innodb_conn_data_t *conn_data,
                           ENGINE_ERROR_CODE *err_ret, const void *key,
                           const size_t nkey, item ***item) {
  if (!check_key_name_for_sdi(key, nkey, SDI_PREFIX)) {
    return (false);
  }

  mci_item_t *result = (mci_item_t *)conn_data->result;

  ib_trx_t trx = conn_data->crsr_trx;
  ib_crsr_t crsr;

  /* +2 for the '/' and trailing '\0' */
  char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 2];
  char *name;
  char *dbname;
  ib_err_t err;

  assert(nkey > 0);

  name = conn_data->conn_meta->col_info[CONTAINER_TABLE].col_name;
  dbname = conn_data->conn_meta->col_info[CONTAINER_DB].col_name;

  snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);

  err = innodb_cb_open_table(table_name, trx, &crsr);

  /* Mapped InnoDB table must be able to open */
  if (err != DB_SUCCESS) {
    fprintf(stderr,
            "InnoDB_Memcached: failed to open table"
            " '%s' \n",
            table_name);

    ib_cb_cursor_close(crsr);
    *err_ret = ENGINE_KEY_ENOENT;
    return (true);
  }

  uint64_t ret_len;
  if (check_key_name_for_sdi(key, nkey, SDI_CREATE_PREFIX)) {
    /* Create SDI Index in the tablespace */
    err = ib_cb_sdi_create(crsr);
    ib_cb_cursor_close(crsr);
    *err_ret = ENGINE_KEY_ENOENT;
    return (true);
  }

  if (check_key_name_for_sdi(key, nkey, SDI_DROP_PREFIX)) {
    /* Create SDI Index in the tablespace */
    err = ib_cb_sdi_drop(crsr);
    ib_cb_cursor_close(crsr);
    *err_ret = ENGINE_KEY_ENOENT;
    return (true);
  }

  if (check_key_name_for_sdi(key, nkey, SDI_LIST_PREFIX)) {
    if (conn_data->sdi_buf) {
      free(conn_data->sdi_buf);
    }
    conn_data->sdi_buf = malloc(SDI_LIST_BUF_MAX_LEN);

    err = ib_cb_sdi_get_keys(crsr, (const char *)key,
                             (char *)conn_data->sdi_buf, SDI_LIST_BUF_MAX_LEN);
    ret_len = strlen((char *)conn_data->sdi_buf);
  } else {
    /* Allocate memory of 64 KB, assuming SDI will fit into
    it. If retrieval fails, we will get actual length of SDI.
    We retry afer allocating the required memory */
    const uint32_t mem_size = 64 * 1024;
    void *new_mem = realloc(conn_data->sdi_buf, mem_size);

    if (new_mem == NULL) {
      free(conn_data->sdi_buf);
      conn_data->sdi_buf = NULL;
      *err_ret = ENGINE_KEY_ENOENT;
      ib_cb_cursor_close(crsr);
      return (true);
    }

    conn_data->sdi_buf = new_mem;
    ret_len = mem_size;
    err = ib_cb_sdi_get(crsr, (const char *)key, conn_data->sdi_buf, &ret_len,
                        trx);

    if (err == DB_SUCCESS) {
      assert(ret_len < mem_size);
    } else if (ret_len != UINT64_MAX) {
      /* Retry with required memory */
      void *new_mem = realloc(conn_data->sdi_buf, ret_len);

      if (new_mem == NULL) {
        free(conn_data->sdi_buf);
        conn_data->sdi_buf = NULL;
        *err_ret = ENGINE_KEY_ENOENT;
        ib_cb_cursor_close(crsr);
        return (true);
      }

      conn_data->sdi_buf = new_mem;
      err = ib_cb_sdi_get(crsr, (const char *)key, conn_data->sdi_buf, &ret_len,
                          trx);
    }
  }

  ib_cb_cursor_close(crsr);

  if (err != DB_SUCCESS) {
    *err_ret = ENGINE_KEY_ENOENT;
  } else {
    *err_ret = ENGINE_SUCCESS;

    memset(result, 0, sizeof(*result));
    result->col_value[MCI_COL_KEY].value_str = (char *)key;
    result->col_value[MCI_COL_KEY].value_len = nkey;
    result->col_value[MCI_COL_KEY].is_str = true;
    result->col_value[MCI_COL_KEY].is_valid = true;

    result->col_value[MCI_COL_VALUE].value_str = (char *)conn_data->sdi_buf;
    result->col_value[MCI_COL_VALUE].value_len = ret_len;
    result->col_value[MCI_COL_VALUE].is_str = true;
    result->col_value[MCI_COL_VALUE].is_valid = true;

    result->col_value[MCI_COL_CAS].is_null = true;
    result->col_value[MCI_COL_EXP].is_null = true;
    result->col_value[MCI_COL_FLAG].is_null = true;
    conn_data->result_in_use = true;
    **item = result;
  }
  return (true);
}

/** Store SDI entry into a tablespace
@param[in,out]	innodb_eng	innodb engine structure
@param[in,out]	conn_data	innodb connection data
@param[in,out]	err_ret		error code
@param[in]	value		memcached value
@param[in]	value_len	memcached value length
@param[in]	nkey		memcached key length
@return true if key is SDI key else false */
static bool innodb_sdi_store(struct innodb_engine *innodb_eng,
                             innodb_conn_data_t *conn_data,
                             ENGINE_ERROR_CODE *err_ret, char *value,
                             uint32_t val_len, const size_t nkey) {
  if (!check_key_name_for_sdi(value, nkey, SDI_PREFIX)) {
    return (false);
  }

  ib_trx_t trx = conn_data->crsr_trx;
  ib_crsr_t crsr;

  /* +2 for the '/' and trailing '\0' */
  char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 2];
  char *name;
  char *dbname;

  name = conn_data->conn_meta->col_info[CONTAINER_TABLE].col_name;
  dbname = conn_data->conn_meta->col_info[CONTAINER_DB].col_name;

  snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);

  ib_err_t err = innodb_cb_open_table(table_name, trx, &crsr);

  /* Mapped InnoDB table must be able to open */
  if (err != DB_SUCCESS) {
    fprintf(stderr,
            "InnoDB_Memcached: failed to open table"
            " '%s' \n",
            table_name);
  } else {
    uint64_t sdi_len = val_len;
    char *sdi = value + nkey;
    char key[100];
    /* Extract key from value */
    assert(nkey < 100);
    strncpy(key, value, nkey);
    key[nkey] = 0;
    err = ib_cb_sdi_set(crsr, key, sdi, &sdi_len, trx);
  }

  ib_cb_cursor_close(crsr);

  if (err != DB_SUCCESS) {
    *err_ret = ENGINE_NOT_STORED;
  } else {
    *err_ret = ENGINE_SUCCESS;
  }
  innodb_api_cursor_reset(innodb_eng, conn_data, CONN_OP_WRITE,
                          *err_ret == ENGINE_SUCCESS);
  return (true);
}
#endif /* UNIV_MEMCACHED_SDI */
