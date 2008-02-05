#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation  // gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "mysql_priv.h"

#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION

#include "tokudb_config.h"

/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif

#include "tokudb_probes.h"

#include "ha_tokudb.h"
#include <mysql/plugin.h>

static handler *tdb_create_handler(handlerton *hton,
                                   TABLE_SHARE *table,
                                   MEM_ROOT *mem_root);

handlerton *tdb_hton;

typedef struct st_tokudb_trx_data {
  DB_TXN *all;
  DB_TXN *stmt;
  DB_TXN *sp_level;
  uint tdb_lock_count;
} tokudb_trx_data;

#define HA_TOKUDB_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_TOKUDB_RANGE_COUNT   100
#define HA_TOKUDB_MAX_ROWS      10000000 /* Max rows in table */
/* extra rows for estimate_rows_upper_bound() */
#define HA_TOKUDB_EXTRA_ROWS    100

/* Bits for share->status */
#define STATUS_PRIMARY_KEY_INIT 1
#define STATUS_ROW_COUNT_INIT   2
#define STATUS_TDB_ANALYZE      4

const u_int32_t tdb_DB_TXN_NOSYNC= DB_TXN_NOSYNC;
const u_int32_t tdb_DB_RECOVER= DB_RECOVER;
const u_int32_t tdb_DB_PRIVATE= DB_PRIVATE;
// const u_int32_t tdb_DB_DIRECT_DB= DB_DIRECT_DB;
// const u_int32_t tdb_DB_DIRECT_LOG= DB_DIRECT_LOG;
const char *ha_tokudb_ext= ".tdb";

static my_bool tokudb_shared_data= FALSE;
static u_int32_t tokudb_init_flags= DB_PRIVATE | DB_RECOVER;
static u_int32_t tokudb_env_flags= DB_LOG_AUTOREMOVE;
static u_int32_t tokudb_lock_type= DB_LOCK_DEFAULT;
static ulong tokudb_log_buffer_size= 0;
static ulong tokudb_log_file_size= 0;
static ulonglong tokudb_cache_size= 0;
static char *tokudb_home;
static char *tokudb_tmpdir;
static char *tokudb_logdir;
static long tokudb_lock_scan_time= 0;
static ulong tokudb_region_size= 0;
static ulong tokudb_cache_parts= 1;
static ulong tokudb_trans_retry= 1;
static ulong tokudb_max_lock;

static DB_ENV *db_env;

static const char tokudb_hton_name[]= "TokuDB";
static const int tokudb_hton_name_length=sizeof(tokudb_hton_name)-1;

const char *tokudb_lock_names[] =
{ "DEFAULT", "OLDEST", "RANDOM", "YOUNGEST", "EXPIRE", "MAXLOCKS",
  "MAXWRITE", "MINLOCKS", "MINWRITE", 0 };
#if 0
u_int32_t tokudb_lock_types[]=
{ DB_LOCK_DEFAULT, DB_LOCK_OLDEST, DB_LOCK_RANDOM, DB_LOCK_YOUNGEST,
  DB_LOCK_EXPIRE, DB_LOCK_MAXLOCKS, DB_LOCK_MAXWRITE, DB_LOCK_MINLOCKS,
  DB_LOCK_MINWRITE };
#endif
TYPELIB tokudb_lock_typelib= {array_elements(tokudb_lock_names)-1, "",
                                tokudb_lock_names, NULL};

static void tokudb_print_error(const DB_ENV *db_env, const char *db_errpfx,
                                 const char *buffer);
static void tokudb_cleanup_log_files(void);
static TOKUDB_SHARE *get_share(const char *table_name, TABLE *table);
static int free_share(TOKUDB_SHARE *share,
                      TABLE *table,
                      uint hidden_primary_key,
                      bool mutex_is_locked);
static int write_status(DB *status_block, char *buff, uint length);
static void update_status(TOKUDB_SHARE *share, TABLE *table);
static int tokudb_end(handlerton *hton, ha_panic_function type);
static bool tokudb_flush_logs(handlerton *hton);
static bool tokudb_show_status(handlerton *hton,
                                 THD *thd,
                                 stat_print_fn *print,
                                 enum ha_stat_type);
static int tokudb_close_connection(handlerton *hton, THD *thd);
static int tokudb_commit(handlerton *hton, THD *thd, bool all);
static int tokudb_rollback(handlerton *hton, THD *thd, bool all);
static int tokudb_rollback_to_savepoint(handlerton *hton, THD* thd,
                                          void *savepoint);
static int tokudb_savepoint(handlerton *hton, THD* thd,
                              void *savepoint);
static int tokudb_release_savepoint(handlerton *hton, THD* thd,
                                      void *savepoint);
static bool tokudb_show_logs(THD *thd, stat_print_fn *stat_print);

static HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;

static uchar* tdb_get_key(TOKUDB_SHARE *share, size_t *length,
                          my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (uchar*) share->table_name;
}

// declare a thread specific variable
// stole this from my_thr_init.h with MontyW's advice
#ifdef USE_TLS
pthread_key(void**, tdb_thr_private);
#else
pthread_key(void*, tdb_thr_private);
#endif /* USE_TLS */

int tokudb_thr_private_create (void)
{
  if ((pthread_key_create(&tdb_thr_private, NULL)) != 0)
    return 1;
  return 0;
}

void *tokudb_thr_private_get (void)
{
  void **tmp;
  tmp= my_pthread_getspecific(void**, tdb_thr_private);
  if (tmp == NULL) return NULL;
  return *tmp;
}

void tokudb_thr_private_set (void *v)
{
  // if i alloc'ed v to store here,
  //  make SURE to set a thread cleanup function to de-alloc it
  //  because the thread may terminate unexpectedly early
  pthread_setspecific(tdb_thr_private, v);
  return;
}

static int tdb_init_func(void *p)
{
  DBUG_ENTER("tdb_init_func");

  tdb_hton= (handlerton *)p;

  if (tokudb_thr_private_create()) {
    DBUG_RETURN(TRUE);
  }

  VOID(pthread_mutex_init(&tokudb_mutex, MY_MUTEX_INIT_FAST));
  (void) hash_init(&tokudb_open_tables, system_charset_info, 32, 0, 0,
		   (hash_get_key) tdb_get_key, 0, 0);

  tdb_hton->state= SHOW_OPTION_YES;
  // tdb_hton->flags= HTON_CAN_RECREATE;  // this came from skeleton
  tdb_hton->flags= HTON_CLOSE_CURSORS_AT_COMMIT | HTON_FLUSH_AFTER_RENAME;
#ifdef DB_TYPE_TOKUDB
  tdb_hton->db_type= DB_TYPE_TOKUDB;  // obsolete?
#endif

  tdb_hton->create= tdb_create_handler;
  tdb_hton->savepoint_offset= sizeof(DB_TXN *);
  tdb_hton->close_connection= tokudb_close_connection;
  tdb_hton->savepoint_set= tokudb_savepoint;
  tdb_hton->savepoint_rollback= tokudb_rollback_to_savepoint;
  tdb_hton->savepoint_release= tokudb_release_savepoint;
  tdb_hton->commit= tokudb_commit;
  tdb_hton->rollback= tokudb_rollback;
  tdb_hton->panic= tokudb_end;
  tdb_hton->flush_logs= tokudb_flush_logs;
  tdb_hton->show_status= tokudb_show_status;

  if (!tokudb_tmpdir)
    tokudb_tmpdir= mysql_tmpdir;
  DBUG_PRINT("info", ("tokudb_tmpdir: %s", tokudb_tmpdir));
  if (!tokudb_home)
    tokudb_home= mysql_real_data_home;
  DBUG_PRINT("info", ("tokudb_home: %s", tokudb_home));

  /*
    If we don't set set_lg_bsize() we will get into trouble when
    trying to use many open BDB tables.
    If log buffer is not set, assume that the we will need 512 byte per
    open table.  This is a number that we have reached by testing.
  */
  if (!tokudb_log_buffer_size)
  {
    tokudb_log_buffer_size= max(table_cache_size*512, 32*1024);
    DBUG_PRINT("info",("computing tokudb_log_buffer_size %ld\n",
		       tokudb_log_buffer_size));
  }
  /*
    Tokudb DB require that
    tokudb_log_file_size >= tokudb_log_buffer_size*4
  */
  tokudb_log_file_size= tokudb_log_buffer_size*4;
  tokudb_log_file_size= MY_ALIGN(tokudb_log_file_size, 1024*1024L);
  tokudb_log_file_size= max(tokudb_log_file_size, 10*1024*1024L);
  DBUG_PRINT("info",("computing tokudb_log_file_size: %ld\n",
		     tokudb_log_file_size));

  if (db_env_create(&db_env, 0))
    goto error;
  db_env->set_errcall(db_env, tokudb_print_error);
  db_env->set_errpfx(db_env, "tokudbdb");
  DBUG_PRINT("info",("tokudb_tmpdir: %s\n", tokudb_tmpdir));
  db_env->set_tmp_dir(db_env, tokudb_tmpdir);
  DBUG_PRINT("info",("mysql_data_home: %s\n", mysql_data_home));
  db_env->set_data_dir(db_env, mysql_data_home);
  DBUG_PRINT("info",("tokudb_env_flags: 0x%lx\n", tokudb_env_flags));
  db_env->set_flags(db_env, tokudb_env_flags, 1);
  if (tokudb_logdir) {
    db_env->set_lg_dir(db_env, tokudb_logdir);
    DBUG_PRINT("info",("tokudb_logdir: %s\n", tokudb_logdir));
  }

  DBUG_PRINT("info",("tokudb_cache_size: %lld\n", tokudb_cache_size));
  DBUG_PRINT("info",("tokudb_cache_parts: %ld\n", tokudb_cache_parts));
  if (tokudb_cache_size > (uint) ~0)
    db_env->set_cachesize(db_env, tokudb_cache_size / (1024*1024L*1024L),
			  tokudb_cache_size % (1024L*1024L*1024L),
			  tokudb_cache_parts);
  else
    db_env->set_cachesize(db_env, 0, tokudb_cache_size, tokudb_cache_parts);


  DBUG_PRINT("info",("tokudb_log_file_size: %ld\n", tokudb_log_file_size));
  db_env->set_lg_max(db_env, tokudb_log_file_size);
  DBUG_PRINT("info",("tokudb_log_buffer_size: %ld\n", tokudb_log_buffer_size));
  db_env->set_lg_bsize(db_env, tokudb_log_buffer_size);
  DBUG_PRINT("info",("tokudb_lock_type: 0x%lx\n", tokudb_lock_type));
  db_env->set_lk_detect(db_env, tokudb_lock_type);
  // DBUG_PRINT("info",("tokudb_region_size: %ld\n", tokudb_region_size));
  // db_env->set_lg_regionmax(db_env, tokudb_region_size);
  // set_lk_max deprecated, use set_lk_max_locks
  // if (tokudb_max_lock) {
  //  DBUG_PRINT("info",("tokudb_max_lock: %ld\n", tokudb_max_lock));
  //  db_env->set_lk_max_locks(db_env, tokudb_max_lock);
  // }

  if (db_env->open(db_env,
 		   tokudb_home,
		   tokudb_init_flags |  DB_INIT_LOCK |
		   DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN |
		   DB_CREATE | DB_THREAD, 0666))
  {
    db_env->close(db_env, 0);
    db_env= 0;
    goto error;
  }

  DBUG_RETURN(FALSE);
error:
  DBUG_RETURN(TRUE);
}

