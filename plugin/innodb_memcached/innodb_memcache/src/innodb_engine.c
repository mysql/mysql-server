#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

//#include "config.h"

#include "default_engine.h"

#include <memcached/util.h>
#include <memcached/config_parser.h>
#include <memcached/extension_loggers.h>

#include "innodb_engine.h"
#include "innodb_engine_private.h"
#include "debug.h"
#include "innodb_api.h"


#define DEBUG_THD_NAME "engine"
#define DEBUG_THD_ID pipeline->id

/* A static global variable */
EXTENSION_LOGGER_DESCRIPTOR *logger;

static ib_crsr_t	g_crsr = NULL;

/* Static and local to this file */
const char * set_ops[] = { "","add","set","replace","append","prepend","cas" };

/* InnoDB API callback functions */
ib_cb_t* innodb_memcached_api[] = {
	(ib_cb_t*) &ib_cb_open_table,
	(ib_cb_t*) &ib_cb_read_row,
	(ib_cb_t*) &ib_cb_insert_row,
	(ib_cb_t*) &ib_cb_delete_row,
	(ib_cb_t*) &ib_cb_update_row,
	(ib_cb_t*) &ib_cb_moveto,
	(ib_cb_t*) &ib_cb_cursor_first,
	(ib_cb_t*) &ib_cb_cursor_last,
	(ib_cb_t*) &ib_cb_cursor_set_match_mode,
	(ib_cb_t*) &ib_cb_search_tuple_create,
	(ib_cb_t*) &ib_cb_read_tuple_create,
	(ib_cb_t*) &ib_cb_tuple_delete,
	(ib_cb_t*) &ib_cb_tuple_copy,
	(ib_cb_t*) &ib_cb_tuple_read_u32,
	(ib_cb_t*) &ib_cb_tuple_write_u32,
	(ib_cb_t*) &ib_cb_tuple_read_u64,
	(ib_cb_t*) &ib_cb_tuple_write_u64,
	(ib_cb_t*) &ib_cb_tuple_read_i32,
	(ib_cb_t*) &ib_cb_tuple_write_i32,
	(ib_cb_t*) &ib_cb_tuple_read_i64,
	(ib_cb_t*) &ib_cb_tuple_write_i64,
	(ib_cb_t*) &ib_cb_tuple_get_n_cols,
	(ib_cb_t*) &ib_cb_col_set_value,
	(ib_cb_t*) &ib_cb_col_get_value,
	(ib_cb_t*) &ib_cb_col_get_meta,
	(ib_cb_t*) &ib_cb_trx_begin,
	(ib_cb_t*) &ib_cb_trx_commit,
	(ib_cb_t*) &ib_cb_trx_rollback,
	(ib_cb_t*) &ib_cb_cursor_lock,
	(ib_cb_t*) &ib_cb_cursor_close,
	(ib_cb_t*) &ib_cb_cursor_new_trx,
	(ib_cb_t*) &ib_cb_cursor_reset,
	(ib_cb_t*) &ib_cb_cursor_create,
	(ib_cb_t*) &ib_cb_open_table_by_name,
	(ib_cb_t*) &ib_cb_col_get_name,
	(ib_cb_t*) &ib_cb_table_truncate,
	(ib_cb_t*) &ib_cb_cursor_open_index_using_name
};



static inline
struct innodb_engine*
innodb_handle(
/*==========*/
	ENGINE_HANDLE*	handle) 
{
	return (struct innodb_engine*) handle;
}

static inline
struct default_engine*
default_handle(
/*===========*/
	struct innodb_engine*	eng) 
{
	return (struct default_engine*) eng->m_default_engine;
}

/****** Gateway to the default_engine's create_instance() function */
ENGINE_ERROR_CODE
create_my_default_instance(
/*=======================*/
	uint64_t,
	GET_SERVER_API,
	ENGINE_HANDLE **);


/*********** FUNCTIONS IMPLEMENTING THE PUBLISHED API BEGIN HERE ********/