static int tdb_done_func(void *p)
{
  DBUG_ENTER("tdb_done_func");
  int error= 0;

  if (tokudb_open_tables.records)
    error= 1;
  hash_free(&tokudb_open_tables);
  pthread_mutex_destroy(&tokudb_mutex);
  pthread_key_delete(tdb_thr_private);

  DBUG_RETURN(0);
}

/** @brief
    Simple lock controls. The "share" it creates is a structure we will
    pass to each tokudb handler. Do you have to have one of these? Well, you have
    pieces that are used for locking, and they are needed to function.
*/
static TOKUDB_SHARE *get_share(const char *table_name, TABLE *table)
{
  TOKUDB_SHARE *share;
  uint length;
  char *tmp_name;

  pthread_mutex_lock(&tokudb_mutex);
  length= (uint) strlen(table_name);

  if (!(share=(TOKUDB_SHARE*) hash_search(&tokudb_open_tables,
					    (uchar*) table_name,
					    length)))
  {
    ulong *rec_per_key;
    char *tmp_name;
    DB **key_file;
    u_int32_t *key_type;
    uint keys= table->s->keys;

    if (!(share=(TOKUDB_SHARE *)
	  my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
			  &share, sizeof(*share),
			  &tmp_name, length+1,
			  &rec_per_key, keys * sizeof(ha_rows),
			  &key_file, (keys+1) * sizeof(*key_file),
			  &key_type, (keys+1) * sizeof(u_int32_t),
			  NullS)))
    {
      pthread_mutex_unlock(&tokudb_mutex);
      return NULL;
    }

    share->use_count= 0;
    share->table_name_length= length;
    share->table_name= tmp_name;
    strmov(share->table_name, table_name);

    share->rec_per_key= rec_per_key;
    share->key_file= key_file;
    share->key_type= key_type;

    if (my_hash_insert(&tokudb_open_tables, (uchar*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  }
  pthread_mutex_unlock(&tokudb_mutex);

  return share;

error:
  pthread_mutex_destroy(&share->mutex);
  my_free((uchar *) share, MYF(0));

  return NULL;
}

static int free_share(TOKUDB_SHARE *share,
                      TABLE *table,
                      uint hidden_primary_key,
                      bool mutex_is_locked)
{
  int error, result= 0;
  uint keys= table->s->keys + test(hidden_primary_key);

  pthread_mutex_lock(&tokudb_mutex);

  if (mutex_is_locked)
    pthread_mutex_unlock(&share->mutex);
  if (!--share->use_count)
  {
    DBUG_PRINT("info",("share->use_count %u", share->use_count));
    DB **key_file= share->key_file;

    /* this does share->file->close() implicitly */
    update_status(share, table);

    for (uint i=0; i < keys; i++)
    {
      if (key_file[i] && (error=key_file[i]->close(key_file[i],0)))
	result= error;
    }

    if (share->status_block &&
	(error= share->status_block->close(share->status_block,0)))
      result= error;

    hash_delete(&tokudb_open_tables, (uchar*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((uchar *) share, MYF(0));
  }
  pthread_mutex_unlock(&tokudb_mutex);

  return result;
}

static handler* tdb_create_handler(handlerton *hton,
                                   TABLE_SHARE *table,
                                   MEM_ROOT *mem_root)
{
  return new (mem_root) ha_tokudb(hton, table);
}

int tokudb_end(handlerton *hton, ha_panic_function type)
{
  DBUG_ENTER("tokudb_end");
  int error= 0;
  if (db_env)
  {
    tokudb_cleanup_log_files();
    error= db_env->close(db_env,0);  // Error is logged
    db_env= 0;
#if 0 /* shutdown crash */
    hash_free(&tokudb_open_tables);
    pthread_mutex_destroy(&tokudb_mutex);
#endif
  }
  DBUG_RETURN(error);
}

static int tokudb_close_connection(handlerton *hton, THD *thd)
{
  my_free((void *)thd->ha_data[hton->slot], MYF(0));
  return 0;
}

bool tokudb_flush_logs(handlerton *hton)
{
  DBUG_ENTER("tokudb_flush_logs");
  int error;
  bool result= 0;
  if ((error=db_env->log_flush(db_env,0)))
  {
    my_error(ER_ERROR_DURING_FLUSH_LOGS,MYF(0),error);
    result= 1;
  }
  if ((error=db_env->txn_checkpoint(db_env,0,0,0)))
  {
    my_error(ER_ERROR_DURING_CHECKPOINT,MYF(0),error);
    result= 1;
  }
  DBUG_RETURN(result);
}

static int tokudb_commit(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("tokudb_commit");
  DBUG_PRINT("trans",("ending transaction %s", all ? "all" : "stmt"));
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[hton->slot];
  DB_TXN **txn= all ? &trx->all : &trx->stmt;
  int error= (*txn)->commit(*txn,0);
  *txn=0;
  DBUG_RETURN(error);
}

static int tokudb_rollback(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("tokudb_rollback");
  DBUG_PRINT("trans",("aborting transaction %s", all ? "all" : "stmt"));
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[hton->slot];
  DB_TXN **txn= all ? &trx->all : &trx->stmt;
  int error= (*txn)->abort(*txn);
  *txn=0;
  DBUG_RETURN(error);
}

static int tokudb_savepoint(handlerton *hton, THD* thd,
			      void *savepoint)
{
  DBUG_ENTER("tokudb_savepoint");
  int error;
  DB_TXN **save_txn= (DB_TXN**) savepoint;
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[hton->slot];
  if (!(error= db_env->txn_begin(db_env, trx->sp_level, save_txn, 0)))
  {
    trx->sp_level= *save_txn;
  }
  DBUG_RETURN(error);
}

static int tokudb_rollback_to_savepoint(handlerton *hton,
					  THD* thd,
					  void *savepoint)
{
  DBUG_ENTER("tokudb_rollback_to_savepoint");
#if 0
  int error;
  DB_TXN *parent, **save_txn= (DB_TXN**) savepoint;
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[hton->slot];
  parent= (*save_txn)->parent;
  if (!(error= (*save_txn)->abort(*save_txn)))
  {
    trx->sp_level= parent;
    error= tokudb_savepoint(hton, thd, savepoint);
  }
#else
  int error = EINVAL;
#endif
  DBUG_RETURN(error);
}

static int tokudb_release_savepoint(handlerton *hton, THD* thd, void *savepoint)
{
  DBUG_ENTER("tokudb_release_savepoint");
#if 0
  int error;
  DB_TXN *parent, **save_txn= (DB_TXN**) savepoint;
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[hton->slot];
  parent= (*save_txn)->parent;
  if (!(error= (*save_txn)->commit(*save_txn,0)))
  {
    trx->sp_level= parent;
    *save_txn= 0;
  }
#else
  int error = EINVAL;
#endif
  DBUG_RETURN(error);
}

static bool tokudb_show_logs(THD *thd, stat_print_fn *stat_print)
{
  DBUG_ENTER("tokudb_show_logs");
  char **all_logs, **free_logs, **a, **f;
  int error=1;
  MEM_ROOT **root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,THR_MALLOC);
  MEM_ROOT show_logs_root, *old_mem_root= *root_ptr;

  init_sql_alloc(&show_logs_root, BDB_LOG_ALLOC_BLOCK_SIZE,
		 BDB_LOG_ALLOC_BLOCK_SIZE);
  *root_ptr= &show_logs_root;
  all_logs= free_logs= 0;

  if ((error= db_env->log_archive(db_env, &all_logs,
				  DB_ARCH_ABS | DB_ARCH_LOG)) ||
      (error= db_env->log_archive(db_env, &free_logs, DB_ARCH_ABS)))
  {
    DBUG_PRINT("error", ("log_archive failed (error %d)", error));
    db_env->err(db_env, error, "log_archive: DB_ARCH_ABS");
    if (error== DB_NOTFOUND)
      error=0;  // No log files
    goto err;
  }
  /* Error is 0 here */
  if (all_logs)
  {
    for (a = all_logs, f = free_logs; *a; ++a)
    {
      if (f && *f && strcmp(*a, *f) == 0)
      {
	f++;
	if ((error= stat_print(thd, tokudb_hton_name,
			       tokudb_hton_name_length, *a, strlen(*a),
			       STRING_WITH_LEN(SHOW_LOG_STATUS_FREE))))
	  break;
      }
      else
      {
	if ((error= stat_print(thd, tokudb_hton_name,
			       tokudb_hton_name_length, *a, strlen(*a),
			       STRING_WITH_LEN(SHOW_LOG_STATUS_INUSE))))
	  break;
      }
    }
  }
err:
  if (all_logs)
    free(all_logs);
  if (free_logs)
    free(free_logs);
  free_root(&show_logs_root,MYF(0));
  *root_ptr= old_mem_root;
  DBUG_RETURN(error);
}

bool tokudb_show_status(handlerton *hton,
			  THD *thd,
			  stat_print_fn *stat_print,
                          enum ha_stat_type stat_type)
{
  switch (stat_type) {
  case HA_ENGINE_LOGS:
    return tokudb_show_logs(thd, stat_print);
  default:
    return FALSE;
  }
}

static void tokudb_print_error(const DB_ENV *db_env, const char *db_errpfx,
                                 const char *buffer)
{
  sql_print_error("%s:  %s", db_errpfx, buffer);
}

void tokudb_cleanup_log_files(void)
{
  DBUG_ENTER("tokudb_cleanup_log_files");
  char **names;
  int error;

  // by HF. Sometimes it crashes. TODO - find out why
#ifndef EMBEDDED_LIBRARY
  /* XXX: Probably this should be done somewhere else, and
   * should be tunable by the user. */
  if ((error = db_env->txn_checkpoint(db_env, 0, 0, 0)))
    my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
#endif
  if ((error = db_env->log_archive(db_env, &names, DB_ARCH_ABS)) != 0)
  {
    DBUG_PRINT("error", ("log_archive failed (error %d)", error));
    db_env->err(db_env, error, "log_archive: DB_ARCH_ABS");
    DBUG_VOID_RETURN;
  }

  if (names)
  {                                           
    char **np;                                
    for (np = names; *np; ++np)               
      my_delete(*np, MYF(MY_WME));            

    free(names);
  }

  DBUG_VOID_RETURN;
}

ha_tokudb::ha_tokudb(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg), alloc_ptr(0), rec_buff(0), file(0),
   int_table_flags(HA_REC_NOT_IN_SEQ | HA_FAST_KEY_READ |
		   HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS |
		   HA_PRIMARY_KEY_IN_READ_INDEX | HA_FILE_BASED |
		   HA_CAN_GEOMETRY |
		   HA_AUTO_PART_KEY | HA_TABLE_SCAN_ON_INDEX),
   changed_rows(0), last_dup_key((uint) -1), version(0), using_ignore(0)
{}

static const char *ha_tokudb_exts[]= {
  ha_tokudb_ext,
  NullS
};

const char **ha_tokudb::bas_ext() const
{
  return ha_tokudb_exts;
}

ulong ha_tokudb::index_flags(uint idx, uint part, bool all_parts) const
{
  ulong flags= (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_KEYREAD_ONLY
		| HA_READ_RANGE);
  for (uint i= all_parts ? 0 : part; i <= part; i++)
  {
    KEY_PART_INFO *key_part= table_share->key_info[idx].key_part+i;
    if (key_part->field->type() == FIELD_TYPE_BLOB)
    {
      /* We can't use BLOBS to shortcut sorts */
      flags&= ~(HA_READ_ORDER | HA_KEYREAD_ONLY | HA_READ_RANGE);
      break;
    }
    switch (key_part->field->key_type()) {
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2:
      /*
	As BDB stores only one copy of equal strings, we can't use key read
	on these. Binary collations do support key read though.
      */
      if (!(key_part->field->charset()->state & MY_CS_BINSORT))
	flags&= ~HA_KEYREAD_ONLY;
      break;
    default:  // Keep compiler happy
      break;
    }
  }
  return flags;
}

static int
tokudb_cmp_hidden_key(DB* file, const DBT *new_key, const DBT *saved_key)
{
  ulonglong a=uint5korr((char*) new_key->data);
  ulonglong b=uint5korr((char*) saved_key->data);
  return  a < b ? -1 : (a > b ? 1 : 0);
}

static int
tokudb_cmp_packed_key(DB *file, const DBT *new_key, const DBT *saved_key)
{
  assert(file->app_private != 0);
  KEY *key= (KEY*) file->app_private;
  uchar *new_key_ptr= (uchar*) new_key->data;
  uchar *saved_key_ptr= (uchar*) saved_key->data;
  KEY_PART_INFO *key_part= key->key_part, *end=key_part+key->key_parts;
  uint key_length= new_key->size;

  //DBUG_DUMP("key_in_index", saved_key_ptr, saved_key->size);
  for (; key_part != end && (int) key_length > 0; key_part++)
  {
    int cmp;
    uint length;
    if (key_part->null_bit)
    {
      if (*new_key_ptr != *saved_key_ptr++)
	return ((int) *new_key_ptr - (int) saved_key_ptr[-1]);
      key_length--;
      if (!*new_key_ptr++)
	continue;
    }
    if (0) printf("%s:%d:insert_or_update=%d\n", __FILE__, __LINE__, key->table->insert_or_update);
    if ((cmp= key_part->field->pack_cmp(new_key_ptr,saved_key_ptr,
					key_part->length,
					key->table->insert_or_update)))
      return cmp;
    length= key_part->field->packed_col_length(new_key_ptr,
					       key_part->length);
    new_key_ptr+=length;
    key_length-=length;
    saved_key_ptr+=key_part->field->packed_col_length(saved_key_ptr,
						      key_part->length);
  }
  return key->handler.bdb_return_if_eq;
}

/* Compare key against row */
static bool
tokudb_key_cmp(TABLE *table, KEY *key_info, const uchar *key, uint key_length)
{
  KEY_PART_INFO *key_part= key_info->key_part,
    *end=key_part+key_info->key_parts;

  for (; key_part != end && (int) key_length > 0; key_part++)
  {
    int cmp;
    uint length;
    if (key_part->null_bit)
    {
      key_length--;
      /*
	With the current usage, the following case will always be FALSE,
	because NULL keys are sorted before any other key
      */
      if (*key != (table->record[0][key_part->null_offset] &
		   key_part->null_bit) ? 0 : 1)
	return 1;
      if (!*key++)  // Null value
	continue;
    }
    /*
      Last argument has to be 0 as we are also using this to function to see
      if a key like 'a  ' matched a row with 'a'
    */
    if ((cmp= key_part->field->pack_cmp(key, key_part->length, 0)))
      return cmp;
    length= key_part->field->packed_col_length(key,key_part->length);
    key+= length;
    key_length-= length;
  }
  return 0;  // Identical keys
}

int ha_tokudb::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_tokudb::open");
  TOKUDB_OPEN();

  char name_buff[FN_REFLEN];
  uint open_mode=(mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
  uint max_key_length;
  int error;

  /* Open primary key */
  hidden_primary_key= 0;
  if ((primary_key= table_share->primary_key) >= MAX_KEY)
  {
    // No primary key
    primary_key= table_share->keys;
    key_used_on_scan= MAX_KEY;
    ref_length= hidden_primary_key= TDB_HIDDEN_PRIMARY_KEY_LENGTH;
  }
  else
    key_used_on_scan= primary_key;

  /* Need some extra memory in case of packed keys */
  max_key_length= table_share->max_key_length + MAX_REF_PARTS*3;
  if (!(alloc_ptr=
	my_multi_malloc(MYF(MY_WME),
			&key_buff,  max_key_length,
			&key_buff2, max_key_length,
			&primary_key_buff,
			(hidden_primary_key ? 0 :
			 table_share->key_info[table_share->primary_key].key_length),
			NullS)))
    DBUG_RETURN(1);
  if (!(rec_buff= (uchar*) my_malloc((alloced_rec_buff_length=
				      table_share->rec_buff_length),
				     MYF(MY_WME))))
  {
    my_free(alloc_ptr,MYF(0));
    DBUG_RETURN(1);
  }

  /* Init shared structure */
  if (!(share= get_share(name,table)))
  {
    my_free((char*) rec_buff,MYF(0));
    my_free(alloc_ptr,MYF(0));
    DBUG_RETURN(1);
  }
  thr_lock_data_init(&share->lock, &lock, NULL);
  key_file = share->key_file;
  key_type = share->key_type;
  bzero((void*) &current_row, sizeof(current_row));

  /* Fill in shared structure, if needed */
  pthread_mutex_lock(&share->mutex);
  file= share->file;
  printf("%s:%d:bdbopen:%p:share=%p:file=%p:table=%p:table->s=%p:%d\n", __FILE__, __LINE__, 
         this, share, share->file, table, table->s, share->use_count);
  if (!share->use_count++)
  {
    DBUG_PRINT("info",("share->use_count %u", share->use_count));

    if ((error=db_create(&file, db_env, 0)))
    {
      free_share(share,table, hidden_primary_key,1);
      my_free((char*) rec_buff,MYF(0));
      my_free(alloc_ptr,MYF(0));
      my_errno=error;
      DBUG_RETURN(1);
    }
    share->file= file;

    file->set_bt_compare(file,
			 (hidden_primary_key ? tokudb_cmp_hidden_key :
			  tokudb_cmp_packed_key));
    if (!hidden_primary_key)
      file->app_private= (void*) (table->key_info + table_share->primary_key);
    if ((error= db_env->txn_begin(db_env, NULL, (DB_TXN**) &transaction, 0)) ||
	(error= (file->open(file, transaction,
			    fn_format(name_buff, name, "", ha_tokudb_ext,
				      MY_UNPACK_FILENAME|MY_APPEND_EXT),
			    "main", DB_BTREE, open_mode, 0))) ||
	(error= transaction->commit(transaction, 0)))
    {
      free_share(share, table, hidden_primary_key,1);
      my_free((char*) rec_buff,MYF(0));
      my_free(alloc_ptr,MYF(0));
      my_errno=error;
      DBUG_RETURN(1);
    }

    /* Open other keys;  These are part of the share structure */
    key_file[primary_key]= file;
    key_type[primary_key]= hidden_primary_key ? 0 : DB_NOOVERWRITE;

    DB **ptr=key_file;
    for (uint i=0, used_keys=0; i < table_share->keys; i++, ptr++)
    {
      char part[7];
      if (i != primary_key)
      {
	if ((error=db_create(ptr, db_env, 0)))
	{
	  __close(1);
	  my_errno=error;
	  DBUG_RETURN(1);
	}
	sprintf(part,"key%02d",++used_keys);
	key_type[i]=table->key_info[i].flags & HA_NOSAME ? DB_NOOVERWRITE : 0;
	(*ptr)->set_bt_compare(*ptr, tokudb_cmp_packed_key);
	(*ptr)->app_private= (void*) (table->key_info+i);
	if (!(table->key_info[i].flags & HA_NOSAME))
	{
	  DBUG_PRINT("info",("Setting DB_DUP+DB_DUPSORT for key %u", i));
	  (*ptr)->set_flags(*ptr, DB_DUP+DB_DUPSORT);
	}
	if ((error= db_env->txn_begin(db_env, NULL, (DB_TXN**) &transaction,
				      0)) ||
	    (error=((*ptr)->open(*ptr, transaction, name_buff, part, DB_BTREE,
				 open_mode, 0))) ||
	    (error= transaction->commit(transaction, 0)))
	{
	  __close(1);
	  my_errno=error;
	  DBUG_RETURN(1);
	}
      }
    }
    /* Calculate pack_length of primary key */
    share->fixed_length_primary_key= 1;
    if (!hidden_primary_key)
    {
      ref_length=0;
      KEY_PART_INFO *key_part= table->key_info[primary_key].key_part;
      KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;
      for (; key_part != end; key_part++)
	ref_length+= key_part->field->max_packed_col_length(key_part->length);
      share->fixed_length_primary_key=
	(ref_length == table->key_info[primary_key].key_length);
      share->status|= STATUS_PRIMARY_KEY_INIT;
    }
    share->ref_length= ref_length;
  }
  ref_length= share->ref_length;  // If second open
  pthread_mutex_unlock(&share->mutex);

  transaction=0;
  cursor=0;
  key_read=0;
  stats.block_size=8192;  // Tokudb DB block size
  share->fixed_length_row= !(table_share->db_create_options &
			     HA_OPTION_PACK_RECORD);

  get_status();
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

  DBUG_RETURN(0);
}

int ha_tokudb::close(void)
{
  DBUG_ENTER("ha_tokudb::close");
  TOKUDB_CLOSE();
  DBUG_RETURN(__close(0));
}

int ha_tokudb::__close(int mutex_is_locked)
{
  DBUG_ENTER("ha_tokudb::__close");
  printf("%s:%d:close:%p\n", __FILE__, __LINE__, this);
  if (file->app_private == table->key_info + table_share->primary_key) {
      printf("%s:%d:reset app_private\n", __FILE__, __LINE__);
      file->app_private = 0;
  }
  my_free(rec_buff,MYF(MY_ALLOW_ZERO_PTR));
  my_free(alloc_ptr,MYF(MY_ALLOW_ZERO_PTR));
  ha_tokudb::reset();  // current_row buffer
  DBUG_RETURN(free_share(share, table, hidden_primary_key, mutex_is_locked));
}

/* Reallocate buffer if needed */
bool ha_tokudb::fix_rec_buff_for_blob(ulong length)
{
  if (! rec_buff || length > alloced_rec_buff_length)
  {
    uchar *newptr;
    if (!(newptr=(uchar*) my_realloc((void *) rec_buff, length,
				     MYF(MY_ALLOW_ZERO_PTR))))
      return 1;
    rec_buff=newptr;
    alloced_rec_buff_length=length;
  }
  return 0;
}

/* Calculate max length needed for row */
ulong ha_tokudb::max_row_length(const uchar *buf)
{
  ulong length= table_share->reclength + table_share->fields*2;
  uint *ptr, *end;
  for (ptr= table_share->blob_field, end=ptr + table_share->blob_fields ;
       ptr != end ;
       ptr++)
  {
    Field_blob *blob= ((Field_blob*) table->field[*ptr]);
    length += blob->get_length((uchar *)(buf + blob->offset((uchar *)buf)))+2;
  }
  return length;
}

/*
  Pack a row for storage.
  If the row is of fixed length, just store the  row 'as is'.
  If not, we will generate a packed row suitable for storage.
  This will only fail if we don't have enough memory to pack the row,
  which may only happen in rows with blobs, as the default row length is
  pre-allocated.
*/

int ha_tokudb::pack_row(DBT *row, const uchar *record, bool new_row)
{
  uchar *ptr;
  bzero((void*) row, sizeof(*row));
  if (share->fixed_length_row)
  {
    row->data=(void*) record;
    row->size= table_share->reclength+hidden_primary_key;
    if (hidden_primary_key)
    {
      if (new_row)
	get_auto_primary_key(current_ident);
      memcpy_fixed((char*) record+table_share->reclength,
		   (char*) current_ident,
		   TDB_HIDDEN_PRIMARY_KEY_LENGTH);
    }
    return 0;
  }
  if (table_share->blob_fields)
  {
    if (fix_rec_buff_for_blob(max_row_length(record)))
      return HA_ERR_OUT_OF_MEM;
  }

  /* Copy null bits */
  memcpy(rec_buff, record, table_share->null_bytes);
  ptr= rec_buff + table_share->null_bytes;

  for (Field **field=table->field; *field; field++)
    ptr= (*field)->pack(ptr,
			(const uchar *)
			(record
			 + (*field)->offset((uchar *)record)));

  if (hidden_primary_key)
  {
    if (new_row)
      get_auto_primary_key(current_ident);
    memcpy_fixed((char*) ptr, (char*) current_ident,
		 TDB_HIDDEN_PRIMARY_KEY_LENGTH);
    ptr+=TDB_HIDDEN_PRIMARY_KEY_LENGTH;
  }
  row->data=rec_buff;
  row->size= (size_t) (ptr - rec_buff);
  return 0;
}

void ha_tokudb::unpack_row(uchar *record, DBT *row)
{
  if (share->fixed_length_row)
    memcpy(record, (void*) row->data,
	   table_share->reclength+hidden_primary_key);
  else
  {
    /* Copy null bits */
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
    const uchar *ptr= (const uchar *)row->data;
    memcpy(record, ptr, table_share->null_bytes);
    ptr+= table_share->null_bytes;
    for (Field **field=table->field; *field; field++)
      ptr= (*field)->unpack(record + (*field)->offset(record), ptr);
    dbug_tmp_restore_column_map(table->write_set, old_map);
  }
}

/* Store the key and the primary key into the row */
void ha_tokudb::unpack_key(uchar *record, DBT *key, uint index)
{
  KEY *key_info= table->key_info+index;
  KEY_PART_INFO *key_part= key_info->key_part,
    *end= key_part+key_info->key_parts;
  uchar *pos= (uchar*) key->data;

  for (; key_part != end; key_part++)
  {
    if (key_part->null_bit)
    {
      if (!*pos++)  // Null value
      {
	/*
	  We don't need to reset the record data as we will not access it
	  if the null data is set
	*/

	record[key_part->null_offset] |= key_part->null_bit;
	continue;
      }
      record[key_part->null_offset] &= ~key_part->null_bit;
    }
    pos= (uchar*) key_part->field->unpack_key(record + key_part->field->offset(record),
					      pos,
#if MYSQL_VERSION_ID <= 60003
                                              key_part->length);
#else
					      key_part->length,
					      table->s->db_low_byte_first);
#endif
  }
}

/*
  Create a packed key from a row. This key will be written as such
  to the index tree.

  This will never fail as the key buffer is pre-allocated.
*/

DBT *ha_tokudb::create_key(DBT *key, uint keynr, uchar *buff,
                             const uchar *record, int key_length)
{
  DBUG_ENTER("ha_tokudb::create_key");
  bzero((void*) key, sizeof(*key));
  if (hidden_primary_key && keynr == primary_key)
  {
    key->data= current_ident;
    key->size= TDB_HIDDEN_PRIMARY_KEY_LENGTH;
    DBUG_RETURN(key);
  }

  KEY *key_info= table->key_info+keynr;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part+key_info->key_parts;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);

  key->data= buff;
  // DBTs dont have app_private members anymore, that was a MySQL hack
  // instead it seems to be called app_data
  // key->app_private= (void*) key_info;
  // key->app_data= (void*) key_info;
  for (; key_part != end && key_length > 0; key_part++)
  {
    if (key_part->null_bit)
    {
      /* Store 0 if the key part is a NULL part */
      if (record[key_part->null_offset] & key_part->null_bit)
      {
	*buff++ =0;
	key->flags|=DB_DBT_DUPOK;
	continue;
      }
      *buff++ = 1;  // Store NOT NULL marker
    }
    buff=key_part->field->pack_key(buff,
				   (uchar*) (record + key_part->offset),
#if MYSQL_VERSION_ID <= 60003
                                   key_part->length);
#else
				   key_part->length,
				   table->s->db_low_byte_first);
#endif
    key_length -= key_part->length;
  }
  key->size= (buff - (uchar*) key->data);
  DBUG_DUMP("key", (uchar*) key->data, key->size);
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(key);
}


/*
  Create a packed key from from a MySQL unpacked key (like the one that is
  sent from the index_read()

  This key is to be used to read a row
*/

DBT *ha_tokudb::pack_key(DBT *key, uint keynr, uchar *buff,
                           const uchar *key_ptr, uint key_length)
{
  DBUG_ENTER("ha_tokudb::pack_key");
  KEY *key_info= table->key_info+keynr;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part+key_info->key_parts;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);

  bzero((void*) key, sizeof(*key));
  key->data= buff;
  // DBTs dont have app_private members anymore, that was a MySQL hack
  // instead it seems to be called app_data
  // key->app_private= (void*) key_info;
  // key->app_data= (void*) key_info;

  for (; key_part != end && (int)key_length > 0; key_part++)
  {
    uint offset=0;
    if (key_part->null_bit)
    {
      if (!(*buff++ = (*key_ptr == 0)))  // Store 0 if NULL
      {
	key_length -= key_part->store_length;
	key_ptr += key_part->store_length;
	key->flags |= DB_DBT_DUPOK;
	continue;
      }
      offset= 1;  // Data is at key_ptr+1
    }
    buff= key_part->field->pack_key_from_key_image(buff,
						   (uchar*) key_ptr+offset,
#if MYSQL_VERSION_ID <= 60003
                                                   key_part->length);
#else
						   key_part->length,
						   table->s->db_low_byte_first);