ENGINE_ERROR_CODE
create_instance(
/*============*/
	uint64_t		interface, 
	GET_SERVER_API		get_server_api,
	ENGINE_HANDLE**		handle )
{
  
	ENGINE_ERROR_CODE e;

	SERVER_HANDLE_V1 *api = get_server_api();

	if (interface != 1 || api == NULL) {
		return ENGINE_ENOTSUP;
	}

	struct innodb_engine *innodb_eng = malloc(sizeof(struct innodb_engine)); 

	if(innodb_eng == NULL) {
		return ENGINE_ENOMEM;
	}

	logger = get_stderr_logger();

	innodb_eng->engine.interface.interface = 1;
	innodb_eng->engine.get_info        = innodb_get_info;
	innodb_eng->engine.initialize      = innodb_initialize;
	innodb_eng->engine.destroy         = innodb_destroy;
	innodb_eng->engine.allocate        = innodb_allocate;
	innodb_eng->engine.remove          = innodb_remove;
	innodb_eng->engine.release         = innodb_release;
	innodb_eng->engine.get             = innodb_get;
	innodb_eng->engine.get_stats       = innodb_get_stats;
	innodb_eng->engine.reset_stats     = innodb_reset_stats;
	innodb_eng->engine.store           = innodb_store;
	innodb_eng->engine.arithmetic      = innodb_arithmetic;
	innodb_eng->engine.flush           = innodb_flush;
	innodb_eng->engine.unknown_command = innodb_unknown_command;
	innodb_eng->engine.item_set_cas    = item_set_cas;           /* reused */
	innodb_eng->engine.get_item_info   = innodb_get_item_info; 
	innodb_eng->engine.get_stats_struct = NULL;
	innodb_eng->engine.errinfo = NULL;

	innodb_eng->server = *api;
	innodb_eng->get_server_api = get_server_api;

	/* configuration, with default values*/
	innodb_eng->info.info.description = "InnoDB Memcache " VERSION;
	innodb_eng->info.info.num_features = 3;
	innodb_eng->info.info.features[0].feature = ENGINE_FEATURE_CAS;
	innodb_eng->info.info.features[1].feature = ENGINE_FEATURE_PERSISTENT_STORAGE;
	innodb_eng->info.info.features[0].feature = ENGINE_FEATURE_LRU;

	/* Now call create_instace() for the default engine */
	e = create_my_default_instance(interface, get_server_api, 
				 & (innodb_eng->m_default_engine));
	if(e != ENGINE_SUCCESS) return e;

	innodb_eng->initialized = true;

	*handle = (ENGINE_HANDLE*) &innodb_eng->engine;

	return ENGINE_SUCCESS;
}


/*** get_info ***/
static const engine_info* innodb_get_info(ENGINE_HANDLE* handle)
{
  return & innodb_handle(handle)->info.info;
}

/*** initialize ***/

static void register_innodb_cb(uchar* p)
{
	int	i;
	int	array_size; 
	ib_cb_t*func_ptr = (ib_cb_t*) p;

	array_size = sizeof(innodb_memcached_api) / sizeof(ib_cb_t);

	for (i = 0; i < array_size; i++) {
		*innodb_memcached_api[i] = *(ib_cb_t*)func_ptr;
		func_ptr++;
	}
}

static ENGINE_ERROR_CODE innodb_initialize(ENGINE_HANDLE* handle,
                                        const char* config_str) 
{   
	int		nthreads;
	ENGINE_ERROR_CODE return_status;
	struct innodb_engine *innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
  
	register_innodb_cb((void*) config_str);

	UT_LIST_INIT(innodb_eng->conn_data);
	pthread_mutex_init(&innodb_eng->conn_mutex, NULL);

	/* Initalize the debug library */
	DEBUG_INIT(NULL, innodb_eng->startup_options.debug_enable);
	DEBUG_ENTER();

	/* Fetch some settings from the memcached core */
	if (!fetch_core_settings(innodb_eng, def_eng)) {
		return(ENGINE_FAILED);
	}

	nthreads = innodb_eng->server_options.nthreads;

	logger->log(LOG_WARNING, NULL, "Server started with %d threads.\n", nthreads);
	logger->log(LOG_WARNING, NULL, "Priming the pump ... ");


	/* Now initialize the default engine with no options
	(it already has them) */
	return_status = def_eng->engine.initialize(
			innodb_eng->m_default_engine, "");

	return return_status;
}