#endif
    key_ptr += key_part->store_length;
    key_length -= key_part->store_length;
  }
  key->size= (buff - (uchar*)key->data);
  DBUG_DUMP("key", (uchar*)key->data, key->size);
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(key);
}

/** @brief
    Get status information that is stored in the 'status' sub database
    and the max used value for the hidden primary key.
*/
void ha_tokudb::get_status()
{
  if (!test_all_bits(share->status, (STATUS_PRIMARY_KEY_INIT |
				     STATUS_ROW_COUNT_INIT)))
  {
    pthread_mutex_lock(&share->mutex);
    if (!(share->status & STATUS_PRIMARY_KEY_INIT))
    {
      (void) extra(HA_EXTRA_KEYREAD);
      index_init(primary_key, 0);
      if (!index_last(table->record[1]))
	share->auto_ident= uint5korr(current_ident);
      index_end();
      (void) extra(HA_EXTRA_NO_KEYREAD);
    }
    if (! share->status_block)
    {
      char name_buff[FN_REFLEN];
      uint open_mode= (((table->db_stat & HA_READ_ONLY) ? DB_RDONLY : 0)
		       | DB_THREAD);
      fn_format(name_buff, share->table_name, "", ha_tokudb_ext,
		MY_UNPACK_FILENAME|MY_APPEND_EXT);
      if (!db_create(&share->status_block, db_env, 0))
      {
	if (share->status_block->open(share->status_block, NULL, name_buff,
				      "status", DB_BTREE, open_mode, 0))
	{
	  share->status_block->close(share->status_block, 0);
	  share->status_block= 0;
	}
      }
    }
    if (!(share->status & STATUS_ROW_COUNT_INIT) && share->status_block)
    {
      share->org_rows= share->rows=
	table_share->max_rows ? table_share->max_rows : HA_TOKUDB_MAX_ROWS;
      if (!share->status_block->cursor(share->status_block, 0, &cursor, 0))
      {
	DBT row;
	char rec_buff[64];
	bzero((void*) &row, sizeof(row));
	bzero((void*) &last_key, sizeof(last_key));
	row.data= rec_buff;
	row.ulen= sizeof(rec_buff);
	row.flags= DB_DBT_USERMEM;
	if (!cursor->c_get(cursor, &last_key, &row, DB_FIRST))
	{
	  uint i;
	  uchar *pos= (uchar*)row.data;
	  share->org_rows= share->rows= uint4korr(pos);
	  pos += 4;
	  for (i=0; i < table_share->keys; i++)
	  {
	    share->rec_per_key[i]= uint4korr(pos);
	    pos += 4;
	  }
	}
	cursor->c_close(cursor);
      }
      cursor= 0;
    }
    share->status |= STATUS_PRIMARY_KEY_INIT | STATUS_ROW_COUNT_INIT;
    pthread_mutex_unlock(&share->mutex);
  }
}

static int write_status(DB *status_block, char *buff, uint length)
{
  DBUG_ENTER("write_status");
  DBT row, key;
  int error;
  const char *key_buff= "status";

  bzero((void*) &row, sizeof(row));
  bzero((void*) &key, sizeof(key));
  row.data= buff;
  key.data= (void*) key_buff;
  key.size= sizeof(key_buff);
  row.size= length;
  error= status_block->put(status_block, 0, &key, &row, 0);
  DBUG_RETURN(error);
}

static void update_status(TOKUDB_SHARE *share, TABLE *table)
{
  DBUG_ENTER("update_status");
  if (share->rows != share->org_rows ||
      (share->status & STATUS_TDB_ANALYZE))
  {
    pthread_mutex_lock(&share->mutex);
    if (!share->status_block)
    {
      /*
	Create sub database 'status' if it doesn't exist from before
	(This '*should*' always exist for table created with MySQL)
      */

      char name_buff[FN_REFLEN];
      if (db_create(&share->status_block, db_env, 0))
	goto end;
      share->status_block->set_flags(share->status_block, 0);
      if (share->status_block->open(share->status_block, NULL,
				    fn_format(name_buff,share->table_name,
					      "", ha_tokudb_ext,
					      MY_UNPACK_FILENAME|MY_APPEND_EXT),
				    "status", DB_BTREE,
				    DB_THREAD | DB_CREATE, my_umask))
	goto end;
    }
    {
      char rec_buff[4+MAX_KEY*4], *pos= rec_buff;
      int4store(pos,share->rows); pos += 4;
      for (uint i=0; i < table->s->keys; i++)
      {
	int4store(pos,share->rec_per_key[i]); pos+=4;
      }
      DBUG_PRINT("info",("updating status for %s", share->table_name));
      (void) write_status(share->status_block, rec_buff,
			  (uint) (pos-rec_buff));
      share->status&= ~STATUS_TDB_ANALYZE;
      share->org_rows=share->rows;
    }
  end:
    pthread_mutex_unlock(&share->mutex);
  }
  DBUG_VOID_RETURN;
}

/** @brief
    Return an estimated of the number of rows in the table.
    Used when sorting to allocate buffers and by the optimizer.
*/
ha_rows ha_tokudb::estimate_rows_upper_bound()
{
  return share->rows + HA_TOKUDB_EXTRA_ROWS;
}

int ha_tokudb::cmp_ref(const uchar *ref1, const uchar *ref2)
{
  if (hidden_primary_key)
    return memcmp(ref1, ref2, TDB_HIDDEN_PRIMARY_KEY_LENGTH);

  int result;
  Field *field;
  KEY *key_info= table->key_info+table_share->primary_key;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part+key_info->key_parts;

  for (; key_part != end; key_part++)
  {
    field= key_part->field;
    result= field->pack_cmp((const uchar*)ref1, (const uchar*)ref2,
			    key_part->length, 0);
    if (result)
      return result;
    ref1+= field->packed_col_length((const uchar*)ref1, key_part->length);
    ref2+= field->packed_col_length((const uchar*)ref2, key_part->length);
  }

  return 0;
}

bool ha_tokudb::check_if_incompatible_data(HA_CREATE_INFO *info,
                                             uint table_changes)
{
  if (table_changes < IS_EQUAL_YES)
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

int ha_tokudb::write_row(uchar *record)
{
  DBUG_ENTER("write_row");
  DBT row, prim_key, key;
  int error;

  statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();
  if ((error=pack_row(&row, (const uchar*) record, 1)))
    DBUG_RETURN(error);

  table->insert_or_update= 1;  // For handling of VARCHAR
  if (table_share->keys + test(hidden_primary_key) == 1)
  {
    error=file->put(file, transaction, create_key(&prim_key, primary_key,
						  key_buff, record),
		    &row, key_type[primary_key]);
    last_dup_key=primary_key;
  }
  else
  {
    DB_TXN *sub_trans = transaction;
    /* Don't use sub transactions in temporary tables */
    for (uint retry=0; retry < tokudb_trans_retry; retry++)
    {
      key_map changed_keys(0);
      if (!(error=file->put(file, sub_trans, create_key(&prim_key, primary_key,
							key_buff, record),
			    &row, key_type[primary_key])))
      {
	changed_keys.set_bit(primary_key);
	for (uint keynr=0; keynr < table_share->keys; keynr++)
	{
	  if (keynr == primary_key)
	    continue;
	  if ((error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  create_key(&key, keynr, key_buff2,
						     record),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	  changed_keys.set_bit(keynr);
	}
      }
      else
	last_dup_key=primary_key;
      if (error)
      {
	/* Remove inserted row */
	DBUG_PRINT("error",("Got error %d",error));
	if (using_ignore)
	{
	  int new_error = 0;
	  if (!changed_keys.is_clear_all())
	  {
	    new_error = 0;
	    for (uint keynr=0;
		 keynr < table_share->keys+test(hidden_primary_key);
		 keynr++)
	    {
	      if (changed_keys.is_set(keynr))
	      {
		if ((new_error = remove_key(sub_trans, keynr, record,
					    &prim_key)))
		  break;
	      }
	    }
	  }
	  if (new_error)
	  {
	    error=new_error;  // This shouldn't happen
	    break;
	  }
	}
      }
      if (error != DB_LOCK_DEADLOCK)
	break;
    }
  }
  table->insert_or_update= 0;
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  else if (!error)
    changed_rows++;
  DBUG_RETURN(error);
}

/* Compare if a key in a row has changed */
int ha_tokudb::key_cmp(uint keynr,
                         const uchar *old_row,
                         const uchar *new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for (; key_part != end; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
	  (new_row[key_part->null_offset] & key_part->null_bit))
	return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {

      if (key_part->field->cmp_binary((uchar*) (old_row + key_part->offset),
				      (uchar*) (new_row + key_part->offset),
				      (ulong) key_part->length))
	return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
		 key_part->length))
	return 1;
    }
  }
  return 0;
}


/*
  Update a row from one value to another.
  Clobbers key_buff2
*/
int ha_tokudb::update_primary_key(DB_TXN *trans, bool primary_key_changed,
                                    const uchar *old_row, DBT *old_key,
                                    const uchar *new_row, DBT *new_key,
                                    bool local_using_ignore)
{
  DBUG_ENTER("update_primary_key");
  DBT row;
  int error;

  if (primary_key_changed)
  {
    // Primary key changed or we are updating a key that can have duplicates.
    // Delete the old row and add a new one
    if (!(error=remove_key(trans, primary_key, old_row, old_key)))
    {
      if (!(error=pack_row(&row, new_row, 0)))
      {
	if ((error=file->put(file, trans, new_key, &row,
			     key_type[primary_key])))
	{
	  // Probably a duplicated key; restore old key and row if needed
	  last_dup_key= primary_key;
	  if (local_using_ignore)
	  {
	    int new_error;
	    if ((new_error=pack_row(&row, old_row, 0)) ||
		(new_error=file->put(file, trans, old_key, &row,
				     key_type[primary_key])))
	      error=new_error;  // fatal error
	  }
	}
      }
    }
  }
  else
  {
    // Primary key didn't change;  just update the row data
    if (!(error=pack_row(&row, new_row, 0)))
      error=file->put(file, trans, new_key, &row, 0);
  }
  DBUG_RETURN(error);
}

/*
  Restore changed keys, when a non-fatal error aborts the insert/update
  of one row.
  Clobbers keybuff2
*/
int ha_tokudb::restore_keys(DB_TXN *trans, key_map *changed_keys,
                              uint primary_key,
                              const uchar *old_row, DBT *old_key,
                              const uchar *new_row, DBT *new_key)
{
  DBUG_ENTER("restore_keys");
  int error;
  DBT tmp_key;
  uint keynr;

  /* Restore the old primary key, and the old row, but don't ignore
     duplicate key failure */
  if ((error=update_primary_key(trans, TRUE, new_row, new_key,
				old_row, old_key, FALSE)))
    goto err;

  /* Remove the new key, and put back the old key
     changed_keys is a map of all non-primary keys that need to be
     rolled back.  The last key set in changed_keys is the one that
     triggered the duplicate key error (it wasn't inserted), so for
     that one just put back the old value. */
  if (!changed_keys->is_clear_all())
  {
    for (keynr=0; keynr < table_share->keys+test(hidden_primary_key); keynr++)
    {
      if (changed_keys->is_set(keynr))
      {
	if (changed_keys->is_prefix(1) &&
	    (error = remove_key(trans, keynr, new_row, new_key)))
	  break;
	if ((error = key_file[keynr]->put(key_file[keynr], trans,
					  create_key(&tmp_key, keynr, key_buff2,
						     old_row),
					  old_key, key_type[keynr])))
	  break;
      }
    }
  }

err:
  DBUG_ASSERT(error != DB_KEYEXIST);
  DBUG_RETURN(error);
}