#define	ut_ad	while (0)
#define ut_a	while (0)
static
int
innodb_conn_clean(
/*==============*/
	innodb_engine_t*	engine,
	bool			clear_all)
{
	innodb_conn_data_t*	conn_data;
	innodb_conn_data_t*	next_conn_data;
	int			num_freed = 0;

	pthread_mutex_lock(&engine->conn_mutex);
	conn_data = UT_LIST_GET_FIRST(engine->conn_data);

	while (conn_data) {
		bool	stale_data = FALSE;
		next_conn_data = UT_LIST_GET_NEXT(c_list, conn_data);

		if (!clear_all && !conn_data->c_in_use) {
			innodb_conn_data_t*	check_data;
			void*		cookie = conn_data->c_cookie;
			check_data = engine->server.cookie->get_engine_specific(
				cookie);

			/* The check data is the original conn_data stored
			in connection "cookie", it can be set to NULL if
			connection closed, or to a new conn_data if it is
			closed and reopened. So verify and see if our
			current conn_data is stale */
			if (!check_data || check_data != conn_data) {
				stale_data = TRUE;
			}
		}

		/* Either we are clearing all conn_data or this conn_data is
		not in use */
		if (clear_all || stale_data) {
			UT_LIST_REMOVE(c_list, engine->conn_data, conn_data);

			if (conn_data->c_idx_crsr) {
				ib_cb_cursor_close(conn_data->c_idx_crsr);
			}

			if (conn_data->c_crsr) {
				ib_cb_cursor_close(conn_data->c_crsr);
			}

			free(conn_data);
			num_freed++;
		}

		conn_data = next_conn_data;
	}
	pthread_mutex_unlock(&engine->conn_mutex);

	return(num_freed);
}

/*** destroy ***/
static void innodb_destroy(ENGINE_HANDLE* handle, bool force) 
{
	DEBUG_ENTER();

	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);

	innodb_conn_clean(innodb_eng, TRUE);

	pthread_mutex_destroy(&innodb_eng->conn_mutex);

	def_eng->engine.destroy(innodb_eng->m_default_engine, force);
}


/*** allocate ***/

/* Allocate gets a struct item from the slab allocator, and fills in 
   everything but the value.  It seems like we can just pass this on to 
   the default engine; we'll intercept it later in store().
   This is also called directly from finalize_read() in the commit thread.
*/
static ENGINE_ERROR_CODE innodb_allocate(ENGINE_HANDLE* handle,
                                           const void* cookie,
                                           item **item,
                                           const void* key,
                                           const size_t nkey,
                                           const size_t nbytes,
                                           const int flags,
                                           const rel_time_t exptime)
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);

	/* We use default engine's memory allocator to allocate memory
	for item */
	return def_eng->engine.allocate(innodb_eng->m_default_engine,
					cookie, item, key, nkey, nbytes,
					flags, exptime);
}


static
innodb_conn_data_t*
innodb_conn_init(
/*=============*/
	innodb_engine_t*	engine,
	const void*		cookie,
	ib_trx_t		ib_trx,
	ib_lck_mode_t		lock_mode)
{
	innodb_conn_data_t*	conn_data;
	meta_info_t*		meta_info = &engine->meta_info;
	meta_index_t*		meta_index = &meta_info->m_index;
	ib_crsr_t		idx_crsr;
	ib_err_t		err = DB_SUCCESS;

	conn_data = engine->server.cookie->get_engine_specific(cookie);

	if (!conn_data) {
		if (UT_LIST_GET_LEN(engine->conn_data) > 1024) {
			innodb_conn_clean(engine, FALSE);
		}

		conn_data = malloc(sizeof(*conn_data));
		memset(conn_data, 0, sizeof(*conn_data));

		err = innodb_api_begin(engine,
				       meta_info->m_item[META_DB].m_str,
				       meta_info->m_item[META_TABLE].m_str,
				       ib_trx, &conn_data->c_crsr,
				       &conn_data->c_idx_crsr,
				       lock_mode);

		conn_data->c_in_use = FALSE;
		conn_data->c_cookie = cookie;

		engine->server.cookie->store_engine_specific(
			cookie, conn_data);

		pthread_mutex_lock(&engine->conn_mutex);
		UT_LIST_ADD_LAST(c_list, engine->conn_data, conn_data);
	} else {
		ib_crsr_t	crsr;
		ib_crsr_t	idx_crsr;

		crsr = conn_data->c_crsr;
		ib_cb_cursor_new_trx(crsr, ib_trx);
		ib_cb_cursor_lock(crsr, lock_mode);
		if (meta_index->m_use_idx == META_SECONDARY) {
			idx_crsr = conn_data->c_idx_crsr;
			ib_cb_cursor_new_trx(meta_index->m_idx_crsr,
					     ib_trx);
			ib_cb_cursor_lock(meta_index->m_idx_crsr,
					  lock_mode);
		}

		pthread_mutex_lock(&engine->conn_mutex);
	}

	assert(!conn_data->c_in_use);
	conn_data->c_in_use = TRUE;
	pthread_mutex_unlock(&engine->conn_mutex);

	return(conn_data);
}

/*** remove ***/
static
ENGINE_ERROR_CODE
innodb_remove(
/*==========*/
	ENGINE_HANDLE* handle,
	const void* cookie,
	const void* key,
	const size_t nkey,
	uint64_t cas,
	uint16_t vbucket)
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	//struct default_engine*def_eng = default_handle(innodb_eng);
	ib_err_t		innodb_err;
	ENGINE_ERROR_CODE	err;
	ib_trx_t		ib_trx;
	innodb_conn_data_t*	conn_data;

	ib_trx = ib_cb_trx_begin(IB_TRX_READ_UNCOMMITTED);

	conn_data = innodb_conn_init(innodb_eng, cookie, ib_trx,
				     IB_LOCK_IX);

	/* In the binary protocol there is such a thing as a CAS delete.
	This is the CAS check. If we will also be deleting from the database,
	there are two possibilities:
	  1: The CAS matches; perform the delete.
	  2: The CAS doesn't match; delete the item because it's stale.
	Therefore we skip the check altogether if(do_db_delete) */

	err = innodb_api_delete(innodb_eng, conn_data, key, nkey);

	innodb_api_cursor_reset(conn_data);
	innodb_err = ib_cb_trx_commit(ib_trx);

	return(err);
}


/*** release ***/
static void innodb_release(ENGINE_HANDLE* handle, const void *cookie,
                        item* item)
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	//struct default_engine*def_eng = default_handle(innodb_eng);
	innodb_conn_data_t*	conn_data;

	conn_data = innodb_eng->server.cookie->get_engine_specific(cookie);
	assert(conn_data);

	pthread_mutex_lock(&innodb_eng->conn_mutex);
        //assert(conn_data->c_in_use);
        conn_data->c_in_use = FALSE; 
        pthread_mutex_unlock(&innodb_eng->conn_mutex);

	if (item) {
//		item_release(def_eng, (hash_item *) item);
	}

	return;
}  

/*** get ***/
static ENGINE_ERROR_CODE innodb_get(ENGINE_HANDLE* handle,
                                 const void* cookie,
                                 item** item,
                                 const void* key,
                                 const int nkey,
                                 uint16_t vbucket)
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	hash_item*		it = NULL;
	ib_trx_t		ib_trx;
	ib_crsr_t		crsr;
	ib_err_t		err = DB_SUCCESS;
	mci_item_t		result;
	ENGINE_ERROR_CODE	err_ret = ENGINE_SUCCESS;
	uint64_t		cas = 0;
	uint64_t		exp = 0;
	uint64_t		flags = 0;
	innodb_conn_data_t*	conn_data;

	ib_trx = ib_cb_trx_begin(IB_TRX_READ_UNCOMMITTED);

	conn_data = innodb_conn_init(innodb_eng, cookie, ib_trx,
				     IB_LOCK_IX);

	err = innodb_api_search(innodb_eng, conn_data, &crsr, key,
				nkey, &result, NULL);

	if (err != DB_SUCCESS) {
		err_ret = ENGINE_KEY_ENOENT;
		goto func_exit;
	}

	/* Only if expiration field is enabled, and the value is not zero,
	we will check whether the item is expired */
	if (result.mci_item[MCI_COL_EXP].m_enabled
	    && result.mci_item[MCI_COL_EXP].m_digit) {
		uint64_t		time;
		time = mci_get_time();

		if (time > result.mci_item[MCI_COL_EXP].m_digit) {
			err_ret = ENGINE_KEY_ENOENT;
			goto func_exit;
		}
	}

	if (result.mci_item[MCI_COL_FLAG].m_enabled) {
		flags = ntohl(result.mci_item[MCI_COL_FLAG].m_digit);
	}

	if (result.mci_item[MCI_COL_CAS].m_enabled) {
		cas = result.mci_item[MCI_COL_CAS].m_digit;
	}

	if (result.mci_item[MCI_COL_EXP].m_enabled) {
		exp = result.mci_item[MCI_COL_EXP].m_digit;
	}

	innodb_allocate(handle, cookie, item, key, nkey,
			result.mci_item[MCI_COL_VALUE].m_len, flags, exp);

        it = *item;

	if (it->iflag & ITEM_WITH_CAS) {
		hash_item_set_cas(it, cas);
	}

        it->nbytes = result.mci_item[MCI_COL_VALUE].m_len;
	memcpy(hash_item_get_data(it),
	       result.mci_item[MCI_COL_VALUE].m_str, it->nbytes);