int ha_tokudb::update_row(const uchar *old_row, uchar *new_row)
{
  DBUG_ENTER("update_row");
  DBT prim_key, key, old_prim_key;
  int error;
  DB_TXN *sub_trans;
  bool primary_key_changed;

  LINT_INIT(error);
  statistic_increment(table->in_use->status_var.ha_update_count,&LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();

  table->insert_or_update= 1;  // For handling of VARCHAR
  if (hidden_primary_key)
  {
    primary_key_changed=0;
    bzero((void*) &prim_key, sizeof(prim_key));
    prim_key.data= (void*) current_ident;
    prim_key.size=TDB_HIDDEN_PRIMARY_KEY_LENGTH;
    old_prim_key=prim_key;
  }
  else
  {
    create_key(&prim_key, primary_key, key_buff, new_row);

    if ((primary_key_changed=key_cmp(primary_key, old_row, new_row)))
      create_key(&old_prim_key, primary_key, primary_key_buff, old_row);
    else
      old_prim_key=prim_key;
  }

  sub_trans = transaction;
  for (uint retry=0; retry < tokudb_trans_retry; retry++)
  {
    key_map changed_keys(0);
    /* Start by updating the primary key */
    if (!(error=update_primary_key(sub_trans, primary_key_changed,
				   old_row, &old_prim_key,
				   new_row, &prim_key,
				   using_ignore)))
    {
      // Update all other keys
      for (uint keynr=0; keynr < table_share->keys; keynr++)
      {
	if (keynr == primary_key)
	  continue;
	if (key_cmp(keynr, old_row, new_row) || primary_key_changed)
	{
	  if ((error=remove_key(sub_trans, keynr, old_row, &old_prim_key)))
	  {
	    table->insert_or_update= 0;
	    DBUG_RETURN(error);  // Fatal error
	  }
	  changed_keys.set_bit(keynr);
	  if ((error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  create_key(&key, keynr, key_buff2,
						     new_row),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	}
      }
    }
    if (error)
    {
      /* Remove inserted row */
      DBUG_PRINT("error",("Got error %d",error));
      if (using_ignore)
      {
	int new_error = 0;
	if (!changed_keys.is_clear_all())
	  new_error=restore_keys(transaction, &changed_keys, primary_key,
				 old_row, &old_prim_key, new_row, &prim_key);
	if (new_error)
	{
	  /* This shouldn't happen */
	  error=new_error;
	  break;
	}
      }
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
  table->insert_or_update= 0;
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  DBUG_RETURN(error);
}

/*
  Delete one key
  This uses key_buff2, when keynr != primary key, so it's important that
  a function that calls this doesn't use this buffer for anything else.
*/

int ha_tokudb::remove_key(DB_TXN *trans, uint keynr, const uchar *record,
                            DBT *prim_key)
{
  DBUG_ENTER("remove_key");
  int error;
  DBT key;
  DBUG_PRINT("enter",("index: %d",keynr));

  if (keynr == active_index && cursor)
    error=cursor->c_del(cursor,0);
  else if (keynr == primary_key ||
	   ((table->key_info[keynr].flags & (HA_NOSAME | HA_NULL_PART_KEY)) ==
	    HA_NOSAME))
  {  // Unique key
    DBUG_ASSERT(keynr == primary_key || prim_key->data != key_buff2);
    error=key_file[keynr]->del(key_file[keynr], trans,
			       keynr == primary_key ?
			       prim_key :
			       create_key(&key, keynr, key_buff2, record),
			       0);
  }
  else
  {
    /*
      To delete the not duplicated key, we need to open an cursor on the
      row to find the key to be delete and delete it.
      We will never come here with keynr = primary_key
    */
    DBUG_ASSERT(keynr != primary_key && prim_key->data != key_buff2);
    DBC *tmp_cursor;
    if (!(error=key_file[keynr]->cursor(key_file[keynr], trans,
					&tmp_cursor, 0)))
    {
      if (!(error=tmp_cursor->c_get(tmp_cursor,
				    create_key(&key, keynr, key_buff2, record),
				    prim_key, DB_GET_BOTH | DB_RMW)))
      {  // This shouldn't happen
	error=tmp_cursor->c_del(tmp_cursor,0);
      }
      int result=tmp_cursor->c_close(tmp_cursor);
      if (!error)
	error=result;
    }
  }
  DBUG_RETURN(error);
}

/* Delete all keys for new_record */
int ha_tokudb::remove_keys(DB_TXN *trans,
                             const uchar *record,
                             DBT *new_record,
                             DBT *prim_key,
                             key_map *keys)
{
  int result= 0;
  for (uint keynr=0;
       keynr < table_share->keys+test(hidden_primary_key);
       keynr++)
  {
    if (keys->is_set(keynr))
    {
      int new_error=remove_key(trans, keynr, record, prim_key);
      if (new_error)
      {
	result=new_error;  // Return last error
	break;  // Let rollback correct things
      }
    }
  }
  return result;
}

int ha_tokudb::delete_row(const uchar *record)
{
  DBUG_ENTER("delete_row");
  int error;
  DBT row, prim_key;
  key_map keys= table_share->keys_in_use;
  statistic_increment(table->in_use->status_var.ha_delete_count,&LOCK_status);

  if ((error=pack_row(&row, record, 0)))
    DBUG_RETURN((error));
  create_key(&prim_key, primary_key, key_buff, record);
  if (hidden_primary_key)
    keys.set_bit(primary_key);

  /* Subtransactions may be used in order to retry the delete in
     case we get a DB_LOCK_DEADLOCK error. */
  DB_TXN *sub_trans = transaction;
  for (uint retry=0; retry < tokudb_trans_retry; retry++)
  {
    error=remove_keys(sub_trans, record, &row, &prim_key, &keys);
    if (error)
    {
      DBUG_PRINT("error",("Got error %d",error));
      break;  // No retry - return error
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
#ifdef CANT_COUNT_DELETED_ROWS
  if (!error)
    changed_rows--;
#endif
  DBUG_RETURN(error);
}

int ha_tokudb::index_init(uint keynr, bool sorted)
{
  DBUG_ENTER("ha_tokudb::index_init");
  int error;
  DBUG_PRINT("enter",("table: '%s'  key: %d",
		      table_share->table_name.str, keynr));

  /*
    Under some very rare conditions (like full joins) we may already have
    an active cursor at this point
  */
  if (cursor)
  {
    DBUG_PRINT("note",("Closing active cursor"));
    cursor->c_close(cursor);
  }
  active_index=keynr;
  DBUG_ASSERT(keynr <= table->s->keys);
  DBUG_ASSERT(key_file[keynr]);
  if ((error=key_file[keynr]->cursor(key_file[keynr], transaction, &cursor,
				     table->reginfo.lock_type >
				     TL_WRITE_ALLOW_READ ?
				     0 : 0)))
    cursor=0;  // Safety
  bzero((void*) &last_key, sizeof(last_key));
  DBUG_RETURN(error);
}

int ha_tokudb::index_end()
{
  DBUG_ENTER("ha_tokudb::index_end");
  int error=0;
  if (cursor)
  {
    DBUG_PRINT("enter",("table: '%s'", table_share->table_name.str));
    error=cursor->c_close(cursor);
    cursor=0;
  }
  active_index=MAX_KEY;
  DBUG_RETURN(error);
}

/* What to do after we have read a row based on an index */
int ha_tokudb::read_row(int error, uchar *buf, uint keynr, DBT *row,
                          DBT *found_key, bool read_next)
{
  DBUG_ENTER("ha_tokudb::read_row");
  if (error)
  {
    if (error == DB_NOTFOUND || error == DB_KEYEMPTY)
      error=read_next ? HA_ERR_END_OF_FILE : HA_ERR_KEY_NOT_FOUND;
    table->status=STATUS_NOT_FOUND;
    DBUG_RETURN(error);
  }
  if (hidden_primary_key)
    memcpy_fixed(current_ident,
		 (char*) row->data+row->size-TDB_HIDDEN_PRIMARY_KEY_LENGTH,
		 TDB_HIDDEN_PRIMARY_KEY_LENGTH);
  table->status=0;
  if (keynr != primary_key)
  {
    /* We only found the primary key.  Now we have to use this to find
       the row data */
    if (key_read && found_key)
    {
      unpack_key(buf,found_key,keynr);
      if (!hidden_primary_key)
	unpack_key(buf,row,primary_key);
      DBUG_RETURN(0);
    }
    DBT key;
    bzero((void*) &key, sizeof(key));
    key.data=key_buff;
    key.size=row->size;
    // DBTs dont have app_private members anymore, that was a MySQL hack
    // now it seems to be called app_data
    // key.app_private= (void*) (table->key_info+primary_key);
    // key.app_data= (void*) (table->key_info+primary_key);
    memcpy(key_buff,row->data,row->size);
    /* Read the data into current_row */
    current_row.flags=DB_DBT_REALLOC;
    if ((error=file->get(file, transaction, &key, &current_row, 0)))
    {
      table->status=STATUS_NOT_FOUND;
      DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
    }
    row= &current_row;
  }
  unpack_row(buf,row);
  DBUG_RETURN(0);
}

/*
  This is only used to read whole keys
*/
int ha_tokudb::index_read_idx(uchar *buf, uint keynr, const uchar *key,
                                uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_tokudb::index_read_idx");
  table->in_use->status_var.ha_read_key_count++;
  current_row.flags=DB_DBT_REALLOC;
  active_index=MAX_KEY;
  DBUG_RETURN(read_row(key_file[keynr]->get(key_file[keynr],
					    transaction,
					    pack_key(&last_key,
						     keynr,
						     key_buff,
						     key,
						     key_len),
					    &current_row,0),
		       buf, keynr, &current_row, &last_key, 0));
}


int ha_tokudb::index_read(uchar *buf, const uchar *key,
                            uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_tokudb::index_read");
  DBT row;
  int error;
  KEY *key_info= &table->key_info[active_index];
  int do_prev= 0;

  table->in_use->status_var.ha_read_key_count++;
  bzero((void*) &row, sizeof(row));
  if (find_flag == HA_READ_BEFORE_KEY)
  {
    find_flag= HA_READ_KEY_OR_NEXT;
    do_prev= 1;
  }
  else if (find_flag == HA_READ_PREFIX_LAST_OR_PREV)
  {
    find_flag= HA_READ_AFTER_KEY;
    do_prev= 1;
  }
  if (key_len == key_info->key_length &&
      !(table->key_info[active_index].flags & HA_END_SPACE_KEY))
  {
    if (find_flag == HA_READ_AFTER_KEY)
      key_info->handler.bdb_return_if_eq= 1;
    error= read_row(cursor->c_get(cursor, pack_key(&last_key,
						   active_index,
						   key_buff,
						   key, key_len),
				  &row,
				  (find_flag == HA_READ_KEY_EXACT ?
				   DB_SET : DB_SET_RANGE)),
		    buf, active_index, &row, (DBT*) 0, 0);
    key_info->handler.bdb_return_if_eq= 0;
  }
  else
  {
    /* read of partial key */
    pack_key(&last_key, active_index, key_buff, key, key_len);
    /* Store for compare */
    memcpy(key_buff2, key_buff, (key_len=last_key.size));
    /*
      If HA_READ_AFTER_KEY is set, return next key, else return first
      matching key.
    */
    key_info->handler.bdb_return_if_eq= (find_flag == HA_READ_AFTER_KEY ?
					 1 : -1);
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE),
		   buf, active_index, &row, (DBT*) 0, 0);
    key_info->handler.bdb_return_if_eq= 0;
    if (!error && find_flag == HA_READ_KEY_EXACT)
    {
      /* Ensure that we found a key that is equal to the current one */
      if (!error && tokudb_key_cmp(table, key_info, key_buff2, key_len))
	error=HA_ERR_KEY_NOT_FOUND;
    }
  }
  if (do_prev)
  {
    bzero((void*) &row, sizeof(row));
    error= read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV),
		    buf, active_index, &row, &last_key, 1);
  }
  DBUG_RETURN(error);
}

/*
  Read last key is solved by reading the next key and then reading
  the previous key
*/

int ha_tokudb::index_read_last(uchar *buf, const uchar *key, uint key_len)
{
  DBUG_ENTER("ha_tokudb::index_read_last");
  DBT row;
  int error;
  KEY *key_info= &table->key_info[active_index];

  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));

  /* read of partial key */
  pack_key(&last_key, active_index, key_buff, key, key_len);
  /* Store for compare */
  memcpy(key_buff2, key_buff, (key_len=last_key.size));
  key_info->handler.bdb_return_if_eq= 1;
  error=read_row(cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE),
		 buf, active_index, &row, (DBT*) 0, 0);
  key_info->handler.bdb_return_if_eq= 0;
  bzero((void*) &row, sizeof(row));
  if (read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV),
	       buf, active_index, &row, &last_key, 1) ||
      tokudb_key_cmp(table, key_info, key_buff2, key_len))
    error=HA_ERR_KEY_NOT_FOUND;
  DBUG_RETURN(error);
}


int ha_tokudb::index_next(uchar *buf)
{
  DBUG_ENTER("ha_tokudb::index_next");
  DBT row;
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       buf, active_index, &row, &last_key, 1));
}

int ha_tokudb::index_next_same(uchar *buf, const uchar *key, uint keylen)
{
  DBUG_ENTER("ha_tokudb::index_next_same");
  DBT row;
  int error;
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  if (keylen == table->key_info[active_index].key_length &&
      !(table->key_info[active_index].flags & HA_END_SPACE_KEY))
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT_DUP),
		   buf, active_index, &row, &last_key, 1);
  else
  {
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		   buf, active_index, &row, &last_key, 1);
    if (!error && ::key_cmp_if_same(table, key, active_index, keylen))
      error=HA_ERR_END_OF_FILE;
  }
  DBUG_RETURN(error);
}

int ha_tokudb::index_prev(uchar *buf)
{
  DBUG_ENTER("ha_tokudb::index_prev");
  DBT row;
  statistic_increment(table->in_use->status_var.ha_read_prev_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV),
		       buf, active_index, &row, &last_key, 1));
}

int ha_tokudb::index_first(uchar *buf)
{
  DBUG_ENTER("ha_tokudb::index_first");
  DBT row;
  statistic_increment(table->in_use->status_var.ha_read_first_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_FIRST),
		       buf, active_index, &row, &last_key, 1));
}

int ha_tokudb::index_last(uchar *buf)
{
  DBUG_ENTER("ha_tokudb::index_last");
  DBT row;
  statistic_increment(table->in_use->status_var.ha_read_last_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_LAST),
		       buf, active_index, &row, &last_key, 0));
}

int ha_tokudb::rnd_init(bool scan)
{
  DBUG_ENTER("ha_tokudb::rnd_init");
  current_row.flags=DB_DBT_REALLOC;
  DBUG_RETURN(index_init(primary_key, 0));
}

int ha_tokudb::rnd_end()
{
  return index_end();
}

int ha_tokudb::rnd_next(uchar *buf)
{
  DBUG_ENTER("ha_tokudb::ha_tokudb::rnd_next");
  DBT row;
  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
  bzero((void*) &row, sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       buf, primary_key, &row, &last_key, 1));
}


DBT *ha_tokudb::get_pos(DBT *to, uchar *pos)
{
  /* We don't need to set app_data here */
  bzero((void*)to, sizeof(*to));

  to->data=pos;
  if (share->fixed_length_primary_key)
    to->size=ref_length;
  else
  {
    KEY_PART_INFO *key_part=table->key_info[primary_key].key_part;
    KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;

    for (; key_part != end; key_part++)
      pos+=key_part->field->packed_col_length(pos, key_part->length);
    to->size= (uint) (pos - (uchar*) to->data);
  }
  DBUG_DUMP("key", (const uchar *) to->data, to->size);
  return to;
}

int ha_tokudb::rnd_pos(uchar *buf, uchar *pos)
{
  DBUG_ENTER("ha_tokudb::rnd_pos");
  DBT db_pos;
  statistic_increment(table->in_use->status_var.ha_read_rnd_count,
		      &LOCK_status);
  active_index= MAX_KEY;
  DBUG_RETURN(read_row(file->get(file, transaction,
				 get_pos(&db_pos, pos),
				 &current_row, 0),
		       buf, primary_key, &current_row, (DBT*) 0, 0));
}

/*
  Set a reference to the current record in (ref,ref_length).

  SYNOPSIS
  ha_tokudb::position()
  record                      The current record buffer

  DESCRIPTION
  The BDB handler stores the primary key in (ref,ref_length).
  There is either an explicit primary key, or an implicit (hidden)
  primary key.
  During open(), 'ref_length' is calculated as the maximum primary
  key length. When an actual key is shorter than that, the rest of
  the buffer must be cleared out. The row cannot be identified, if
  garbage follows behind the end of the key. There is no length
  field for the current key, so that the whole ref_length is used
  for comparison.

  RETURN
  nothing
*/

void ha_tokudb::position(const uchar *record)
{
  DBUG_ENTER("ha_tokudb::position");
  DBT key;
  if (hidden_primary_key)
  {
    DBUG_ASSERT(ref_length == TDB_HIDDEN_PRIMARY_KEY_LENGTH);
    memcpy_fixed(ref, (char*) current_ident, TDB_HIDDEN_PRIMARY_KEY_LENGTH);
  }
  else
  {
    create_key(&key, primary_key, ref, record);
    if (key.size < ref_length)
      bzero(ref + key.size, ref_length - key.size);
  }
  DBUG_VOID_RETURN;
}

int ha_tokudb::info(uint flag)
{
  DBUG_ENTER("ha_tokudb::info");
  if (flag & HA_STATUS_VARIABLE)
  {
    // Just to get optimizations right
    stats.records = share->rows + changed_rows;
    stats.deleted = 0;
  }
  if ((flag & HA_STATUS_CONST) || version != share->version)
  {
    version=share->version;
    for (uint i=0; i < table_share->keys; i++)
    {
      table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]=
	share->rec_per_key[i];
    }
  }
  /* Don't return key if we got an error for the internal primary key */
  if (flag & HA_STATUS_ERRKEY && last_dup_key < table_share->keys)
    errkey= last_dup_key;
  DBUG_RETURN(0);
}


int ha_tokudb::extra(enum ha_extra_function operation)
{
  switch (operation) {
  case HA_EXTRA_RESET_STATE:
    reset();
    break;
  case HA_EXTRA_KEYREAD:
    key_read=1;  // Query satisfied with key
    break;
  case HA_EXTRA_NO_KEYREAD:
    key_read=0;
    break;
  case HA_EXTRA_IGNORE_DUP_KEY:
    using_ignore=1;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    using_ignore=0;
    break;
  default:
    break;
  }
  return 0;
}

int ha_tokudb::reset(void)
{
  key_read= 0;
  using_ignore= 0;
  if (current_row.flags & (DB_DBT_MALLOC | DB_DBT_REALLOC))
  {
    current_row.flags= 0;
    if (current_row.data)
    {
      free(current_row.data);
      current_row.data= 0;
    }
  }
  return 0;
}

/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement to be able to rollback the statement.
  If not, we have to start a master transaction if there doesn't exist
  one from before.
*/

int ha_tokudb::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_tokudb::external_lock");
  int error=0;
  tokudb_trx_data *trx=(tokudb_trx_data *)thd->ha_data[tdb_hton->slot];
  if (!trx)
  {
    thd->ha_data[tdb_hton->slot]= trx= (tokudb_trx_data *)
      my_malloc(sizeof(*trx), MYF(MY_ZEROFILL));
    if (!trx)
      DBUG_RETURN(1);
  }
  if (trx->all == 0)
    trx->sp_level= 0;
  if (lock_type != F_UNLCK)
  {
    if (!trx->tdb_lock_count++)
    {
      DBUG_ASSERT(trx->stmt == 0);
      transaction=0;  // Safety
      /* First table lock, start transaction */
      if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN |
			   OPTION_TABLE_LOCK)) && !trx->all)
      {
	/* We have to start a master transaction */
	DBUG_PRINT("trans",("starting transaction all:  options: 0x%lx",
			    (ulong) thd->options));
	if ((error= db_env->txn_begin(db_env, NULL, &trx->all, 0)))
	{
	  trx->tdb_lock_count--;  // We didn't get the lock
	  DBUG_RETURN(error);
	}
	trx->sp_level= trx->all;
	trans_register_ha(thd, TRUE, tdb_hton);
	if (thd->in_lock_tables)
	  DBUG_RETURN(0);  // Don't create stmt trans
      }
      DBUG_PRINT("trans",("starting transaction stmt"));
      if ((error= db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, 0)))
      {
	/* We leave the possible master transaction open */
	trx->tdb_lock_count--;  // We didn't get the lock
	DBUG_RETURN(error);
      }
      trans_register_ha(thd, FALSE, tdb_hton);
    }
    transaction= trx->stmt;
  }
  else
  {
    lock.type=TL_UNLOCK;  // Unlocked
    thread_safe_add(share->rows, changed_rows, &share->mutex);
    changed_rows=0;
    if (!--trx->tdb_lock_count)
    {
      if (trx->stmt)
      {
	/*
	  F_UNLCK is done without a transaction commit / rollback.
	  This happens if the thread didn't update any rows
	  We must in this case commit the work to keep the row locks
	*/
	DBUG_PRINT("trans",("commiting non-updating transaction"));
	error= trx->stmt->commit(trx->stmt,0);
	trx->stmt= transaction= 0;
      }
    }
  }
  DBUG_RETURN(error);
}


/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_tokudb::start_stmt(THD *thd, thr_lock_type lock_type)
{
  DBUG_ENTER("ha_tokudb::start_stmt");
  int error=0;
  tokudb_trx_data *trx= (tokudb_trx_data *)thd->ha_data[tdb_hton->slot];
  DBUG_ASSERT(trx);
  /*
    note that trx->stmt may have been already initialized as start_stmt()
    is called for *each table* not for each storage engine,
    and there could be many bdb tables referenced in the query
  */
  if (!trx->stmt)
  {
    DBUG_PRINT("trans",("starting transaction stmt"));
    error= db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, 0);
    trans_register_ha(thd, FALSE, tdb_hton);
  }
  transaction= trx->stmt;
  DBUG_RETURN(error);
}

/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Tokudb DB changes all WRITE locks to TL_WRITE_ALLOW_WRITE (which
  signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.
*/

THR_LOCK_DATA **ha_tokudb::store_lock(THD *thd, THR_LOCK_DATA **to,
                                        enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /* If we are not doing a LOCK TABLE, then allow multiple writers */
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
	 lock_type <= TL_WRITE) &&
	!thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;
    lock.type= lock_type;
  }
  *to++= &lock;
  return to;
}


static int create_sub_table(const char *table_name, const char *sub_name,
                            DBTYPE type, int flags)
{
  DBUG_ENTER("create_sub_table");
  int error;
  DB *file;
  DBUG_PRINT("enter",("sub_name: %s  flags: %d",sub_name, flags));

  if (!(error=db_create(&file, db_env, 0)))
  {
    file->set_flags(file, flags);
    error=(file->open(file, NULL, table_name, sub_name, type,
		      DB_THREAD | DB_CREATE, my_umask));
    if (error)
    {
      DBUG_PRINT("error",("Got error: %d when opening table '%s'",error,
			  table_name));
      (void) file->remove(file,table_name,NULL,0);
    }
    else
      (void) file->close(file,0);
  }
  else
  {
    DBUG_PRINT("error",("Got error: %d when creating table",error));
  }
  if (error)
    my_errno=error;
  DBUG_RETURN(error);
}

int ha_tokudb::create(const char *name, TABLE *form,
                        HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_tokudb::create");
  char name_buff[FN_REFLEN];
  char part[7];
  uint index=1;
  int error;

  fn_format(name_buff,name,"", ha_tokudb_ext,
	    MY_UNPACK_FILENAME|MY_APPEND_EXT);

  /* Create the main table that will hold the real rows */
  if ((error= create_sub_table(name_buff,"main",DB_BTREE,0)))
    DBUG_RETURN(error);

  primary_key= form->s->primary_key;
  /* Create the keys */
  for (uint i=0; i < form->s->keys; i++)
  {
    if (i != primary_key)
    {
      sprintf(part,"key%02d",index++);
      if ((error= create_sub_table(name_buff, part, DB_BTREE,
				   (form->key_info[i].flags & HA_NOSAME) ? 0 :
				   DB_DUP+DB_DUPSORT)))
	DBUG_RETURN(error);
    }
  }

  /* Create the status block to save information from last status command */
  /* Is DB_BTREE the best option here ? (QUEUE can't be used in sub tables) */

  DB *status_block;
  if (!(error=(db_create(&status_block, db_env, 0))))
  {
    if (!(error=(status_block->open(status_block, NULL, name_buff,
				    "status", DB_BTREE, DB_CREATE, 0))))
    {
      char rec_buff[4+MAX_KEY*4];
      uint length= 4+ form->s->keys*4;
      bzero(rec_buff, length);
      error= write_status(status_block, rec_buff, length);
      status_block->close(status_block,0);
    }
  }
  DBUG_RETURN(error);
}



int ha_tokudb::delete_table(const char *name)
{
  DBUG_ENTER("ha_tokudb::delete_table");
  int error;
  char name_buff[FN_REFLEN];
  if ((error=db_create(&file, db_env, 0)))
    my_errno=error;
  else
    error=file->remove(file,fn_format(name_buff,name,"",ha_tokudb_ext,
				      MY_UNPACK_FILENAME|MY_APPEND_EXT),
		       NULL,0);
  file= 0;  // Safety
  DBUG_RETURN(error);
}


int ha_tokudb::rename_table(const char * from, const char * to)
{
  int error;
  char from_buff[FN_REFLEN];
  char to_buff[FN_REFLEN];

  if ((error= db_create(&file, db_env, 0)))
    my_errno= error;
  else
  {
    /* On should not do a file->close() after rename returns */
    error= file->rename(file,
			fn_format(from_buff, from, "",
				  ha_tokudb_ext,
				  MY_UNPACK_FILENAME|MY_APPEND_EXT),
			NULL, fn_format(to_buff, to, "", ha_tokudb_ext,
					MY_UNPACK_FILENAME|MY_APPEND_EXT), 0);
  }
  return error;
}


/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_tokudb::scan_time()
{
  return rows2double(stats.records/3);
}