func_exit:
	innodb_api_cursor_reset(conn_data);
	err = ib_cb_trx_commit(ib_trx);

	return(err_ret);
}


/*** get_stats ***/
static ENGINE_ERROR_CODE innodb_get_stats(ENGINE_HANDLE* handle,
                                       const void *cookie,
                                       const char *stat_key,
                                       int nkey,
                                       ADD_STAT add_stat)
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
	return def_eng->engine.get_stats(innodb_eng->m_default_engine, cookie,
					 stat_key, nkey, add_stat);
}


/*** reset_stats ***/
static void innodb_reset_stats(ENGINE_HANDLE* handle, 
                            const void *cookie)
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
	def_eng->engine.reset_stats(innodb_eng->m_default_engine, cookie);
}


/*** store ***/

static ENGINE_ERROR_CODE innodb_store(ENGINE_HANDLE* handle,
                                   const void *cookie,
                                   item* item,
                                   uint64_t *cas,
                                   ENGINE_STORE_OPERATION op,
                                   uint16_t vbucket)
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	uint16_t		len = hash_item_get_key_len(item);
	char*			value = hash_item_get_key(item);
	uint64			exptime = hash_item_get_exp(item);
	uint64			flags = hash_item_get_flag(item);
	ENGINE_ERROR_CODE	result;
	ib_trx_t		ib_trx;
	uint64_t		input_cas;
	innodb_conn_data_t*	conn_data;

	ib_trx = ib_cb_trx_begin(IB_TRX_READ_UNCOMMITTED);

	conn_data = innodb_conn_init(innodb_eng, cookie, ib_trx,
				     IB_LOCK_IX);

	input_cas = hash_item_get_cas(item);

	result = innodb_api_store(innodb_eng, conn_data, value, len,
				  exptime, cas, input_cas, flags, op);

	innodb_api_cursor_reset(conn_data);
	ib_cb_trx_commit(ib_trx);

	/* write to cache */
	//return store_item(default_handle(innodb_eng), item, cas, op, cookie);
  
	/* NOP case: db_write and mc_write are both disabled. */
	return(result);  
}


/*** arithmetic ***/
static ENGINE_ERROR_CODE innodb_arithmetic(ENGINE_HANDLE* handle,
                                        const void* cookie,
                                        const void* key,
                                        const int nkey,
                                        const bool increment,
                                        const bool create,
                                        const uint64_t delta,
                                        const uint64_t initial,
                                        const rel_time_t exptime,
                                        uint64_t *cas,
                                        uint64_t *result,
                                        uint16_t vbucket)
{
	struct innodb_engine*	innodb_eng = innodb_handle(handle);
	//struct default_engine*def_eng = default_handle(innodb_eng);
	ib_trx_t		ib_trx;
	innodb_conn_data_t*	conn_data;

	ib_trx = ib_cb_trx_begin(IB_TRX_READ_UNCOMMITTED);

	conn_data = innodb_conn_init(innodb_eng, cookie, ib_trx,
				     IB_LOCK_IX);

	innodb_api_arithmetic(innodb_eng, conn_data, key, nkey, delta,
			      increment, cas, exptime, create, initial,
			      result);

	innodb_api_cursor_reset(conn_data);
	ib_cb_trx_commit(ib_trx);

	return ENGINE_SUCCESS;


	/* For cache-only prefixes, forward this to the default engine */
	/*
	if(! prefix.use_ndb) {
	return def_eng->engine.arithmetic(innodb_eng->m_default_engine, cookie,
				      key, nkey, increment, create, 
				      delta, initial, exptime, cas,
				      result, vbucket);    
	}
	*/
}