ha_rows ha_tokudb::records_in_range(uint keynr, key_range *start_key,
                                      key_range *end_key)
{
  DBUG_ENTER("ha_tokudb::records_in_range");
  DBT key;
  DB_KEY_RANGE start_range, end_range;
  DB *kfile=key_file[keynr];
  double start_pos,end_pos,rows;
  bool error;
  KEY *key_info= &table->key_info[keynr];

  /* Ensure we get maximum range, even for varchar keys with different space */
  key_info->handler.bdb_return_if_eq= -1;
  error= ((start_key && kfile->key_range(kfile,transaction,
					 pack_key(&key, keynr, key_buff,
						  start_key->key,
						  start_key->length),
					 &start_range,0)));
  if (error)
  {
    key_info->handler.bdb_return_if_eq= 0;
    // Better than returning an error
    DBUG_RETURN(HA_TOKUDB_RANGE_COUNT);
  }
  key_info->handler.bdb_return_if_eq= 1;
  error= (end_key && kfile->key_range(kfile,transaction,
				      pack_key(&key, keynr, key_buff,
					       end_key->key,
					       end_key->length),
				      &end_range,0));
  key_info->handler.bdb_return_if_eq= 0;
  if (error)
  {
    // Better than returning an error
    DBUG_RETURN(HA_TOKUDB_RANGE_COUNT);
  }

  if (!start_key)
    start_pos= 0.0;
  else if (start_key->flag == HA_READ_KEY_EXACT)
    start_pos=start_range.less;
  else
    start_pos=start_range.less+start_range.equal;

  if (!end_key)
    end_pos= 1.0;
  else if (end_key->flag == HA_READ_BEFORE_KEY)
    end_pos=end_range.less;
  else
    end_pos=end_range.less+end_range.equal;
  rows=(end_pos-start_pos)*stats.records;
  DBUG_PRINT("exit",("rows: %g",rows));
  DBUG_RETURN((ha_rows)(rows <= 1.0 ? 1 : rows));
}


void ha_tokudb::get_auto_increment(ulonglong offset, ulonglong increment,
                                     ulonglong nb_desired_values,
                                     ulonglong *first_value,
                                     ulonglong *nb_reserved_values)
{
  /* Ideally in case of real error (not "empty table") nr should be ~ULL(0) */
  ulonglong nr=1;  // Default if error or new key
  int error;
  (void) ha_tokudb::extra(HA_EXTRA_KEYREAD);

  /* Set 'active_index' */
  ha_tokudb::index_init(table_share->next_number_index, 0);

  if (!table_share->next_number_key_offset)
  {  // Autoincrement at key-start
    error=ha_tokudb::index_last(table->record[1]);
    /* has taken read lock on page of max key so reserves to infinite  */
    *nb_reserved_values= ULONGLONG_MAX;
  }
  else
  {
    /*
      MySQL needs to call us for next row: assume we are inserting ("a",null)
      here, we return 3, and next this statement will want to insert ("b",null):
      there is no reason why ("b",3+1) would be the good row to insert: maybe it
      already exists, maybe 3+1 is too large...
    */
    *nb_reserved_values= 1;
    DBT row, old_key;
    bzero((void*) &row, sizeof(row));
    KEY *key_info= &table->key_info[active_index];

    /* Reading next available number for a sub key */
    ha_tokudb::create_key(&last_key, active_index,
			    key_buff, table->record[0],
			    table_share->next_number_key_offset);
    /* Store for compare */
    memcpy(old_key.data=key_buff2, key_buff, (old_key.size=last_key.size));
    // DBTs dont have app_private members anymore, that was a MySQL hack
    // instead it seems to be called app_data
    // old_key.app_private= (void*)key_info;
    // old_key.app_data = (void*)key_info;
    error= 1;
    {
      /* Modify the compare so that we will find the next key */
      key_info->handler.bdb_return_if_eq= 1;
      /* We lock the next key as the new key will probl. be on the same page */
      error=cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE | DB_RMW);
      key_info->handler.bdb_return_if_eq= 0;
      if (!error || error == DB_NOTFOUND)
      {
	/*
	  Now search go one step back and then we should have found the
	  biggest key with the given prefix
	*/
	error= 1;
	if (!cursor->c_get(cursor, &last_key, &row, DB_PREV | DB_RMW) &&
	    !tokudb_cmp_packed_key(key_file[active_index], &old_key,
				     &last_key))
	{
	  error=0;  // Found value
	  unpack_key(table->record[1], &last_key, active_index);
	}
      }
    }
  }
  if (!error)
    nr= (ulonglong)
      table->next_number_field->val_int_offset(0)+1;
  ha_tokudb::index_end();
  (void) ha_tokudb::extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
}

void ha_tokudb::print_error(int error, myf errflag)
{
  if (error == DB_LOCK_DEADLOCK)
    error= HA_ERR_LOCK_DEADLOCK;
  handler::print_error(error,errflag);
}

int ha_tokudb::analyze(THD* thd, HA_CHECK_OPT* check_opt)
{
#if 0
  uint i;
  DB_BTREE_STAT *stat= 0;
  DB_TXN_STAT *txn_stat_ptr= 0;
  tokudb_trx_data *trx= (tokudb_trx_data *)thd->ha_data[tdb_hton->slot];
  DBUG_ASSERT(trx);

  for (i=0; i < table_share->keys; i++)
  {
    if (stat)
    {
      free(stat);
      stat= 0;
    }
    if ((key_file[i]->stat)(key_file[i], trx->all, (void*) &stat, 0))
      goto err;
    share->rec_per_key[i]= (stat->bt_ndata /
			    (stat->bt_nkeys ? stat->bt_nkeys : 1));
  }
  /* A hidden primary key is not in key_file[] */
  if (hidden_primary_key)
  {
    if (stat)
    {
      free(stat);
      stat= 0;
    }
    if ((file->stat)(file, trx->all, (void*) &stat, 0))
      goto err;
  }
  pthread_mutex_lock(&share->mutex);
  share->rows= stat->bt_ndata;
  share->status |= STATUS_TDB_ANALYZE;  // Save status on close
  share->version++;  // Update stat in table
  pthread_mutex_unlock(&share->mutex);
  update_status(share,table);  // Write status to file
  if (stat)
    free(stat);
  return ((share->status & STATUS_TDB_ANALYZE) ? HA_ADMIN_FAILED :
	  HA_ADMIN_OK);

err:
  if (stat)
    free(stat);
#endif
  return HA_ADMIN_FAILED;
}

int ha_tokudb::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  return ha_tokudb::analyze(thd,check_opt);
}

int ha_tokudb::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_tokudb::check");
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  // look in old_ha_tokudb.cc for a start of an implementation
}

struct st_mysql_storage_engine storage_engine_structure=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

// options flags
//   PLUGIN_VAR_THDLOCAL  Variable is per-connection
//   PLUGIN_VAR_READONLY  Server variable is read only
//   PLUGIN_VAR_NOSYSVAR  Not a server variable
//   PLUGIN_VAR_NOCMDOPT  Not a command line option
//   PLUGIN_VAR_NOCMDARG  No argument for cmd line
//   PLUGIN_VAR_RQCMDARG  Argument required for cmd line
//   PLUGIN_VAR_OPCMDARG  Argument optional for cmd line
//   PLUGIN_VAR_MEMALLOC  String needs memory allocated

static MYSQL_SYSVAR_ULONG(cache_parts,
                          tokudb_cache_parts,
                          PLUGIN_VAR_READONLY,
                          "Sets bdb set_cachesize ncache",
                          NULL, NULL, 0, 0, ~0L, 0);

static MYSQL_SYSVAR_ULONGLONG(cache_size,
                              tokudb_cache_size,
                              PLUGIN_VAR_READONLY,
                              "Sets bdb set_cachesize gbytes & bytes",
                              NULL, NULL, 8*1024*1024, 0, ~0LL, 0);

// this is really a u_int32_t
// ? use MYSQL_SYSVAR_SET
static MYSQL_SYSVAR_UINT(env_flags,
                         tokudb_env_flags,
                         PLUGIN_VAR_READONLY,
                         "Sets bdb set_flags",
                         NULL, NULL, DB_LOG_AUTOREMOVE, 0, ~0, 0);

static MYSQL_SYSVAR_STR(home,
			tokudb_home,
                        PLUGIN_VAR_READONLY,
                        "Sets bdb DB_ENV->open db_home",
                        NULL, NULL, NULL);

// this is really a u_int32_t
//? use MYSQL_SYSVAR_SET
static MYSQL_SYSVAR_UINT(init_flags,
                         tokudb_init_flags,
                         PLUGIN_VAR_READONLY,
                         "Sets bdb DB_ENV->open flags",
                         NULL, NULL, DB_PRIVATE | DB_RECOVER, 0, ~0, 0);

// this looks to be unused
static MYSQL_SYSVAR_LONG(lock_scan_time,
                         tokudb_lock_scan_time,
                         PLUGIN_VAR_READONLY,
                         "Tokudb Lock Scan Time (UNUSED)",
                         NULL, NULL, 0, 0, ~0L, 0);

// this is really a u_int32_t
//? use MYSQL_SYSVAR_ENUM
static MYSQL_SYSVAR_UINT(lock_type,
                         tokudb_lock_type,
                         PLUGIN_VAR_READONLY,
			 "Sets set_lk_detect",
                         NULL, NULL, DB_LOCK_DEFAULT, 0, ~0, 0);

static MYSQL_SYSVAR_ULONG(log_buffer_size,
                          tokudb_log_buffer_size,
                          PLUGIN_VAR_READONLY,
                          "Tokudb Log Buffer Size",
                          NULL, NULL, 0, 0, ~0L, 0);

static MYSQL_SYSVAR_STR(logdir, tokudb_logdir,
                        PLUGIN_VAR_READONLY,
                        "Tokudb Log Dir",
                        NULL, NULL, NULL);

static MYSQL_SYSVAR_ULONG(max_lock,
                          tokudb_max_lock,
                          PLUGIN_VAR_READONLY,
                          "Tokudb Max Lock",
                          NULL, NULL, 8*1024, 0, ~0L, 0);

static MYSQL_SYSVAR_ULONG(region_size,
                          tokudb_region_size,
                          PLUGIN_VAR_READONLY,
                          "Tokudb Region Size",
                          NULL, NULL, 128*1024, 0, ~0L, 0);

static MYSQL_SYSVAR_BOOL(shared_data,
                         tokudb_shared_data,
                         PLUGIN_VAR_READONLY,
                         "Tokudb Shared Data",
                         NULL, NULL, FALSE);

static MYSQL_SYSVAR_STR(tmpdir, tokudb_tmpdir,
                        PLUGIN_VAR_READONLY,
                        "Tokudb Tmp Dir",
                        NULL, NULL, NULL);

static struct st_mysql_sys_var* tdb_system_variables[]= {
  MYSQL_SYSVAR(cache_parts),
  MYSQL_SYSVAR(cache_size),
  MYSQL_SYSVAR(env_flags),
  MYSQL_SYSVAR(home),
  MYSQL_SYSVAR(init_flags),
  MYSQL_SYSVAR(lock_scan_time),
  MYSQL_SYSVAR(lock_type),
  MYSQL_SYSVAR(log_buffer_size),
  MYSQL_SYSVAR(logdir),
  MYSQL_SYSVAR(max_lock),
  MYSQL_SYSVAR(region_size),
  MYSQL_SYSVAR(shared_data),
  MYSQL_SYSVAR(tmpdir),
  NULL
};

mysql_declare_plugin(tokudb)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &storage_engine_structure,
  "TokuDB",
  "Tokutek Inc",
  "Supports transactions and page-level locking",
  PLUGIN_LICENSE_BSD,
  tdb_init_func, /* plugin init */
  tdb_done_func, /* plugin deinit */
  0x0200, /* 2.0 */
  NULL, /* status variables */
  tdb_system_variables, /* system variables */
  NULL /* config options */
}
mysql_declare_plugin_end;