/*** flush ***/
static ENGINE_ERROR_CODE innodb_flush(ENGINE_HANDLE* handle,
                                   const void* cookie, time_t when)
{                                   
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);
	ENGINE_ERROR_CODE	err = ENGINE_SUCCESS;

	err = innodb_api_flush(
		innodb_eng->meta_info.m_item[META_DB].m_str,
		innodb_eng->meta_info.m_item[META_TABLE].m_str);
	
	/* default engine flush */
	//return def_eng->engine.flush(innodb_eng->m_default_engine, cookie, when);
	return(err);
}


/*** unknown_command ***/
static ENGINE_ERROR_CODE innodb_unknown_command(ENGINE_HANDLE* handle,
                                             const void* cookie,
                                             protocol_binary_request_header *request,
                                             ADD_RESPONSE response)
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);
	struct default_engine *def_eng = default_handle(innodb_eng);

	return def_eng->engine.unknown_command(innodb_eng->m_default_engine,
					       cookie, request, response);
}


/*** get_item_info ***/
static bool innodb_get_item_info(ENGINE_HANDLE *handle, 
                              const void *cookie,
                              const item* item, 
                              item_info *item_info)
{
	struct innodb_engine* innodb_eng = innodb_handle(handle);

	if (item_info->nvalue < 1) {
	THREAD_DEBUG_PRINT("nvalue too small.");
	return false;
	}
	/* Use a hash item */
	hash_item *it = (hash_item*) item;
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
	THREAD_DEBUG_PRINT("hash_item [KEY: %s][CAS: %llu].", 
		       hash_item_get_key(it), hash_item_get_cas(it));
	return true;
}


/* read_cmdline_options requires duplicating code from the default engine. 
   If the default engine supports a new option, you will need to add it here.
   We create a single config_items structure containing options for both 
   engines.
*/
void read_cmdline_options(struct innodb_engine *ndb, struct default_engine *se,
                          const char * conf)
{
  DEBUG_ENTER();
  int did_parse = 0;   /* 0 = success from parse_config() */
  if (conf != NULL) {
    struct config_item items[] = {
      /* DEFAULT ENGINE OPTIONS */
      { .key = "use_cas",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.use_cas },
      { .key = "verbose",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.verbose },
      { .key = "eviction",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.evict_to_free },
      { .key = "cache_size",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.maxbytes },
      { .key = "preallocate",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.preallocate },
      { .key = "factor",
        .datatype = DT_FLOAT,
        .value.dt_float = &se->config.factor },
      { .key = "chunk_size",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.chunk_size },
      { .key = "item_size_max",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.item_size_max },
      { .key = "config_file",
        .datatype = DT_CONFIGFILE },
      { .key = NULL}
    };
    
    did_parse = se->server.core->parse_config(conf, items, stderr);
  }
  switch(did_parse) {
      case -1:
        logger->log(LOG_WARNING, NULL, 
                    "Unknown tokens in config string \"%s\"\n", conf);
        break;
      case 1:
        logger->log(LOG_WARNING, NULL, 
                    "Illegal values in config string: \"%s\"\n", conf);
        break;
      case 0: /* success */
        break;
  }
}


int fetch_core_settings(struct innodb_engine *engine,
                         struct default_engine *se) {
	DEBUG_ENTER();

	/* Set up a struct config_item containing the keys
	we're interested in. */
	struct config_item items[] = {    
		{ .key = "cas_enabled",
		.datatype = DT_BOOL,
		.value.dt_bool = &engine->server_options.cas_enabled },
		{ .key = "num_threads",
		.datatype = DT_SIZE,
		.value.dt_size = &engine->server_options.nthreads },
		{ .key = NULL }
	};
  
	/* InnoDB related configuration setup */
	if (!innodb_config(&engine->meta_info)) {
		return(FALSE);
	}

	if (!innodb_verify(&engine->meta_info)) {
		return(FALSE);
	}

	return(TRUE);
}

