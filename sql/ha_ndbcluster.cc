/* Copyright (C) 2000-2003 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "rpl_mi.h"

#include <my_dir.h>
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include "ha_ndbcluster.h"
#include <ndbapi/NdbApi.hpp>
#include "ha_ndbcluster_cond.h"
#include <../util/Bitmask.hpp>
#include <ndbapi/NdbIndexStat.hpp>

#include "ha_ndbcluster_binlog.h"
#include "ha_ndbcluster_tables.h"

#include <mysql/plugin.h>

#ifdef ndb_dynamite
#undef assert
#define assert(x) do { if(x) break; ::printf("%s %d: assert failed: %s\n", __FILE__, __LINE__, #x); ::fflush(stdout); ::signal(SIGABRT,SIG_DFL); ::abort(); ::kill(::getpid(),6); ::kill(::getpid(),9); } while (0)
#endif

// options from from mysqld.cc
extern my_bool opt_ndb_optimized_node_selection;
extern const char *opt_ndbcluster_connectstring;
extern ulong opt_ndb_cache_check_time;
extern ulong opt_ndb_wait_connected;
extern ulong opt_ndb_cluster_connection_pool;

// ndb interface initialization/cleanup
#ifdef  __cplusplus
extern "C" {
#endif
extern void ndb_init_internal();
extern void ndb_end_internal();
#ifdef  __cplusplus
}
#endif

const char *ndb_distribution_names[]= {"KEYHASH", "LINHASH", NullS};
TYPELIB ndb_distribution_typelib= { array_elements(ndb_distribution_names)-1,
                                    "", ndb_distribution_names, NULL };
const char *opt_ndb_distribution= ndb_distribution_names[ND_KEYHASH];
enum ndb_distribution opt_ndb_distribution_id= ND_KEYHASH;

// Default value for parallelism
static const int parallelism= 0;

// Default value for max number of transactions
// createable against NDB from this handler
static const int max_transactions= 3; // should really be 2 but there is a transaction to much allocated when loch table is used

static uint ndbcluster_partition_flags();
static uint ndbcluster_alter_table_flags(uint flags);
static int ndbcluster_init(void *);
static int ndbcluster_end(handlerton *hton, ha_panic_function flag);
static bool ndbcluster_show_status(handlerton *hton, THD*,
                                   stat_print_fn *,
                                   enum ha_stat_type);
static int ndbcluster_alter_tablespace(handlerton *hton,
                                       THD* thd, 
                                       st_alter_tablespace *info);
static int ndbcluster_fill_files_table(handlerton *hton,
                                       THD *thd, 
                                       TABLE_LIST *tables, 
                                       COND *cond);

handlerton *ndbcluster_hton;

static handler *ndbcluster_create_handler(handlerton *hton,
                                          TABLE_SHARE *table,
                                          MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ndbcluster(hton, table);
}

static uint ndbcluster_partition_flags()
{
  return (HA_CAN_PARTITION | HA_CAN_UPDATE_PARTITION_KEY |
          HA_CAN_PARTITION_UNIQUE | HA_USE_AUTO_PARTITION);
}

static uint ndbcluster_alter_table_flags(uint flags)
{
  if (flags & ALTER_DROP_PARTITION)
    return 0;
  else
    return (HA_ONLINE_ADD_INDEX | HA_ONLINE_DROP_INDEX |
            HA_ONLINE_ADD_UNIQUE_INDEX | HA_ONLINE_DROP_UNIQUE_INDEX |
            HA_PARTITION_FUNCTION_SUPPORTED);

}

#define NDB_AUTO_INCREMENT_RETRIES 10
#define BATCH_FLUSH_SIZE (32768)

#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("%d  message: %s", err.code, err.message))

#define ERR_RETURN(err)                  \
{                                        \
  const NdbError& tmp= err;              \
  ERR_PRINT(tmp);                        \
  DBUG_RETURN(ndb_to_mysql_error(&tmp)); \
}

#define ERR_BREAK(err, code)             \
{                                        \
  const NdbError& tmp= err;              \
  ERR_PRINT(tmp);                        \
  code= ndb_to_mysql_error(&tmp);        \
  break;                                 \
}

static int ndbcluster_inited= 0;
int ndbcluster_terminating= 0;

static Ndb* g_ndb= NULL;
Ndb_cluster_connection* g_ndb_cluster_connection= NULL;
Ndb_cluster_connection **g_ndb_cluster_connection_pool= NULL;
ulong g_ndb_cluster_connection_pool_alloc= 0;
ulong g_ndb_cluster_connection_pool_pos= 0;
pthread_mutex_t g_ndb_cluster_connection_pool_mutex;
uchar g_node_id_map[max_ndb_nodes];

// Handler synchronization
pthread_mutex_t ndbcluster_mutex;

// Table lock handling
HASH ndbcluster_open_tables;

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)));
#ifdef HAVE_NDB_BINLOG
static int rename_share(NDB_SHARE *share, const char *new_key);
#endif
static
NdbRecord *
ndb_get_table_statistics_ndbrecord(NDBDICT *, const NDBTAB *);
static int ndb_get_table_statistics(ha_ndbcluster*, bool, Ndb*, const NDBTAB *, 
                                    struct Ndb_statistics *);
static int ndb_get_table_statistics(ha_ndbcluster*, bool, Ndb*,
                                    const NdbRecord *, struct Ndb_statistics *);


// Util thread variables
pthread_t ndb_util_thread;
int ndb_util_thread_running= 0;
pthread_mutex_t LOCK_ndb_util_thread;
pthread_cond_t COND_ndb_util_thread;
pthread_cond_t COND_ndb_util_ready;
pthread_handler_t ndb_util_thread_func(void *arg);
ulong ndb_cache_check_time;

/*
  Stats that can be retrieved from ndb
*/

struct Ndb_statistics {
  Uint64 row_count;
  Uint64 commit_count;
  Uint64 row_size;
  Uint64 fragment_memory;
};

/* Status variables shown with 'show status like 'Ndb%' */

struct st_ndb_status {
  st_ndb_status() { bzero(this, sizeof(struct st_ndb_status)); }
  long cluster_node_id;
  const char * connected_host;
  long connected_port;
  long number_of_replicas;
  long number_of_data_nodes;
  long number_of_ready_data_nodes;
  long connect_count;
};

static struct st_ndb_status g_ndb_status;

static int update_status_variables(st_ndb_status *ns, Ndb_cluster_connection *c)
{
  ns->connected_port= c->get_connected_port();
  ns->connected_host= c->get_connected_host();
  if (ns->cluster_node_id != (int) c->node_id())
  {
    ns->cluster_node_id= c->node_id();
    if (&g_ndb_status == ns && g_ndb_cluster_connection == c)
      sql_print_information("NDB: NodeID is %lu, management server '%s:%lu'",
                            ns->cluster_node_id, ns->connected_host,
                            ns->connected_port);
  }
  ns->number_of_replicas= 0;
  ns->number_of_ready_data_nodes= c->get_no_ready();
  ns->number_of_data_nodes= c->no_db_nodes();
  ns->connect_count= c->get_connect_count();
  return 0;
}

SHOW_VAR ndb_status_variables[]= {
  {"cluster_node_id",     (char*) &g_ndb_status.cluster_node_id,      SHOW_LONG},
  {"config_from_host",    (char*) &g_ndb_status.connected_host,       SHOW_CHAR_PTR},
  {"config_from_port",    (char*) &g_ndb_status.connected_port,       SHOW_LONG},
//{"number_of_replicas",  (char*) &g_ndb_status.number_of_replicas,   SHOW_LONG},
  {"number_of_data_nodes",(char*) &g_ndb_status.number_of_data_nodes, SHOW_LONG},
  {NullS, NullS, SHOW_LONG}
};

/*
  Error handling functions
*/

/* Note for merge: old mapping table, moved to storage/ndb/ndberror.c */

static int ndb_to_mysql_error(const NdbError *ndberr)
{
  /* read the mysql mapped error code */
  int error= ndberr->mysql_code;

  switch (error)
  {
    /* errors for which we do not add warnings, just return mapped error code
    */
  case HA_ERR_NO_SUCH_TABLE:
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_FOUND_DUPP_KEY:
    return error;

    /* Mapping missing, go with the ndb error code*/
  case -1:
    error= ndberr->code;
    break;

    /* Mapping exists, go with the mapped code */
  default:
    break;
  }

  /*
    Push the NDB error message as warning
    - Used to be able to use SHOW WARNINGS toget more info on what the error is
    - Used by replication to see if the error was temporary
  */
  if (ndberr->status == NdbError::TemporaryError)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_TEMPORARY_ERRMSG, ER(ER_GET_TEMPORARY_ERRMSG),
			ndberr->code, ndberr->message, "NDB");
  else
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
			ndberr->code, ndberr->message, "NDB");
  return error;
}

/*
  When execute() is called, this resets the internal state on things that
  were awaiting execute(), such as pending scan take-over operations and
  rows for batched operations.
  Also used to initialize the state at start of a statement.
*/
void ha_ndbcluster::reset_state_at_execute()
{
  Thd_ndb *thd_ndb= get_thd_ndb(current_thd);
  m_ops_pending= 0;
  m_blobs_pending= FALSE;
  thd_ndb->m_unsent_bytes= 0;
}

static int
check_completed_operations(NdbTransaction *trans, const NdbOperation *first)
{
  DBUG_ENTER("check_completed_operations");
  /*
    Check that all errors are "accepted" errors
  */
  while (first)
  {
    const NdbError &err= first->getNdbError();
    if (err.classification != NdbError::NoError &&
        err.classification != NdbError::ConstraintViolation &&
        err.classification != NdbError::NoDataFound)
      DBUG_RETURN(err.code);
    first= trans->getNextCompletedOperation(first);
  }
  DBUG_RETURN(0);
}

inline
int execute_no_commit(ha_ndbcluster *h, NdbTransaction *trans,
		      bool force_release)
{
  DBUG_ENTER("execute_no_commit");
  h->release_completed_operations(trans, force_release);
  const NdbOperation *first= trans->getFirstDefinedOperation();
  if (trans->execute(NdbTransaction::NoCommit,
                      NdbOperation::AO_IgnoreError,
                      h->m_force_send))
  {
    h->reset_state_at_execute();
    DBUG_RETURN(-1);
  }
  h->reset_state_at_execute();
  if (!h->m_ignore_no_key || trans->getNdbError().code == 0)
    DBUG_RETURN(trans->getNdbError().code);

  DBUG_RETURN(check_completed_operations(trans, first));
}

inline
int execute_commit(NdbTransaction *trans, int force_send, int ignore_error)
{
  DBUG_ENTER("execute_commit");
  const NdbOperation *first= trans->getFirstDefinedOperation();
  if (trans->execute(NdbTransaction::Commit,
                     NdbOperation::AO_IgnoreError,
                     force_send))
  {
    DBUG_RETURN(-1);
  }
  if (!ignore_error || trans->getNdbError().code == 0)
    DBUG_RETURN(trans->getNdbError().code);
  DBUG_RETURN(check_completed_operations(trans, first));
}

inline
int execute_commit(ha_ndbcluster *h, NdbTransaction *trans)
{
  int res= execute_commit(trans, h->m_force_send, h->m_ignore_no_key);
  h->reset_state_at_execute();
  return res;
}

inline
int execute_commit(THD *thd, NdbTransaction *trans)
{
  /*
    We do not have to reset_state_at_execute() here, as this is at transaction
    end time, and we will not take further action after this (nor can we
    easily, as we execute outside the context of the ha_ndbcluster object).
  */
  return execute_commit(trans, thd->variables.ndb_force_send, FALSE);
}

inline
int execute_no_commit_ie(ha_ndbcluster *h, NdbTransaction *trans,
			 bool force_release)
{
  DBUG_ENTER("execute_no_commit_ie");
  h->release_completed_operations(trans, force_release);
  int res= trans->execute(NdbTransaction::NoCommit,
                          NdbOperation::AO_IgnoreError,
                          h->m_force_send);
  h->reset_state_at_execute();
  DBUG_RETURN(res);
}

/*
  Place holder for ha_ndbcluster thread specific data
*/
static
byte *thd_ndb_share_get_key(THD_NDB_SHARE *thd_ndb_share, uint *length,
                            my_bool not_used __attribute__((unused)))
{
  *length= sizeof(thd_ndb_share->key);
  return (byte*) &thd_ndb_share->key;
}

Thd_ndb::Thd_ndb()
{
  pthread_mutex_lock(&g_ndb_cluster_connection_pool_mutex);
  connection=
    g_ndb_cluster_connection_pool[g_ndb_cluster_connection_pool_pos];
  g_ndb_cluster_connection_pool_pos++;
  if (g_ndb_cluster_connection_pool_pos ==
      g_ndb_cluster_connection_pool_alloc)
    g_ndb_cluster_connection_pool_pos= 0;
  pthread_mutex_unlock(&g_ndb_cluster_connection_pool_mutex);
  ndb= new Ndb(connection, "");
  lock_count= 0;
  count= 0;
  all= NULL;
  stmt= NULL;
  m_error= FALSE;
  query_state&= NDB_QUERY_NORMAL;
  options= 0;
  (void) hash_init(&open_tables, &my_charset_bin, 5, 0, 0,
                   (hash_get_key)thd_ndb_share_get_key, 0, 0);
  m_unsent_bytes= 0;
  init_alloc_root(&m_batch_mem_root, BATCH_FLUSH_SIZE/4, 0);
}

Thd_ndb::~Thd_ndb()
{
  if (ndb)
  {
#ifndef DBUG_OFF
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      uint leaked= (uint) tmp.m_created - tmp.m_free;
      if (leaked)
        fprintf(stderr, "NDB: Found %u %s%s that %s not been released\n",
                leaked, tmp.m_name,
                (leaked == 1)?"":"'s",
                (leaked == 1)?"has":"have");
    }
#endif
    delete ndb;
    ndb= NULL;
  }
  changed_tables.empty();
  hash_free(&open_tables);
  free_root(&m_batch_mem_root, MYF(0));
}

void
Thd_ndb::init_open_tables()
{
  count= 0;
  m_error= FALSE;
  my_hash_reset(&open_tables);
}

THD_NDB_SHARE *
Thd_ndb::get_open_table(THD *thd, const void *key)
{
  DBUG_ENTER("Thd_ndb::get_open_table");
  HASH_SEARCH_STATE state;
  THD_NDB_SHARE *thd_ndb_share=
    (THD_NDB_SHARE*)hash_first(&open_tables, (byte *)&key, sizeof(key), &state);
  while (thd_ndb_share && thd_ndb_share->key != key)
    thd_ndb_share= (THD_NDB_SHARE*)hash_next(&open_tables, (byte *)&key, sizeof(key), &state);
  if (thd_ndb_share == 0)
  {
    thd_ndb_share= (THD_NDB_SHARE *) alloc_root(&thd->transaction.mem_root,
                                                sizeof(THD_NDB_SHARE));
    thd_ndb_share->key= key;
    thd_ndb_share->stat.last_count= count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
    my_hash_insert(&open_tables, (byte *)thd_ndb_share);
  }
  else if (thd_ndb_share->stat.last_count != count)
  {
    thd_ndb_share->stat.last_count= count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
  }
  DBUG_PRINT("exit", ("thd_ndb_share: 0x%lx  key: 0x%lx",
                      (long) thd_ndb_share, (long) key));
  DBUG_RETURN(thd_ndb_share);
}

inline
Ndb *ha_ndbcluster::get_ndb()
{
  return get_thd_ndb(current_thd)->ndb;
}

/*
 * manage uncommitted insert/deletes during transactio to get records correct
 */

void ha_ndbcluster::set_rec_per_key()
{
  DBUG_ENTER("ha_ndbcluster::set_rec_per_key");
  for (uint i=0 ; i < table_share->keys ; i++)
  {
    table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]= 1;
  }
  DBUG_VOID_RETURN;
}

ha_rows ha_ndbcluster::records()
{
  ha_rows retval;
  DBUG_ENTER("ha_ndbcluster::records");
  struct Ndb_local_table_statistics *local_info= m_table_info;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      local_info->no_uncommitted_rows_count));

  Ndb *ndb= get_ndb();
  ndb->setDatabaseName(m_dbname);
  struct Ndb_statistics stat;
  if (ndb_get_table_statistics(this, TRUE, ndb, m_ndb_statistics_record,
                               &stat) == 0)
  {
    retval= stat.row_count;
  }
  else
  {
    DBUG_RETURN(HA_POS_ERROR);
  }

  THD *thd= current_thd;
  if (get_thd_ndb(thd)->m_error)
    local_info->no_uncommitted_rows_count= 0;

  DBUG_RETURN(retval + local_info->no_uncommitted_rows_count);
}

int ha_ndbcluster::records_update()
{
  if (m_ha_not_exact_count)
    return 0;
  DBUG_ENTER("ha_ndbcluster::records_update");
  int result= 0;

  struct Ndb_local_table_statistics *local_info= m_table_info;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      local_info->no_uncommitted_rows_count));
  {
    Ndb *ndb= get_ndb();
    struct Ndb_statistics stat;
    if (ndb->setDatabaseName(m_dbname))
    {
      return my_errno= HA_ERR_OUT_OF_MEM;
    }
    result= ndb_get_table_statistics(this, TRUE, ndb, m_ndb_statistics_record,
                                     &stat);
    if (result == 0)
    {
      stats.mean_rec_length= stat.row_size;
      stats.data_file_length= stat.fragment_memory;
      local_info->records= stat.row_count;
    }
  }
  {
    THD *thd= current_thd;
    if (get_thd_ndb(thd)->m_error)
      local_info->no_uncommitted_rows_count= 0;
  }
  if (result == 0)
    stats.records= local_info->records+ local_info->no_uncommitted_rows_count;
  DBUG_RETURN(result);
}

void ha_ndbcluster::no_uncommitted_rows_execute_failure()
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_execute_failure");
  get_thd_ndb(current_thd)->m_error= TRUE;
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_update(int c)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_update");
  struct Ndb_local_table_statistics *local_info= m_table_info;
  local_info->no_uncommitted_rows_count+= c;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      local_info->no_uncommitted_rows_count));
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_reset(THD *thd)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_reset");
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  thd_ndb->count++;
  thd_ndb->m_error= FALSE;
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::ndb_err(NdbTransaction *trans)
{
  int res;
  NdbError err= trans->getNdbError();
  DBUG_ENTER("ndb_err");
  
  ERR_PRINT(err);
  switch (err.classification) {
  case NdbError::SchemaError:
  {
    // TODO perhaps we need to do more here, invalidate also in the cache
    m_table->setStatusInvalid();
    /* Close other open handlers not used by any thread */
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.db= m_dbname;
    table_list.alias= table_list.table_name= m_tabname;
    close_cached_tables(current_thd, 0, &table_list);
    break;
  }
  default:
    break;
  }
  res= ndb_to_mysql_error(&err);
  DBUG_PRINT("info", ("transformed ndbcluster error %d to mysql error %d", 
                      err.code, res));
  if (res == HA_ERR_FOUND_DUPP_KEY)
  {
    if (m_rows_to_insert == 1)
    {
      /*
	We can only distinguish between primary and non-primary
	violations here, so we need to return MAX_KEY for non-primary
	to signal that key is unknown
      */
      m_dupkey= err.code == 630 ? table_share->primary_key : MAX_KEY; 
    }
    else
    {
      /* We are batching inserts, offending key is not available */
      m_dupkey= (uint) -1;
    }
  }
  DBUG_RETURN(res);
}


/*
  Override the default get_error_message in order to add the 
  error message of NDB 
 */

bool ha_ndbcluster::get_error_message(int error, 
                                      String *buf)
{
  DBUG_ENTER("ha_ndbcluster::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  Ndb *ndb= get_ndb();
  if (!ndb)
    DBUG_RETURN(FALSE);

  const NdbError err= ndb->getNdbError(error);
  bool temporary= err.status==NdbError::TemporaryError;
  buf->set(err.message, strlen(err.message), &my_charset_bin);
  DBUG_PRINT("exit", ("message: %s, temporary: %d", buf->ptr(), temporary));
  DBUG_RETURN(temporary);
}


void
ha_ndbcluster::set_hidden_key(char *row, Uint64 auto_value)
{
  /* The hidden primary key is stored just after the normal row data. */
  uint32 offset= offset_hidden_key();
  memcpy(&row[offset], &auto_value, NDB_HIDDEN_PRIMARY_KEY_LENGTH);
}

Uint64
ha_ndbcluster::get_hidden_key(const char *row)
{
  Uint64 hidden_key;
  uint32 offset= offset_hidden_key();
  memcpy(&hidden_key, &row[offset], NDB_HIDDEN_PRIMARY_KEY_LENGTH);
  return hidden_key;
}

void
ha_ndbcluster::request_hidden_key(uchar *mask)
{
  uint32 field_no= field_number_hidden_key();
  mask[field_no>>3]|= (1 << (field_no & 7));
}

/*
  Check if MySQL field type forces var part in ndb storage
*/
static bool field_type_forces_var_part(enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    return TRUE;
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    return FALSE;
  default:
    return FALSE;
  }
}

void
ha_ndbcluster::set_partition_function_value(char *row, uint32 func_value)
{
  /* The partition function value is stored just after the hidden primary
     key (if any). */
  uint32 offset= offset_user_partition_function();
  memcpy(&row[offset], &func_value, 4);
}

uint32
ha_ndbcluster::get_partition_fragment(const char *row)
{
  uint32 fragment;
  uint32 offset= offset_user_partition_fragment();
  memcpy(&fragment, &row[offset], 4);
  return fragment;
}

void
ha_ndbcluster::request_partition_function_value(uchar *mask)
{
  uint32 field_no= field_number_user_partition_function();
  mask[field_no>>3]|= (1 << (field_no & 7));
}

static inline char *
alloc_batch_row(Thd_ndb *thd_ndb, uint size)
{
  /*
    We only reset the batch mem_root on first allocate after execute(), not
    immediately at execute() time.
    This is so that we have the chance after execute() to copy out data from
    any read buffers.
   */
  if (thd_ndb->m_unsent_bytes == 0)
    free_root(&(thd_ndb->m_batch_mem_root), MY_MARK_BLOCKS_FREE);
  return alloc_root(&(thd_ndb->m_batch_mem_root), size);
}

/*
  Copy a record into a newly allocated buffer.
  The returned record is valid until next execute() (actually until next
  allocation after next execute()).
  The input parameter op_batch_size is an estimate of the signal bytes
  needed for the operation; this is used to set the output parameter
  batch_full to true when it is time to flush the batch with execute().
*/
char *
ha_ndbcluster::batch_copy_row_to_buffer(Thd_ndb *thd_ndb, const byte *record,
                                        bool & batch_full)
{
  char *row= copy_row_to_buffer(thd_ndb, record);
  if (unlikely(!row))
    return NULL;
  uint unsent= thd_ndb->m_unsent_bytes;
  unsent+= m_bytes_per_write;
  batch_full= unsent >= BATCH_FLUSH_SIZE;
  thd_ndb->m_unsent_bytes= unsent;
  return row;
}

char *
ha_ndbcluster::batch_copy_key_to_buffer(Thd_ndb *thd_ndb, const byte *key,
                                        uint key_len,
                                        uint op_batch_size, bool & batch_full)
{
  char *row= alloc_batch_row(thd_ndb, key_len);
  if (unlikely(!row))
    return NULL;
  memcpy(row, key, key_len);
  uint unsent= thd_ndb->m_unsent_bytes;
  unsent+= op_batch_size;
  DBUG_ASSERT(op_batch_size > 0);
  batch_full= unsent >= BATCH_FLUSH_SIZE;
  thd_ndb->m_unsent_bytes= unsent;
  return row;
}

/*
  Simpler row buffer copy, for when we know we will not batch.
  Only valid until next buffer allocation.
*/
char *
ha_ndbcluster::copy_row_to_buffer(Thd_ndb *thd_ndb, const byte *record)
{
  char *row;
  uint size= table->s->reclength + m_extra_reclength;
  row= alloc_batch_row(thd_ndb, size);
  if (unlikely(!row))
    return NULL;
  memcpy(row, record, table->s->reclength);
  return row;
}

/* Return a row buffer, valid until next execute(). */
char *
ha_ndbcluster::get_row_buffer()
{
  Thd_ndb *thd_ndb= get_thd_ndb(table->in_use);
  return alloc_root(&(thd_ndb->m_batch_mem_root),
                    table->s->reclength + m_extra_reclength);
}

/*
  When using extra hidden columns, the mysqld column bitmaps do not
  include bits for the extra columns, so we use this method to initialize
  them (after copying the mysqld bitmap to a larger one).  
*/
void
ha_ndbcluster::clear_extended_column_set(uchar *mask)
{
  if (table_share->primary_key == MAX_KEY)
  {
    uint32 field_no= field_number_hidden_key();
    mask[field_no>>3]&= ~(1 << (field_no & 7));
  }
  if (m_use_partition_function)
  {
    uint32 field_no= field_number_user_partition_function();
    mask[field_no>>3]&= ~(1 << (field_no & 7));
  }
}

uchar *
ha_ndbcluster::copy_column_set(MY_BITMAP *bitmap)
{
  bitmap_copy(&m_bitmap, bitmap);
  uchar *mask= (uchar *)m_bitmap_buf;
  clear_extended_column_set(mask);
  return mask;
}

int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg)
{
  ha_ndbcluster *ha= (ha_ndbcluster *)arg;
  DBUG_ENTER("g_get_ndb_blobs_value");
  DBUG_PRINT("info", ("destination row: %p", ha->m_blob_destination_record));

  /* Count the total length needed for blob data. */
  int isNull;
  if (ndb_blob->getNull(isNull) != 0)
    ERR_RETURN(ndb_blob->getNdbError());
  if (isNull == 0) {
    Uint64 len64= 0;
    if (ndb_blob->getLength(len64) != 0)
      ERR_RETURN(ndb_blob->getNdbError());
    /* Align to Uint64. */
    ha->m_blob_total_size+= (len64 + 7) & ~((Uint64)7);
    if (ha->m_blob_total_size > 0xffffffff)
    {
      DBUG_ASSERT(FALSE);
      DBUG_RETURN(-1);
    }
  }
  ha->m_blob_counter++;

  /*
    Wait until all blobs are active with reading, so we can allocate
    and use a common buffer containing all.
  */
  if (ha->m_blob_counter < ha->m_blob_expected_count)
    DBUG_RETURN(0);
  ha->m_blob_counter= 0;

  /* Re-allocate bigger blob buffer if necessary. */
  if (ha->m_blob_total_size > ha->m_blobs_buffer_size)
  {
    my_free(ha->m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_PRINT("info", ("allocate blobs buffer size %u",
                        (uint32)(ha->m_blob_total_size)));
    ha->m_blobs_buffer= my_malloc(ha->m_blob_total_size, MYF(MY_WME));
    if (ha->m_blobs_buffer == NULL)
    {
      ha->m_blobs_buffer_size= 0;
      DBUG_RETURN(-1);
    }
    ha->m_blobs_buffer_size= ha->m_blob_total_size;
  }

  /*
    Now read all blob data.
    If we know the destination mysqld row, we also set the blob null bit and
    pointer/length (if not, it will be done instead in unpack_record()).
  */
  uint32 offset= 0;
  for (uint i= 0; i < ha->table->s->fields; i++)
  {
    Field *field= ha->table->field[i];
    if (! (field->flags & BLOB_FLAG))
      continue;
    NdbValue value= ha->m_value[i];
    if (value.blob == NULL)
    {
      DBUG_PRINT("info",("[%u] skipped", i));
      continue;
    }
    Field_blob *field_blob= (Field_blob *)field;
    NdbBlob *ndb_blob= value.blob;
    int isNull;
    if (ndb_blob->getNull(isNull) != 0)
      ERR_RETURN(ndb_blob->getNdbError());
    if (isNull == 0) {
      Uint64 len64= 0;
      if (ndb_blob->getLength(len64) != 0)
        ERR_RETURN(ndb_blob->getNdbError());
      DBUG_ASSERT(len64 < 0xffffffff);
      char *buf= ha->m_blobs_buffer + offset;
      uint32 len= ha->m_blobs_buffer_size - offset;
      if (ndb_blob->readData(buf, len) != 0)
          ERR_RETURN(ndb_blob->getNdbError());
      DBUG_PRINT("info", ("[%u] offset: %u  buf: 0x%lx  len=%u",
                          i, offset, (long) buf, len));
      DBUG_ASSERT(len == len64);
      if (ha->m_blob_destination_record)
      {
        my_ptrdiff_t ptrdiff=
          ha->m_blob_destination_record - ha->table->record[0];
        field_blob->move_field_offset(ptrdiff);
        field_blob->set_ptr(len, buf);
        field_blob->set_notnull();
        field_blob->move_field_offset(-ptrdiff);
      }
      offset+= ((len64 + 7) & ~((Uint64)7));
    }
    else if (ha->m_blob_destination_record)
    {
      /* Have to set length even in this case. */
      my_ptrdiff_t ptrdiff=
        ha->m_blob_destination_record - ha->table->record[0];
      char *buf= ha->m_blobs_buffer + offset;
      field_blob->move_field_offset(ptrdiff);
      field_blob->set_ptr((uint32)0, buf);
      field_blob->set_null();
      field_blob->move_field_offset(-ptrdiff);
      DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
    }
  }

  DBUG_RETURN(0);
}

/*
  Request reading of blob values.

  If dst_record is specified, the blob null bit, pointer, and length will be
  set in that record. Otherwise they must be set later by calling
  unpack_record().
*/
int
ha_ndbcluster::get_blob_values(NdbOperation *ndb_op, byte *dst_record,
                               const MY_BITMAP *bitmap)
{
  uint i;
  DBUG_ENTER("ha_ndbcluster::get_blob_values");

  m_blob_counter= 0;
  m_blob_expected_count= 0;
  m_blob_destination_record= dst_record;
  m_blob_total_size= 0;

  for (i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (!(field->flags & BLOB_FLAG))
      continue;

    DBUG_PRINT("info", ("fieldnr=%d", i));
    NdbBlob *ndb_blob;
    if (bitmap_is_set(bitmap, i))
    {
      if ((ndb_blob= ndb_op->getBlobHandle(i)) == NULL ||
          ndb_blob->setActiveHook(g_get_ndb_blobs_value, this) != 0)
        DBUG_RETURN(1);
      m_blob_expected_count++;
    }
    else
      ndb_blob= NULL;

    m_value[i].blob= ndb_blob;
  }

  DBUG_RETURN(0);
}

int
ha_ndbcluster::set_blob_values(NdbOperation *ndb_op, my_ptrdiff_t row_offset,
                               const MY_BITMAP *bitmap, uint *set_count)
{
  uint field_no;
  uint *blob_index, *blob_index_end;
  int res= 0;
  DBUG_ENTER("ha_ndbcluster::set_blob_values");

  *set_count= 0;

  if (table_share->blob_fields == 0)
    DBUG_RETURN(0);

  blob_index= table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    field_no= *blob_index;
    /* A NULL bitmap sets all blobs. */
    if (bitmap && !bitmap_is_set(bitmap, field_no))
      continue;
    Field *field= table->field[field_no];

    NdbBlob *ndb_blob= ndb_op->getBlobHandle(field_no);
    if (ndb_blob == NULL)
      DBUG_RETURN(1);
    if (field->is_null_in_record_with_offset(row_offset))
    {
      if (ndb_blob->setNull() != 0)
        DBUG_RETURN(1);
    }
    else
    {
      Field_blob *field_blob= (Field_blob *)field;

      // Get length and pointer to data
      const byte* field_ptr= field->ptr + row_offset;
      uint32 blob_len= field_blob->get_length(field_ptr);
      char* blob_ptr= NULL;
      field_blob->get_ptr(&blob_ptr);

      // Looks like NULL ptr signals length 0 blob
      if (blob_ptr == NULL) {
        DBUG_ASSERT(blob_len == 0);
        blob_ptr= (char*)"";
      }

      DBUG_PRINT("value", ("set blob ptr: 0x%lx  len: %u",
                           (long) blob_ptr, blob_len));
      DBUG_DUMP("value", (char*)blob_ptr, min(blob_len, 26));

      // No callback needed to write value
      res= ndb_blob->setValue(blob_ptr, blob_len);
      if (res != 0)
        DBUG_RETURN(1);
    }

    ++(*set_count);
  } while (++blob_index != blob_index_end);

  DBUG_RETURN(res);
}

/*
  This routine is shared by injector.  There is no common blobs buffer
  so the buffer and length are passed by reference.  Injector also
  passes a record pointer diff.
 */
int get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                        byte*& buffer, uint& buffer_size,
                        my_ptrdiff_t ptrdiff)
{
  DBUG_ENTER("get_ndb_blobs_value");

  // Field has no field number so cannot use TABLE blob_field
  // Loop twice, first only counting total buffer size
  for (int loop= 0; loop <= 1; loop++)
  {
    uint32 offset= 0;
    for (uint i= 0; i < table->s->fields; i++)
    {
      Field *field= table->field[i];
      NdbValue value= value_array[i];
      if (! (field->flags & BLOB_FLAG))
        continue;
      if (value.blob == NULL)
      {
        DBUG_PRINT("info",("[%u] skipped", i));
        continue;
      }
      Field_blob *field_blob= (Field_blob *)field;
      NdbBlob *ndb_blob= value.blob;
      int isNull;
      if (ndb_blob->getNull(isNull) != 0)
        ERR_RETURN(ndb_blob->getNdbError());
      if (isNull == 0) {
        Uint64 len64= 0;
        if (ndb_blob->getLength(len64) != 0)
          ERR_RETURN(ndb_blob->getNdbError());
        // Align to Uint64
        uint32 size= len64;
        if (size % 8 != 0)
          size+= 8 - size % 8;
        if (loop == 1)
        {
          char *buf= buffer + offset;
          uint32 len= 0xffffffff;  // Max uint32
          if (ndb_blob->readData(buf, len) != 0)
            ERR_RETURN(ndb_blob->getNdbError());
          DBUG_PRINT("info", ("[%u] offset: %u  buf: 0x%lx  len=%u  [ptrdiff=%d]",
                              i, offset, (long) buf, len, (int)ptrdiff));
          DBUG_ASSERT(len == len64);
          // Ugly hack assumes only ptr needs to be changed
          field_blob->set_ptr_offset(ptrdiff, len, buf);
        }
        offset+= size;
      }
      else if (loop == 1) // undefined or null
      {
        // have to set length even in this case
        char *buf= buffer + offset; // or maybe NULL
        uint32 len= 0;
        field_blob->set_ptr_offset(ptrdiff, len, buf);
        DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
      }
    }
    if (loop == 0 && offset > buffer_size)
    {
      my_free(buffer, MYF(MY_ALLOW_ZERO_PTR));
      buffer_size= 0;
      DBUG_PRINT("info", ("allocate blobs buffer size %u", offset));
      buffer= my_malloc(offset, MYF(MY_WME));
      if (buffer == NULL)
      {
        sql_print_error("ha_ndbcluster::get_ndb_blobs_value: "
                        "my_malloc(%u) failed", offset);
        DBUG_RETURN(-1);
      }
      buffer_size= offset;
    }
  }
  DBUG_RETURN(0);
}


/*
  Check if any set or get of blob value in current query.
*/

bool ha_ndbcluster::uses_blob_value(const MY_BITMAP *bitmap)
{
  uint *blob_index, *blob_index_end;
  if (table_share->blob_fields == 0)
    return FALSE;

  blob_index=     table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    if (bitmap_is_set(bitmap, table->field[*blob_index]->field_index))
      return TRUE;
  } while (++blob_index != blob_index_end);
  return FALSE;
}


/*
  Get metadata for this table from NDB 

  IMPLEMENTATION
    - check that frm-file on disk is equal to frm-file
      of table accessed in NDB

  RETURN
    0    ok
    -2   Meta data has changed; Re-read data and try again
*/

int cmp_frm(const NDBTAB *ndbtab, const void *pack_data,
            uint pack_length)
{
  DBUG_ENTER("cmp_frm");
  /*
    Compare FrmData in NDB with frm file from disk.
  */
  if ((pack_length != ndbtab->getFrmLength()) || 
      (memcmp(pack_data, ndbtab->getFrmData(), pack_length)))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

int ha_ndbcluster::get_metadata(const char *path)
{
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *tab;
  int error;
  DBUG_ENTER("get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s, path: %s", m_tabname, path));

  DBUG_ASSERT(m_table == NULL);
  DBUG_ASSERT(m_table_info == NULL);

  const void *data= NULL, *pack_data= NULL;
  uint length, pack_length;

  /*
    Compare FrmData in NDB with frm file from disk.
  */
  error= 0;
  if (readfrm(path, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
    
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

  if (get_ndb_share_state(m_share) != NSS_ALTERED 
      && cmp_frm(tab, pack_data, pack_length))
  {
    DBUG_PRINT("error", 
               ("metadata, pack_length: %d  getFrmLength: %d  memcmp: %d",
                pack_length, tab->getFrmLength(),
                memcmp(pack_data, tab->getFrmData(), pack_length)));
    DBUG_DUMP("pack_data", (char*)pack_data, pack_length);
    DBUG_DUMP("frm", (char*)tab->getFrmData(), tab->getFrmLength());
    error= HA_ERR_TABLE_DEF_CHANGED;
  }
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));

  if (error)
    goto err;

  DBUG_PRINT("info", ("fetched table %s", tab->getName()));
  m_table= tab;

  if (bitmap_init(&m_bitmap, m_bitmap_buf, table_share->fields, 0) ||
      bitmap_init(&m_pk_bitmap, m_pk_bitmap_buf, table_share->fields, 0))
  {
    error= HA_ERR_OUT_OF_MEM;
    goto err;
  }
  if (table_share->primary_key != MAX_KEY)
  {
    KEY *pk_info= table->key_info + table_share->primary_key;
    uint i;
    for (i= 0; i < pk_info->key_parts; i++)
    {
      KEY_PART_INFO *kp= &pk_info->key_part[i];
      bitmap_set_bit(&m_pk_bitmap, kp->fieldnr - 1);
    }
  }
  else
  {
    /* Hidden primary key. */
    uint field_no= table_share->fields;
    ((uchar *)m_pk_bitmap_buf)[field_no>>3]|= (1 << (field_no & 7));

    if ((error= add_hidden_pk_ndb_record(dict)) != 0)
      goto err;
  }

  if ((error= add_table_ndb_record(dict)) != 0)
    goto err;

  /*
    Approx. write size in bytes over transporter
  */
  m_bytes_per_write= 12 + tab->getRowSizeInBytes() + 4 * tab->getNoOfColumns();
  if ((error= open_indexes(ndb, table, FALSE)) == 0)
  {
    ndbtab_g.release();
    DBUG_RETURN(0);
  }

err:
  ndbtab_g.invalidate();
  m_table= NULL;
  DBUG_RETURN(error);
}

static int fix_unique_index_attr_order(NDB_INDEX_DATA &data,
                                       const NDBINDEX *index,
                                       KEY *key_info)
{
  DBUG_ENTER("fix_unique_index_attr_order");
  unsigned sz= index->getNoOfIndexColumns();

  if (data.unique_index_attrid_map)
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  data.unique_index_attrid_map= (uchar*)my_malloc(sz,MYF(MY_WME));
  if (data.unique_index_attrid_map == 0)
  {
    sql_print_error("fix_unique_index_attr_order: my_malloc(%u) failure",
                    (unsigned int)sz);
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ASSERT(key_info->key_parts == sz);
  for (unsigned i= 0; key_part != end; key_part++, i++) 
  {
    const char *field_name= key_part->field->field_name;
#ifndef DBUG_OFF
   data.unique_index_attrid_map[i]= 255;
#endif
    for (unsigned j= 0; j < sz; j++)
    {
      const NDBCOL *c= index->getColumn(j);
      if (strcmp(field_name, c->getName()) == 0)
      {
        data.unique_index_attrid_map[i]= j;
        break;
      }
    }
    DBUG_ASSERT(data.unique_index_attrid_map[i] != 255);
  }
  DBUG_RETURN(0);
}

/*
  Create all the indexes for a table.
  If any index should fail to be created,
  the error is returned immediately
*/
int ha_ndbcluster::create_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::create_indexes");

  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    error= create_index(index_name, key_info, idx_type, i);
    if (error)
    {
      DBUG_PRINT("error", ("Failed to create index %u", i));
      break;
    }
  }

  DBUG_RETURN(error);
}

static void ndb_init_index(NDB_INDEX_DATA &data)
{
  data.type= UNDEFINED_INDEX;
  data.status= UNDEFINED;
  data.unique_index= NULL;
  data.index= NULL;
  data.unique_index_attrid_map= NULL;
  data.index_stat=NULL;
  data.index_stat_cache_entries=0;
  data.index_stat_update_freq=0;
  data.index_stat_query_count=0;
  data.ndb_record_key= NULL;
  data.ndb_record_row= NULL;
  data.ndb_unique_record_key= NULL;
  data.ndb_unique_record_row= NULL;
}

static void ndb_clear_index(NDBDICT *dict, NDB_INDEX_DATA &data)
{
  if (data.unique_index_attrid_map)
  {
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  }
  if (data.index_stat)
  {
    delete data.index_stat;
  }
  if (data.ndb_unique_record_key)
    dict->releaseRecord(data.ndb_unique_record_key);
  if (data.ndb_unique_record_row)
    dict->releaseRecord(data.ndb_unique_record_row);
  if (data.ndb_record_key)
    dict->releaseRecord(data.ndb_record_key);
  if (data.ndb_record_row)
    dict->releaseRecord(data.ndb_record_row);
  ndb_init_index(data);
}

/*
  Associate a direct reference to an index handle
  with an index (for faster access)
 */
int ha_ndbcluster::add_index_handle(THD *thd, NDBDICT *dict, KEY *key_info,
                                    const char *index_name, uint index_no)
{
  int error= 0;

  NDB_INDEX_TYPE idx_type= get_index_type_from_table(index_no);
  m_index[index_no].type= idx_type;
  DBUG_ENTER("ha_ndbcluster::add_index_handle");
  DBUG_PRINT("enter", ("table %s", m_tabname));

  if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX)
  {
    DBUG_PRINT("info", ("Get handle to index %s", index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(index_name, *m_table);
      if (!index)
        ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("index: 0x%lx  id: %d  version: %d.%d  status: %d",
                          (long) index,
                          index->getObjectId(),
                          index->getObjectVersion() & 0xFFFFFF,
                          index->getObjectVersion() >> 24,
                          index->getObjectStatus()));
      DBUG_ASSERT(index->getObjectStatus() ==
                  NdbDictionary::Object::Retrieved);
      break;
    } while (1);
    m_index[index_no].index= index;
    // ordered index - add stats
    NDB_INDEX_DATA& d=m_index[index_no];
    delete d.index_stat;
    d.index_stat=NULL;
    if (thd->variables.ndb_index_stat_enable)
    {
      d.index_stat=new NdbIndexStat(index);
      d.index_stat_cache_entries=thd->variables.ndb_index_stat_cache_entries;
      d.index_stat_update_freq=thd->variables.ndb_index_stat_update_freq;
      d.index_stat_query_count=0;
      d.index_stat->alloc_cache(d.index_stat_cache_entries);
      DBUG_PRINT("info", ("index %s stat=on cache_entries=%u update_freq=%u",
                          index->getName(),
                          d.index_stat_cache_entries,
                          d.index_stat_update_freq));
    } else
    {
      DBUG_PRINT("info", ("index %s stat=off", index->getName()));
    }
  }
  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    char unique_index_name[FN_LEN];
    static const char* unique_suffix= "$unique";
    m_has_unique_index= TRUE;
    strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(unique_index_name, *m_table);
      if (!index)
        ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("index: 0x%lx  id: %d  version: %d.%d  status: %d",
                          (long) index,
                          index->getObjectId(),
                          index->getObjectVersion() & 0xFFFFFF,
                          index->getObjectVersion() >> 24,
                          index->getObjectStatus()));
      DBUG_ASSERT(index->getObjectStatus() ==
                  NdbDictionary::Object::Retrieved);
      break;
    } while (1);
    m_index[index_no].unique_index= index;
    error= fix_unique_index_attr_order(m_index[index_no], index, key_info);
  }

  if (!error)
    error= add_index_ndb_record(dict, key_info, index_no);

  if (!error)
    m_index[index_no].status= ACTIVE;
  
  DBUG_RETURN(error);
}

/*
  We use this function to convert null bit masks, as found in class Field,
  to bit numbers, as used in NdbRecord.
*/
static uint
null_bit_mask_to_bit_number(uchar bit_mask)
{
  switch (bit_mask)
  {
    case  0x1: return 0;
    case  0x2: return 1;
    case  0x4: return 2;
    case  0x8: return 3;
    case 0x10: return 4;
    case 0x20: return 5;
    case 0x40: return 6;
    case 0x80: return 7;
    default:
      DBUG_ASSERT(false);
      return 0;
  }
}

static void
ndb_set_record_specification(uint field_no,
                             NdbDictionary::RecordSpecification *spec,
                             const TABLE *table,
                             const NdbDictionary::Table *ndb_table)
{
  spec->column= ndb_table->getColumn(field_no);
  spec->offset= table->field[field_no]->ptr - table->record[0];
  if (table->field[field_no]->null_ptr)
  {
    spec->nullbit_byte_offset=
      (char *)table->field[field_no]->null_ptr - table->record[0];
    spec->nullbit_bit_in_byte=
      null_bit_mask_to_bit_number(table->field[field_no]->null_bit);
  }
  else if (table->field[field_no]->type() == MYSQL_TYPE_BIT)
  {
    /* We need to store the position of the overflow bits. */
    const Field_bit* field_bit= static_cast<Field_bit*>(table->field[field_no]);
    spec->nullbit_byte_offset=
      (char *)field_bit->bit_ptr - table->record[0];
    spec->nullbit_bit_in_byte= field_bit->bit_ofs;
  }
  else
  {
    spec->nullbit_byte_offset= 0;
    spec->nullbit_bit_in_byte= 0;
  }
}

int
ha_ndbcluster::add_table_ndb_record(NDBDICT *dict)
{
  DBUG_ENTER("ha_ndbcluster::add_table_ndb_record()");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;
  uint i;

  for (i= 0; i < table_share->fields; i++)
  {
    ndb_set_record_specification(i, &spec[i], table, m_table);
  }

  uint32 size= 0;
  if (table_share->primary_key == MAX_KEY)
  {
    /* Access to the hidden primary key. */
    spec[i].column= m_table->getColumn(i);
    spec[i].offset= offset_hidden_key();
    spec[i].nullbit_byte_offset= 0;
    spec[i].nullbit_bit_in_byte= 0;
    size+= NDB_HIDDEN_PRIMARY_KEY_LENGTH;
    i++;
  }
  if (m_use_partition_function)
  {
    /* Access to the hidden partition function column. */
    spec[i].column= m_table->getColumn(i);
    spec[i].offset= offset_user_partition_function();
    spec[i].nullbit_byte_offset= 0;
    spec[i].nullbit_bit_in_byte= 0;
    size+= 4;
    i++;
  }

  rec= dict->createRecord(m_table, spec, i, sizeof(spec[0]),
                          NdbDictionary::RecMysqldBitfield);
  if (! rec)
    ERR_RETURN(dict->getNdbError());
  m_ndb_record= rec;

  /*
    We need a different NdbRecord for reading the FRAGMENT pseudo-column,
    as pseudo-columns cannot be enabled/disabled with bitmask.
  */
  if (m_use_partition_function && table_share->primary_key == MAX_KEY)
  {
    spec[i].column= NdbDictionary::Column::FRAGMENT;
    spec[i].offset= offset_user_partition_fragment();
    spec[i].nullbit_byte_offset= 0;
    spec[i].nullbit_bit_in_byte= 0;
    size+= 4;
    i++;

    rec= dict->createRecord(m_table, spec, i, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_ndb_record_fragment= rec;
  }
  else
    m_ndb_record_fragment= NULL;

  m_extra_reclength= size;

  rec= ndb_get_table_statistics_ndbrecord(dict, m_table);
  if (! rec)
    ERR_RETURN(dict->getNdbError());
  m_ndb_statistics_record= rec;

  DBUG_RETURN(0);
}

/* Create NdbRecord for setting hidden primary key from Uint64. */
int
ha_ndbcluster::add_hidden_pk_ndb_record(NDBDICT *dict)
{
  DBUG_ENTER("ha_ndbcluster::add_hidden_pk_ndb_record");
  NdbDictionary::RecordSpecification spec[1];
  NdbRecord *rec;

  spec[0].column= m_table->getColumn(table_share->fields);
  spec[0].offset= 0;
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;

  rec= dict->createRecord(m_table, spec, 1, sizeof(spec[0]));
  if (! rec)
    ERR_RETURN(dict->getNdbError());
  m_ndb_hidden_key_record= rec;

  DBUG_RETURN(0);
}

int
ha_ndbcluster::add_index_ndb_record(NDBDICT *dict, KEY *key_info, uint index_no)
{
  DBUG_ENTER("ha_ndbcluster::add_index_ndb_record");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;

  Uint32 offset= 0;
  for (uint i= 0; i < key_info->key_parts; i++)
  {
    KEY_PART_INFO *kp= &key_info->key_part[i];

    spec[i].column= m_table->getColumn(kp->fieldnr - 1);
    if (! spec[i].column)
      ERR_RETURN(dict->getNdbError());
    if (kp->null_bit)
    {
      /* Nullable column. */
      spec[i].offset= offset + 1;           // First byte is NULL flag
      spec[i].nullbit_byte_offset= offset;
      spec[i].nullbit_bit_in_byte= 0;
    }
    else
    {
      /* Not nullable column. */
      spec[i].offset= offset;
      spec[i].nullbit_byte_offset= 0;
      spec[i].nullbit_bit_in_byte= 0;
    }
    offset+= kp->store_length;
  }

  if (m_index[index_no].index)
  {
    /*
      Enable MysqldShrinkVarchar flag so that the two-byte length used by
      mysqld for short varchar keys is correctly converted into a one-byte
      length used by Ndb kernel.
    */
    rec= dict->createRecord(m_index[index_no].index, m_table,
                            spec, key_info->key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_record_key= rec;
  }
  else
    m_index[index_no].ndb_record_key= NULL;

  if (m_index[index_no].unique_index)
  {
    rec= dict->createRecord(m_index[index_no].unique_index, m_table,
                            spec, key_info->key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key= rec;
  }
  else if (index_no == table_share->primary_key)
  {
    /* The primary key is special, there is no explicit NDB index associated. */
    rec= dict->createRecord(m_table,
                            spec, key_info->key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key= rec;
  }
  else
    m_index[index_no].ndb_unique_record_key= NULL;

  /* Now do the same, but this time with offsets from Field, for row access. */
  for (uint i= 0; i < key_info->key_parts; i++)
  {
    const KEY_PART_INFO *kp= &key_info->key_part[i];

    spec[i].offset= kp->offset;
    if (kp->null_bit)
    {
      /* Nullable column. */
      spec[i].nullbit_byte_offset= kp->null_offset;
      spec[i].nullbit_bit_in_byte= null_bit_mask_to_bit_number(kp->null_bit);
    }
    else
    {
      /* Not nullable column. */
      spec[i].nullbit_byte_offset= 0;
      spec[i].nullbit_bit_in_byte= 0;
    }
  }

  if (m_index[index_no].unique_index)
  {
    rec= dict->createRecord(m_index[index_no].unique_index, m_table,
                            spec, key_info->key_parts, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row= rec;
  }
  else if (index_no == table_share->primary_key)
  {
    rec= dict->createRecord(m_table,
                            spec, key_info->key_parts, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row= rec;
  }
  else
    m_index[index_no].ndb_unique_record_row= NULL;

  /*
    Now create ordered index ndb record for row access with all columns.
    We need this to properly sort rows retrieved from ordered index scan.
  */
  if (m_index[index_no].index)
  {
    uint i;
    for (i= 0; i < table_share->fields; i++)
    {
      ndb_set_record_specification(i, &spec[i], table, m_table);
    }

    if (table_share->primary_key == MAX_KEY)
    {
      /* Access to the hidden primary key. */
      spec[i].column= m_table->getColumn(i);
      spec[i].offset= offset_hidden_key();
      spec[i].nullbit_byte_offset= 0;
      spec[i].nullbit_bit_in_byte= 0;
      i++;

      if (m_use_partition_function)
      {
        spec[i].column= NdbDictionary::Column::FRAGMENT;
        spec[i].offset= offset_user_partition_fragment();
        spec[i].nullbit_byte_offset= 0;
        spec[i].nullbit_bit_in_byte= 0;
        i++;
      }
    }

    rec= dict->createRecord(m_index[index_no].index, m_table,
                            spec, i, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_record_row= rec;
  }
  else
    m_index[index_no].ndb_record_row= NULL;

  DBUG_RETURN(0);
}

/*
  Associate index handles for each index of a table
*/
int ha_ndbcluster::open_indexes(Ndb *ndb, TABLE *tab, bool ignore_error)
{
  uint i;
  int error= 0;
  THD *thd=current_thd;
  NDBDICT *dict= ndb->getDictionary();
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::open_indexes");
  m_has_unique_index= FALSE;
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    if ((error= add_index_handle(thd, dict, key_info, *key_name, i)))
      if (ignore_error)
        m_index[i].index= m_index[i].unique_index= NULL;
      else
        break;
    m_index[i].null_in_unique_index= FALSE;
    if (check_index_fields_not_null(key_info))
      m_index[i].null_in_unique_index= TRUE;
  }

  if (error && !ignore_error)
  {
    while (i > 0)
    {
      i--;
      if (m_index[i].index)
      {
         dict->removeIndexGlobal(*m_index[i].index, 1);
         m_index[i].index= NULL;
      }
      if (m_index[i].unique_index)
      {
         dict->removeIndexGlobal(*m_index[i].unique_index, 1);
         m_index[i].unique_index= NULL;
      }
    }
  }

  DBUG_ASSERT(error == 0 || error == 4243);

  DBUG_RETURN(error);
}

/*
  Renumber indexes in index list by shifting out
  indexes that are to be dropped
 */
void ha_ndbcluster::renumber_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::renumber_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED) 
    {
      DBUG_PRINT("info", ("Shifting index %s(%i) out of the list", 
                          index_name, i));
      NDB_INDEX_DATA tmp;
      uint j= i + 1;
      // Shift index out of list
      while(j != MAX_KEY && m_index[j].status != UNDEFINED)
      {
        tmp=  m_index[j - 1];
        m_index[j - 1]= m_index[j];
        m_index[j]= tmp;
        j++;
      }
    }
  }

  DBUG_VOID_RETURN;
}

/*
  Drop all indexes that are marked for deletion
*/
int ha_ndbcluster::drop_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::drop_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      const NdbDictionary::Index *unique_index= m_index[i].unique_index;
      
      if (index)
      {
        index_name= index->getName();
        DBUG_PRINT("info", ("Dropping index %u: %s", i, index_name));  
        // Drop ordered index from ndb
        error= dict->dropIndexGlobal(*index);
        if (!error)
        {
          dict->removeIndexGlobal(*index, 1);
          m_index[i].index= NULL;
        }
      }
      if (!error && unique_index)
      {
        index_name= unique_index->getName();
        DBUG_PRINT("info", ("Dropping unique index %u: %s", i, index_name));
        // Drop unique index from ndb
        error= dict->dropIndexGlobal(*unique_index);
        if (!error)
        {
          dict->removeIndexGlobal(*unique_index, 1);
          m_index[i].unique_index= NULL;
        }
      }
      if (error)
        DBUG_RETURN(error);
      ndb_clear_index(dict, m_index[i]);
      continue;
    }
  }
  
  DBUG_RETURN(error);
}

/*
  Decode the type of an index from information 
  provided in table object
*/
NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_table(uint inx) const
{
  return get_index_type_from_key(inx, table_share->key_info,
                                 inx == table_share->primary_key);
}

NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_key(uint inx,
                                                      KEY *key_info,
                                                      bool primary) const
{
  bool is_hash_index=  (key_info[inx].algorithm == 
                        HA_KEY_ALG_HASH);
  if (primary)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;
  
  return ((key_info[inx].flags & HA_NOSAME) ? 
          (is_hash_index ? UNIQUE_INDEX : UNIQUE_ORDERED_INDEX) :
          ORDERED_INDEX);
} 

bool ha_ndbcluster::check_index_fields_not_null(KEY* key_info)
{
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("ha_ndbcluster::check_index_fields_not_null");
  
  for (; key_part != end; key_part++) 
    {
      Field* field= key_part->field;
      if (field->maybe_null())
	DBUG_RETURN(TRUE);
    }
  
  DBUG_RETURN(FALSE);
}

void ha_ndbcluster::release_metadata(THD *thd, Ndb *ndb)
{
  uint i;

  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  NDBDICT *dict= ndb->getDictionary();
  int invalidate_indexes= 0;
  if (thd && thd->lex && thd->lex->sql_command == SQLCOM_FLUSH)
  {
    invalidate_indexes = 1;
  }
  if (m_table != NULL)
  {
    if (m_ndb_record != NULL)
    {
      dict->releaseRecord(m_ndb_record);
      m_ndb_record= NULL;
    }
    if (m_ndb_record_fragment != NULL)
    {
      dict->releaseRecord(m_ndb_record_fragment);
      m_ndb_record_fragment= NULL;
    }
    if (m_ndb_hidden_key_record != NULL)
    {
      dict->releaseRecord(m_ndb_hidden_key_record);
      m_ndb_hidden_key_record= NULL;
    }
    if (m_ndb_statistics_record != NULL)
    {
      dict->releaseRecord(m_ndb_statistics_record);
      m_ndb_statistics_record= NULL;
    }
    if (m_table->getObjectStatus() == NdbDictionary::Object::Invalid)
      invalidate_indexes= 1;
    dict->removeTableGlobal(*m_table, invalidate_indexes);
  }
  // TODO investigate
  DBUG_ASSERT(m_table_info == NULL);
  m_table_info= NULL;

  // Release index list 
  for (i= 0; i < MAX_KEY; i++)
  {
    if (m_index[i].unique_index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].unique_index, invalidate_indexes);
    }
    if (m_index[i].index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].index, invalidate_indexes);
    }
    ndb_clear_index(dict, m_index[i]);
  }

  m_table= NULL;
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::get_ndb_lock_type(enum thr_lock_type type,
                                     const MY_BITMAP *column_bitmap)
{
  if (type >= TL_WRITE_ALLOW_WRITE)
    return NdbOperation::LM_Exclusive;
  if (type ==  TL_READ_WITH_SHARED_LOCKS ||
      (column_bitmap != NULL && uses_blob_value(column_bitmap)))
    return NdbOperation::LM_Read;
  return NdbOperation::LM_CommittedRead;
}

static const ulong index_type_flags[]=
{
  /* UNDEFINED_INDEX */
  0,                         

  /* PRIMARY_KEY_INDEX */
  HA_ONLY_WHOLE_INDEX, 

  /* PRIMARY_KEY_ORDERED_INDEX */
  /* 
     Enable HA_KEYREAD_ONLY when "sorted" indexes are supported, 
     thus ORDERD BY clauses can be optimized by reading directly 
     through the index.
  */
  // HA_KEYREAD_ONLY | 
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* UNIQUE_INDEX */
  HA_ONLY_WHOLE_INDEX,

  /* UNIQUE_ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER
};

static const int index_flags_size= sizeof(index_type_flags)/sizeof(ulong);

inline NDB_INDEX_TYPE ha_ndbcluster::get_index_type(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_index[idx_no].type;
}

inline bool ha_ndbcluster::has_null_in_unique_index(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_index[idx_no].null_in_unique_index;
}


/*
  Get the flags for an index

  RETURN
    flags depending on the type of the index.
*/

inline ulong ha_ndbcluster::index_flags(uint idx_no, uint part,
                                        bool all_parts) const 
{ 
  DBUG_ENTER("ha_ndbcluster::index_flags");
  DBUG_PRINT("enter", ("idx_no: %u", idx_no));
  DBUG_ASSERT(get_index_type_from_table(idx_no) < index_flags_size);
  DBUG_RETURN(index_type_flags[get_index_type_from_table(idx_no)] | 
              HA_KEY_SCAN_NOT_ROR);
}

static void shrink_varchar(Field* field, const byte* & ptr, char* buf)
{
  if (field->type() == MYSQL_TYPE_VARCHAR && ptr != NULL) {
    Field_varstring* f= (Field_varstring*)field;
    if (f->length_bytes == 1) {
      uint pack_len= field->pack_length();
      DBUG_ASSERT(1 <= pack_len && pack_len <= 256);
      if (ptr[1] == 0) {
        buf[0]= ptr[0];
      } else {
        DBUG_ASSERT(FALSE);
        buf[0]= 255;
      }
      memmove(buf + 1, ptr + 2, pack_len - 1);
      ptr= buf;
    }
  }
}


/*
  Read one record from NDB using primary key
*/

int ha_ndbcluster::pk_read(const byte *key, uint key_len, byte *buf,
                           uint32 part_id)
{
  NdbConnection *trans= m_active_trans;
  NdbOperation *op;
  char *row;
  int res;
  DBUG_ENTER("pk_read");
  DBUG_PRINT("enter", ("key_len: %u read_set=%x",
                       key_len, table->read_set->bitmap[0]));
  DBUG_DUMP("key", (char*)key, key_len);

  if (table_share->primary_key == MAX_KEY)
  {
    row= get_row_buffer();
    if (!row)
      DBUG_RETURN(ER_OUTOFMEMORY);
  }
  else
    row= buf;

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  if (!(op= pk_unique_index_read_key(table->s->primary_key, key, row, lm)))
    ERR_RETURN(trans->getNdbError());
  
  if (m_use_partition_function)
    op->setPartitionId(part_id);

  if ((res = execute_no_commit_ie(this,trans,FALSE)) != 0 ||
      op->getNdbError().code) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  if (table_share->primary_key == MAX_KEY)
  {
    memcpy(buf, row, table_share->reclength);
    m_ref= get_hidden_key(row);
    if (m_use_partition_function)
      m_part_id= get_partition_fragment(row);
  }

  table->status= 0;     
  DBUG_RETURN(0);
}

/*
  Read one complementing record from NDB using primary key from old_data
  or hidden key
*/

int ha_ndbcluster::complemented_read(const byte *old_data, byte *new_data,
                                     uint32 old_part_id)
{
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  DBUG_ENTER("complemented_read");

  if (bitmap_is_set_all(table->read_set))
  {
    // We have allready retrieved all fields, nothing to complement
    DBUG_RETURN(0);
  }

  const NdbRecord *key_rec;
  const char *key_row;
  if (table_share->primary_key != MAX_KEY)
  {
    key_rec= m_index[table->s->primary_key].ndb_unique_record_row;
    key_row= old_data;
  }
  else
  {
    /* Hidden primary key, previously read into m_ref. */
    key_rec= m_ndb_hidden_key_record;
    key_row= (const char *)(&m_ref);
  }

  /*
    Use mask only with columns that are not in write_set, not in
    read_set, and not part of the primary key.
  */
  bitmap_copy(&m_bitmap, table->read_set);
  bitmap_union(&m_bitmap, table->write_set);
  bitmap_invert(&m_bitmap);
  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, &m_bitmap);
  if (!(op= trans->readTuple(key_rec, key_row, m_ndb_record, new_data,
                             lm, (const unsigned char *)(m_bitmap.bitmap))))
    ERR_RETURN(trans->getNdbError());

  if (table_share->blob_fields > 0)
  {
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    int res= get_blob_values(op, new_data, &m_bitmap);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (res != 0)
      ERR_RETURN(op->getNdbError());
  }

  if (m_use_partition_function)
    op->setPartitionId(old_part_id);
  
  if (execute_no_commit(this,trans,FALSE) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  DBUG_RETURN(0);
}

/*
 * Check that all operations between first and last all
 * have gotten the errcode
 * If checking for HA_ERR_KEY_NOT_FOUND then update m_dupkey
 * for all succeeding operations
 */
bool ha_ndbcluster::check_all_operations_for_error(NdbTransaction *trans,
                                                   const NdbOperation *first,
                                                   const NdbOperation *last,
                                                   uint errcode)
{
  const NdbOperation *op= first;
  DBUG_ENTER("ha_ndbcluster::check_all_operations_for_error");

  while(op)
  {
    NdbError err= op->getNdbError();
    if (err.status != NdbError::Success)
    {
      if (ndb_to_mysql_error(&err) != (int) errcode)
        DBUG_RETURN(FALSE);
      if (op == last) break;
      op= trans->getNextCompletedOperation(op);
    }
    else
    {
      // We found a duplicate
      if (op->getType() == NdbOperation::UniqueIndexAccess)
      {
        if (errcode == HA_ERR_KEY_NOT_FOUND)
        {
          NdbIndexOperation *iop= (NdbIndexOperation *) op;
          const NDBINDEX *index= iop->getIndex();
          // Find the key_no of the index
          for(uint i= 0; i<table->s->keys; i++)
          {
            if (m_index[i].unique_index == index)
            {
              m_dupkey= i;
              break;
            }
          }
        }
      }
      else
      {
        // Must have been primary key access
        DBUG_ASSERT(op->getType() == NdbOperation::PrimaryKeyAccess);
        if (errcode == HA_ERR_KEY_NOT_FOUND)
          m_dupkey= table->s->primary_key;
      }
      DBUG_RETURN(FALSE);      
    }
  }
  DBUG_RETURN(TRUE);
}


/**
 * Check if record contains any null valued columns that are part of a key
 */
static
int
check_null_in_record(const KEY* key_info, const byte *record)
{
  KEY_PART_INFO *curr_part, *end_part;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->key_parts;

  while (curr_part != end_part)
  {
    if (curr_part->null_bit &&
        (record[curr_part->null_offset] & curr_part->null_bit))
      return 1;
    curr_part++;
  }
  return 0;
  /*
    We could instead pre-compute a bitmask in table_share with one bit for
    every null-bit in the key, and so check this just by OR'ing the bitmask
    with the null bitmap in the record.
    But not sure it's worth it.
  */
}

/* Empty mask and dummy row, for reading no attributes using NdbRecord. */
/* Mask will be initialized to all zeros by linker. */
static unsigned char empty_mask[(NDB_MAX_ATTRIBUTES_IN_TABLE+7)/8];
static char dummy_row[1];

/*
 * Peek to check if any rows already exist with conflicting
 * primary key or unique index values
*/

int ha_ndbcluster::peek_indexed_rows(const byte *record, bool check_pk)
{
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  const NdbOperation *first, *last;
  uint i;
  int res;
  DBUG_ENTER("peek_indexed_rows");

  NdbOperation::LockMode lm=
      (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, NULL);
  first= NULL;
  if (check_pk && table->s->primary_key != MAX_KEY)
  {
    /*
     * Fetch any row with colliding primary key
     */
    const NdbRecord *key_rec=
      m_index[table->s->primary_key].ndb_unique_record_row;
    if (!(op= trans->readTuple(key_rec, (const char *)record,
                               key_rec, dummy_row, lm, empty_mask)))
      ERR_RETURN(trans->getNdbError());
    
    first= op;

    if (m_use_partition_function)
    {
      uint32 part_id;
      int error;
      longlong func_value;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
      error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
      dbug_tmp_restore_column_map(table->read_set, old_map);
      if (error)
      {
        m_part_info->err_value= func_value;
        DBUG_RETURN(error);
      }
      op->setPartitionId(part_id);
    }
  }
  /*
   * Fetch any rows with colliding unique indexes
   */
  KEY* key_info;
  for (i= 0, key_info= table->key_info; i < table->s->keys; i++, key_info++)
  {
    if (i != table->s->primary_key &&
        key_info->flags & HA_NOSAME)
    {
      /*
        A unique index is defined on table.
        We cannot look up a NULL field value in a unique index. But since
        keys with NULLs are not indexed, such rows cannot conflict anyway, so
        we just skip the index in this case.
      */
      if (check_null_in_record(key_info, record))
      {
        DBUG_PRINT("info", ("skipping check for key with NULL"));
        continue;
      }

      NdbOperation *iop;
      const NdbRecord *key_rec= m_index[i].ndb_unique_record_row;
      if (!(iop= trans->readTuple(key_rec, record, key_rec, dummy_row,
                                  lm, empty_mask)))
        ERR_RETURN(trans->getNdbError());

      if (!first)
        first= iop;
    }
  }
  last= trans->getLastDefinedOperation();
  if (first)
    res= execute_no_commit_ie(this,trans,FALSE);
  else
  {
    // Table has no keys
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
  }
  if (check_all_operations_for_error(trans, first, last, 
                                     HA_ERR_KEY_NOT_FOUND))
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  } 
  else
  {
    DBUG_PRINT("info", ("m_dupkey %d", m_dupkey));
  }
  DBUG_RETURN(0);
}


/*
  Read one record from NDB using unique secondary index
*/

int ha_ndbcluster::unique_index_read(const byte *key,
                                     uint key_len, byte *buf)
{
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  char *row;
  DBUG_ENTER("ha_ndbcluster::unique_index_read");
  DBUG_PRINT("enter", ("key_len: %u, index: %u", key_len, active_index));
  DBUG_DUMP("key", (char*)key, key_len);
  
  if (table_share->primary_key == MAX_KEY)
  {
    row= get_row_buffer();
    if (!row)
      DBUG_RETURN(ER_OUTOFMEMORY);
  }
  else
    row= buf;

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  if (!(op= pk_unique_index_read_key(active_index, key, row, lm)))
    ERR_RETURN(trans->getNdbError());
  
  if (execute_no_commit_ie(this,trans,FALSE) != 0 ||
      op->getNdbError().code) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  if (table_share->primary_key == MAX_KEY)
  {
    memcpy(buf, row, table_share->reclength);
    m_ref= get_hidden_key(row);
    if (m_use_partition_function)
      m_part_id= get_partition_fragment(row);
  }

  table->status= 0;
  DBUG_RETURN(0);
}

int
ha_ndbcluster::scan_handle_lock_tuple(NdbScanOperation *scanOp,
                                      NdbTransaction *trans)
{
  DBUG_ENTER("ha_ndbcluster::scan_handle_lock_tuple");
  if (m_lock_tuple)
  {
    /*
      Lock level m_lock.type either TL_WRITE_ALLOW_WRITE
      (SELECT FOR UPDATE) or TL_READ_WITH_SHARED_LOCKS (SELECT
      LOCK WITH SHARE MODE) and row was not explictly unlocked 
      with unlock_row() call
    */
    NdbOperation *op;
    // Lock row
    DBUG_PRINT("info", ("Keeping lock on scanned row"));
      
    if (!(op= scanOp->lockCurrentTuple(trans, m_ndb_record,
                                       dummy_row, empty_mask)))
    {
      /* purecov: begin inspected */
      m_lock_tuple= FALSE;
      ERR_RETURN(trans->getNdbError());
      /* purecov: end */    
    }
    m_ops_pending++;
  }
  m_lock_tuple= FALSE;
  DBUG_RETURN(0);
}

inline int ha_ndbcluster::fetch_next(NdbScanOperation* cursor)
{
  DBUG_ENTER("fetch_next");
  int local_check;
  int error;
  NdbTransaction *trans= m_active_trans;
  
  if ((error= scan_handle_lock_tuple(cursor, trans)) != 0)
    DBUG_RETURN(error);
  
  bool contact_ndb= m_lock.type < TL_WRITE_ALLOW_WRITE &&
                    m_lock.type != TL_READ_WITH_SHARED_LOCKS;
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    /*
      We can only handle one tuple with blobs at a time.
    */
    if (m_ops_pending && m_blobs_pending)
    {
      if (execute_no_commit(this,trans,FALSE) != 0)
        DBUG_RETURN(ndb_err(trans));
    }
    
    if ((local_check= cursor->nextResult(m_next_row, contact_ndb,
                                         m_force_send)) == 0)
    {
      /*
	Explicitly lock tuple if "select for update" or
	"select lock in share mode"
      */
      m_lock_tuple= (m_lock.type == TL_WRITE_ALLOW_WRITE
		     || 
		     m_lock.type == TL_READ_WITH_SHARED_LOCKS);
      DBUG_RETURN(0);
    } 
    else if (local_check == 1 || local_check == 2)
    {
      // 1: No more records
      // 2: No more cached records
      
      /*
        Before fetching more rows and releasing lock(s),
        all pending update or delete operations should 
        be sent to NDB
      */
      DBUG_PRINT("info", ("ops_pending: %ld", (long) m_ops_pending));    
      if (m_ops_pending)
      {
        if (execute_no_commit(this,trans,FALSE) != 0)
          DBUG_RETURN(-1);
      }
      contact_ndb= (local_check == 2);
    }
    else
    {
      DBUG_RETURN(-1);
    }
  } while (local_check == 2);

  DBUG_RETURN(1);
}

/*
  Get the next record of a started scan. Try to fetch
  it locally from NdbApi cached records if possible, 
  otherwise ask NDB for more.

  NOTE
  If this is a update/delete make sure to not contact 
  NDB before any pending ops have been sent to NDB.

*/

inline int ha_ndbcluster::next_result(byte *buf)
{  
  int res;
  DBUG_ENTER("next_result");
    
  if (!m_active_cursor)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  
  if ((res= fetch_next(m_active_cursor)) == 0)
  {
    DBUG_PRINT("info", ("One more record found"));    
    
    if (table_share->primary_key == MAX_KEY)
    {
      m_ref= get_hidden_key(m_next_row);
      if (m_use_partition_function)
        m_part_id= get_partition_fragment(m_next_row);
    }

    unpack_record(buf, m_next_row);
    table->status= 0;
    DBUG_RETURN(0);
  }
  else if (res == 1)
  {
    // No more records
    table->status= STATUS_NOT_FOUND;
    
    DBUG_PRINT("info", ("No more records"));
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  else
  {
    DBUG_RETURN(ndb_err(m_active_trans));
  }
}

/*
  Do a primary key or unique key index read operation.
  The key value is taken from a buffer in mysqld key format.
*/
NdbOperation *
ha_ndbcluster::pk_unique_index_read_key(uint idx, const byte *key, byte *buf,
                                        NdbOperation::LockMode lm)
{
  NdbOperation *op;
  const NdbRecord *ndb_record= m_ndb_record;
  uchar *mask= (uchar *)(table->read_set->bitmap);
  const NdbRecord *key_rec;
  if (idx != MAX_KEY)
    key_rec= m_index[idx].ndb_unique_record_key;
  else
    key_rec= m_ndb_hidden_key_record;

  /* Initialize the null bitmap, setting unused null bits to 1. */
  memset(buf, 0xff, table->s->null_bytes);

  if (m_use_partition_function || table_share->primary_key == MAX_KEY)
  {
    /*
      We need an extended column mask.
      We may also need to read the hidden primary key and the FRAGMENT
      pseudo-column.
    */
    mask= copy_column_set(table->read_set);
    if (table_share->primary_key == MAX_KEY)
    {
      request_hidden_key(mask);
      if (m_use_partition_function)
        ndb_record= m_ndb_record_fragment;
    }
  }
  op= m_active_trans->readTuple(key_rec, key, ndb_record, buf, lm, mask);

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, buf, table->read_set) != 0)
    return NULL;

  return op;
}

/*
  Set bounds for ordered index scan.
*/

/* ToDo: remove if converting records_in_range() to NdbRecord. */
int ha_ndbcluster::set_bounds(NdbIndexScanOperation *op,
                              uint inx,
                              bool rir,
                              const key_range *keys[2],
                              uint range_no)
{
  const KEY *const key_info= table->key_info + inx;
  const uint key_parts= key_info->key_parts;
  uint key_tot_len[2];
  uint tot_len;
  uint i, j;

  DBUG_ENTER("set_bounds");
  DBUG_PRINT("info", ("key_parts=%d", key_parts));

  for (j= 0; j <= 1; j++)
  {
    const key_range *key= keys[j];
    if (key != NULL)
    {
      // for key->flag see ha_rkey_function
      DBUG_PRINT("info", ("key %d length=%d flag=%d",
                          j, key->length, key->flag));
      key_tot_len[j]= key->length;
    }
    else
    {
      DBUG_PRINT("info", ("key %d not present", j));
      key_tot_len[j]= 0;
    }
  }
  tot_len= 0;

  for (i= 0; i < key_parts; i++)
  {
    KEY_PART_INFO *key_part= &key_info->key_part[i];
    Field *field= key_part->field;
#ifndef DBUG_OFF
    uint part_len= key_part->length;
#endif
    uint part_store_len= key_part->store_length;
    // Info about each key part
    struct part_st {
      bool part_last;
      const key_range *key;
      const byte *part_ptr;
      bool part_null;
      int bound_type;
      const char* bound_ptr;
    };
    struct part_st part[2];

    for (j= 0; j <= 1; j++)
    {
      struct part_st &p= part[j];
      p.key= NULL;
      p.bound_type= -1;
      if (tot_len < key_tot_len[j])
      {
        p.part_last= (tot_len + part_store_len >= key_tot_len[j]);
        p.key= keys[j];
        p.part_ptr= &p.key->key[tot_len];
        p.part_null= key_part->null_bit && *p.part_ptr;
        p.bound_ptr=
          p.part_null ? 0 : key_part->null_bit ? p.part_ptr + 1 : p.part_ptr;

        if (j == 0)
        {
          switch (p.key->flag)
          {
            case HA_READ_KEY_EXACT:
              if (! rir)
                p.bound_type= NdbIndexScanOperation::BoundEQ;
              else // differs for records_in_range
                p.bound_type= NdbIndexScanOperation::BoundLE;
              break;
            // ascending
            case HA_READ_KEY_OR_NEXT:
              p.bound_type= NdbIndexScanOperation::BoundLE;
              break;
            case HA_READ_AFTER_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundLE;
              else
                p.bound_type= NdbIndexScanOperation::BoundLT;
              break;
            // descending
            case HA_READ_PREFIX_LAST:           // weird
              p.bound_type= NdbIndexScanOperation::BoundEQ;
              break;
            case HA_READ_PREFIX_LAST_OR_PREV:   // weird
              p.bound_type= NdbIndexScanOperation::BoundGE;
              break;
            case HA_READ_BEFORE_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundGE;
              else
                p.bound_type= NdbIndexScanOperation::BoundGT;
              break;
            default:
              break;
          }
        }
        if (j == 1) {
          switch (p.key->flag)
          {
            // ascending
            case HA_READ_BEFORE_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundGE;
              else
                p.bound_type= NdbIndexScanOperation::BoundGT;
              break;
            case HA_READ_AFTER_KEY:     // weird
              p.bound_type= NdbIndexScanOperation::BoundGE;
              break;
            default:
              break;
            // descending strangely sets no end key
          }
        }

        if (p.bound_type == -1)
        {
          DBUG_PRINT("error", ("key %d unknown flag %d", j, p.key->flag));
          DBUG_ASSERT(FALSE);
          // Stop setting bounds but continue with what we have
          DBUG_RETURN(op->end_of_bound(range_no));
        }
      }
    }

    // Seen with e.g. b = 1 and c > 1
    if (part[0].bound_type == NdbIndexScanOperation::BoundLE &&
        part[1].bound_type == NdbIndexScanOperation::BoundGE &&
        memcmp(part[0].part_ptr, part[1].part_ptr, part_store_len) == 0)
    {
      DBUG_PRINT("info", ("replace LE/GE pair by EQ"));
      part[0].bound_type= NdbIndexScanOperation::BoundEQ;
      part[1].bound_type= -1;
    }
    // Not seen but was in previous version
    if (part[0].bound_type == NdbIndexScanOperation::BoundEQ &&
        part[1].bound_type == NdbIndexScanOperation::BoundGE &&
        memcmp(part[0].part_ptr, part[1].part_ptr, part_store_len) == 0)
    {
      DBUG_PRINT("info", ("remove GE from EQ/GE pair"));
      part[1].bound_type= -1;
    }

    for (j= 0; j <= 1; j++)
    {
      struct part_st &p= part[j];
      // Set bound if not done with this key
      if (p.key != NULL)
      {
        DBUG_PRINT("info", ("key %d:%d  offset: %d  length: %d  last: %d  bound: %d",
                            j, i, tot_len, part_len, p.part_last, p.bound_type));
        DBUG_DUMP("info", (const char*)p.part_ptr, part_store_len);

        // Set bound if not cancelled via type -1
        if (p.bound_type != -1)
        {
          const char* ptr= p.bound_ptr;
          char buf[256];
          shrink_varchar(field, ptr, buf);
          if (op->setBound(i, p.bound_type, ptr))
            ERR_RETURN(op->getNdbError());
        }
      }
    }

    tot_len+= part_store_len;
  }
  DBUG_RETURN(op->end_of_bound(range_no));
}

/* Count number of columns in key part. */
static uint
count_key_columns(const KEY *key_info, const key_range *key)
{
  KEY_PART_INFO *first_key_part= key_info->key_part;
  KEY_PART_INFO *key_part_end= first_key_part + key_info->key_parts;
  KEY_PART_INFO *key_part;
  uint length= 0;
  for(key_part= first_key_part; key_part < key_part_end; key_part++)
  {
    if (length >= key->length)
      break;
    length+= key_part->store_length;
  }
  return key_part - first_key_part;
}

/* Helper method to compute NDB index bounds. Note: does not set range_no. */
static void
compute_index_bounds(NdbIndexScanOperation::IndexBound & bound,
                     const KEY *key_info,
                     const key_range *start_key, const key_range *end_key)
{
  if (start_key)
  {
    bound.low_key= start_key->key;
    bound.low_key_count= count_key_columns(key_info, start_key);
    bound.low_inclusive=
      start_key->flag != HA_READ_AFTER_KEY &&
      start_key->flag != HA_READ_BEFORE_KEY;
  }
  else
  {
    bound.low_key= NULL;
    bound.low_key_count= 0;
  }

  if (start_key &&
      (start_key->flag == HA_READ_KEY_EXACT ||
       start_key->flag == HA_READ_PREFIX_LAST))
  {
    bound.high_key= bound.low_key;
    bound.high_key_count= bound.low_key_count;
    bound.high_inclusive= TRUE;
  }
  else if (end_key)
  {
    bound.high_key= end_key->key;
    bound.high_key_count= count_key_columns(key_info, end_key);
    /*
      For some reason, 'where b >= 1 and b <= 3' uses HA_READ_AFTER_KEY for
      the end_key.
      So HA_READ_AFTER_KEY in end_key sets high_inclusive, even though in
      start_key it does not set low_inclusive.
    */
    bound.high_inclusive= end_key->flag != HA_READ_BEFORE_KEY;
    if (end_key->flag == HA_READ_KEY_EXACT ||
        end_key->flag == HA_READ_PREFIX_LAST)
    {
      bound.low_key= bound.high_key;
      bound.low_key_count= bound.high_key_count;
      bound.low_inclusive= TRUE;
    }
  }
  else
  {
    bound.high_key= NULL;
    bound.high_key_count= 0;
  }
}

struct ordered_index_scan_data {
  const KEY *key_info;
  const key_range *start_key;
  const key_range *end_key;
};

/* Callback to set up scan bounds for ordered_index_scan(). */
static int
ordered_index_scan_callback(void *arg, Uint32 i,
                            NdbIndexScanOperation::IndexBound & bound)
{
  struct ordered_index_scan_data *data= (struct ordered_index_scan_data *)arg;
  compute_index_bounds(bound, data->key_info, data->start_key, data->end_key);
  bound.range_no= 0;
  return 0;                                     // Success
}


/*
  Start ordered index scan in NDB
*/

int ha_ndbcluster::ordered_index_scan(const key_range *start_key,
                                      const key_range *end_key,
                                      bool sorted, bool descending,
                                      byte* buf, part_id_range *part_spec)
{  
  NdbTransaction *trans= m_active_trans;
  NdbIndexScanOperation *op;
  struct ordered_index_scan_data data;
  uchar *mask;
  int error;

  DBUG_ENTER("ha_ndbcluster::ordered_index_scan");
  DBUG_PRINT("enter", ("index: %u, sorted: %d, descending: %d read_set=0x%x",
             active_index, sorted, descending, table->read_set->bitmap[0]));
  DBUG_PRINT("enter", ("Starting new ordered scan on %s", m_tabname));

  // Check that sorted seems to be initialised
  DBUG_ASSERT(sorted == 0 || sorted == 1);
  
  if (m_active_cursor && (error= close_scan()))
    DBUG_RETURN(error);

  if (m_use_partition_function || table_share->primary_key == MAX_KEY)
  {
    mask= copy_column_set(table->read_set);
    if (table_share->primary_key == MAX_KEY)
      request_hidden_key(mask);
  }
  else
    mask= (uchar *)(table->read_set->bitmap);

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  Uint32 scan_flags= 0;
  if (lm == NdbOperation::LM_Read)
    scan_flags|= NdbScanOperation::SF_KeyInfo;
  if (sorted)
    scan_flags|= NdbScanOperation::SF_OrderBy;
  if (descending)
    scan_flags|= NdbScanOperation::SF_Descending;
  const NdbRecord *key_rec= m_index[active_index].ndb_record_key;
  const NdbRecord *row_rec= m_index[active_index].ndb_record_row;
  Uint32 num_bounds= (start_key != NULL || end_key != NULL);
  data.key_info= table->key_info + active_index;
  if (!descending) {
    data.start_key= start_key;
    data.end_key= end_key;
  }
  else
  {
    data.start_key= end_key;
    data.end_key= start_key;
  }

  if (!(op= trans->scanIndex(key_rec, ordered_index_scan_callback, &data,
                             num_bounds, row_rec, lm,
                             mask,
                             scan_flags, parallelism, 0)))
    ERR_RETURN(trans->getNdbError());

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, NULL, table->read_set) != 0)
    ERR_RETURN(op->getNdbError());

  if (m_use_partition_function && part_spec != NULL &&
      part_spec->start_part == part_spec->end_part)
    op->setPartitionId(part_spec->start_part);
  m_active_cursor= op;

  if (m_cond && m_cond->generate_scan_filter(op))
    DBUG_RETURN(ndb_err(trans));

  if (execute_no_commit(this,trans,FALSE) != 0)
    DBUG_RETURN(ndb_err(trans));
  
  DBUG_RETURN(next_result(buf));
}

static
int
guess_scan_flags(NdbOperation::LockMode lm, 
		 const NDBTAB* tab, const MY_BITMAP* readset)
{
  int flags= 0;
  flags|= (lm == NdbOperation::LM_Read) ? NdbScanOperation::SF_KeyInfo : 0;
  if (tab->checkColumns(0, 0) & 2)
  {
    int ret = tab->checkColumns(readset->bitmap, no_bytes_in_map(readset));
    
    if (ret & 2)
    { // If disk columns...use disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    }
    else if ((ret & 4) == 0 && (lm == NdbOperation::LM_Exclusive))
    {
      // If no mem column is set and exclusive...guess disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    }
  }
  return flags;
}


/*
  Unique index scan in NDB (full table scan with scan filter)
 */

int ha_ndbcluster::unique_index_scan(const KEY* key_info, 
				     const byte *key, 
				     uint key_len,
				     byte *buf)
{
  NdbScanOperation *op;
  NdbTransaction *trans= m_active_trans;
  part_id_range part_spec;
  uchar *mask= (uchar *)(table->read_set->bitmap);
  const NdbRecord *ndb_record= m_ndb_record;

  DBUG_ENTER("unique_index_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));

  if (table_share->primary_key == MAX_KEY || m_use_partition_function)
  {
    mask= copy_column_set(table->read_set);
    if (m_use_partition_function)
    {
      part_spec.start_part= 0;
      part_spec.end_part= m_part_info->get_tot_partitions() - 1;
      prune_partition_set(table, &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                          part_spec.start_part, part_spec.end_part));
      /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      */
      if (part_spec.start_part > part_spec.end_part)
      {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }

      // If table has user defined partitioning
      // and no primary key, we need to read the partition id
      // to support ORDER BY queries
      if (table_share->primary_key == MAX_KEY)
        ndb_record= m_ndb_record_fragment;
    }
    if (table_share->primary_key == MAX_KEY)
      request_hidden_key(mask);
  }

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  int flags= guess_scan_flags(lm, m_table, table->read_set);
  if (!(op=trans->scanTable(ndb_record, lm, mask, flags, parallelism)))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= op;

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, NULL, table->read_set) != 0)
    ERR_RETURN(op->getNdbError());

  if (m_use_partition_function)
  {
    part_spec.start_part= 0;
    part_spec.end_part= m_part_info->get_tot_partitions() - 1;
    prune_partition_set(table, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part = %u, part_spec.end_part = %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      m_active_cursor->setPartitionId(part_spec.start_part);
    }
  }
  if (!m_cond)
    m_cond= new ha_ndbcluster_cond;
  if (!m_cond)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(my_errno);
  }       
  if (m_cond->generate_scan_filter_from_key(op, key_info, key, key_len, buf))
    DBUG_RETURN(ndb_err(trans));

  if (execute_no_commit(this,trans,FALSE) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_PRINT("exit", ("Scan started successfully"));
  DBUG_RETURN(next_result(buf));
}


/*
  Start full table scan in NDB
 */

int ha_ndbcluster::full_table_scan(byte *buf)
{
  NdbScanOperation *op;
  NdbTransaction *trans= m_active_trans;
  part_id_range part_spec;
  uchar *mask= (uchar *)(table->read_set->bitmap);
  const NdbRecord *ndb_record= m_ndb_record;

  DBUG_ENTER("full_table_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));

  if (table_share->primary_key == MAX_KEY || m_use_partition_function)
  {
    mask= copy_column_set(table->read_set);
    if (m_use_partition_function)
    {
      part_spec.start_part= 0;
      part_spec.end_part= m_part_info->get_tot_partitions() - 1;
      prune_partition_set(table, &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                          part_spec.start_part, part_spec.end_part));
      /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      */
      if (part_spec.start_part > part_spec.end_part)
      {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }

      // If table has user defined partitioning
      // and no primary key, we need to read the partition id
      // to support ORDER BY queries
      if (table_share->primary_key == MAX_KEY)
        ndb_record= m_ndb_record_fragment;
    }
    if (table_share->primary_key == MAX_KEY)
      request_hidden_key(mask);
  }

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  int flags= guess_scan_flags(lm, m_table, table->read_set);
  if (!(op= trans->scanTable(ndb_record, lm, mask, flags, parallelism)))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= op;

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, NULL, table->read_set) != 0)
    ERR_RETURN(op->getNdbError());

  if (m_use_partition_function)
  {
    /*
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      m_active_cursor->setPartitionId(part_spec.start_part);
    }
  }

  if (m_cond && m_cond->generate_scan_filter(op))
    DBUG_RETURN(ndb_err(trans));

  if (execute_no_commit(this,trans,FALSE) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_PRINT("exit", ("Scan started successfully"));
  DBUG_RETURN(next_result(buf));
}

inline void
ha_ndbcluster::eventSetAnyValue(const THD *thd, NdbOperation *op)
{
  if (unlikely(m_slow_path))
  {
    if (!(thd->options & OPTION_BIN_LOG))
      op->setAnyValue(NDB_ANYVALUE_FOR_NOLOGGING);
    else if (thd->slave_thread)
      op->setAnyValue(thd->server_id);
  }
}

int ha_ndbcluster::write_row(byte *record)
{
  DBUG_ENTER("ha_ndbcluster::write_row");
  DBUG_RETURN(ndb_write_row(record, FALSE, FALSE));
}

/*
  Insert one record into NDB
*/
int ha_ndbcluster::ndb_write_row(byte *record, bool primary_key_update,
                                 bool batched_update)
{
  bool has_auto_increment;
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  uint32 part_id;
  char *row;
  bool need_execute;
  int error;
  DBUG_ENTER("ha_ndbcluster::ndb_write_row");

  has_auto_increment= (table->next_number_field && record == table->record[0]);

  if (has_auto_increment && table_share->primary_key != MAX_KEY) 
  {
    /*
     * Increase any auto_incremented primary key
     */
    m_skip_auto_increment= FALSE;
    if ((error= update_auto_increment()))
      DBUG_RETURN(error);
    m_skip_auto_increment= (insert_id_for_cur_row == 0);
  }

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys
   */
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in start_bulk_insert()
      start_bulk_insert will set parameters to ensure that each
      write_row is committed individually
    */
    int peek_res= peek_indexed_rows(record, TRUE);
    
    if (!peek_res) 
    {
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  /*
    Since the NdbRecord operations need row data to remain valid until
    execute(), for bulk insert we need to save rows in a buffer, and
    execute() whenever the buffer gets full.

    For non-bulk insert, we may still need to copy the row into a (bigger)
    buffer if we need extra space for the user-defined partitioning hash
    or the hidden primary key, but we always execute() in this case.

    Note that when using writeTuple() with blobs, we cannot batch, as
    NdbBlob::setValue() uses call-by-reference semantics for the blob value,
    which must remain valid until execute(). For insertTuple(), the blob
    value is buffered by NdbBlob::setValue().
  */
  bool uses_blobs= uses_blob_value(table->write_set);
  if ((m_rows_to_insert > 1 && !uses_blobs) || batched_update ||
      ( (thd->options & OPTION_ALLOW_BATCH) && !(uses_blobs && m_use_write)))
  {
    /* This sets row and need_execute (output parameters). */
    row= batch_copy_row_to_buffer(thd_ndb, record, need_execute);
    DBUG_PRINT("info", ("allocating buffer for bulk insert, "
                        "m_rows_to_insert=%d write_set=0x%x",
                        (int)m_rows_to_insert, table->write_set->bitmap[0]));
    if (unlikely(!row))
      DBUG_RETURN(ER_OUTOFMEMORY);
  }
  else
  {
    DBUG_PRINT("info", ("Non-bulk insert."));
    need_execute= TRUE;
    if (table_share->primary_key == MAX_KEY || m_use_partition_function)
    {
      DBUG_PRINT("info", ("Getting single buffer for oversize record."));
      row= copy_row_to_buffer(thd_ndb, record);
      if (unlikely(!row))
        DBUG_RETURN(ER_OUTOFMEMORY);
    }
    else
      row= record;
  }

  if (table_share->primary_key == MAX_KEY)
  {
    // Table has hidden primary key
    Ndb *ndb= get_ndb();
    Uint64 auto_value;
    uint retries= NDB_AUTO_INCREMENT_RETRIES;
    int retry_sleep= 30; /* 30 milliseconds, transaction */
    for (;;)
    {
      Ndb_tuple_id_range_guard g(m_share);
      if (ndb->getAutoIncrementValue(m_table, g.range, auto_value, 1) == -1)
      {
        if (--retries &&
            ndb->getNdbError().status == NdbError::TemporaryError);
        {
          my_sleep(retry_sleep);
          continue;
        }
        ERR_RETURN(ndb->getNdbError());
      }
      break;
    }
    set_hidden_key(row, auto_value);
  } 

  if (m_use_partition_function)
  {
    longlong func_value= 0;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (error)
    {
      m_part_info->err_value= func_value;
      DBUG_RETURN(error);
    }

    /*
      We need to set the value of the partition function value in
      NDB since the NDB kernel doesn't have easy access to the function
      to calculate the value.
    */
    if (func_value >= INT_MAX32)
      func_value= INT_MAX32;
    set_partition_function_value(row, (uint32)func_value);
  }

  ha_statistic_increment(&SSV::ha_write_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  /*
    We do not use the table->write_set here.
    The reason is that for REPLACE INTO t(a), the write_set is passed with
    only column 'a' enabled.
    But it is wrong not to write all columns in REPLACE, since REPLACE is
    the same as DELETE+INSERT (ie. not writing all columns risks loosing
    default values).
  */
  /*
    ToDo: Actually, we have to use the write set, since otherwise replication
    fails. Replication seems to rely on being able to replicate an update with
    a write_row() with only some bits set in write_set, leaving other fields
    intact.
    This means that we now suffer from BUG#22045... :-/
  */

  if (m_use_write)
  {
    const NdbRecord *key_rec;
    const char *key_row;
    uchar *mask;
    if (table_share->primary_key == MAX_KEY || m_use_partition_function)
    {
      mask= copy_column_set(table->write_set);
      if (m_use_partition_function)
        request_partition_function_value(mask);
      if (table_share->primary_key == MAX_KEY)
        request_hidden_key(mask);
    }
    else
      mask= (uchar *)(table->write_set->bitmap);

    if (table_share->primary_key == MAX_KEY)
    {
      key_rec= m_ndb_hidden_key_record;
      key_row= &row[offset_hidden_key()];
    }
    else
    {
      key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
      key_row= row;
    }
    op= trans->writeTuple(key_rec, key_row, m_ndb_record, row, mask);
  }
  else
    op= trans->insertTuple(m_ndb_record, row);
  if (!(op))
    ERR_RETURN(trans->getNdbError());

  eventSetAnyValue(thd, op);

  if (m_use_partition_function)
    op->setPartitionId(part_id);

  uint blob_count= 0;
  if (table_share->blob_fields > 0)
  {
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    int res= set_blob_values(op, row - table->record[0], NULL, &blob_count);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (res != 0)
      ERR_RETURN(op->getNdbError());
  }

  m_rows_changed++;

  /*
    Execute write operation
    NOTE When doing inserts with many values in 
    each INSERT statement it should not be necessary
    to NoCommit the transaction between each row.
    Find out how this is detected!
  */
  m_rows_inserted++;
  no_uncommitted_rows_update(1);
  if (need_execute || primary_key_update)
  {
    int res= flush_bulk_insert();
    if (res != 0)
    {
      m_skip_auto_increment= TRUE;
      DBUG_RETURN(res);
    }
  }
  if ((has_auto_increment) && (m_skip_auto_increment))
  {
    Ndb *ndb= get_ndb();
    Uint64 next_val= (Uint64) table->next_number_field->val_int() + 1;
#ifndef DBUG_OFF
    char buff[22];
    DBUG_PRINT("info", 
               ("Trying to set next auto increment value to %s",
                llstr(next_val, buff)));
#endif
    Ndb_tuple_id_range_guard g(m_share);
    if (ndb->setAutoIncrementValue(m_table, g.range, next_val, TRUE)
        == -1)
      ERR_RETURN(ndb->getNdbError());
  }
  m_skip_auto_increment= TRUE;

  DBUG_PRINT("exit",("ok"));
  DBUG_RETURN(0);
}


/* Compare if an update changes the primary key in a row. */
int ha_ndbcluster::primary_key_cmp(const byte * old_row, const byte * new_row)
{
  uint keynr= table_share->primary_key;
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for (; key_part != end ; key_part++)
  {
    if (!bitmap_is_set(table->write_set, key_part->fieldnr - 1))
      continue;

    /* The primary key does not allow NULLs. */
    DBUG_ASSERT(!key_part->null_bit);

    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {

      if (key_part->field->cmp_binary((char*) (old_row + key_part->offset),
                                      (char*) (new_row + key_part->offset),
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
  Update one record in NDB using primary key
*/

int ha_ndbcluster::update_row(const byte *old_data, byte *new_data)
{
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= m_active_trans;
  NdbScanOperation* cursor= m_active_cursor;
  NdbOperation *op;
  uint32 old_part_id= 0, new_part_id= 0;
  int error;
  longlong func_value;
  bool pk_update= (table_share->primary_key != MAX_KEY &&
		   primary_key_cmp(old_data, new_data));
  DBUG_ENTER("update_row");
  
  /*
   * If IGNORE the ignore constraint violations on primary and unique keys,
   * but check that it is not part of INSERT ... ON DUPLICATE KEY UPDATE
   */
  if (m_ignore_dup_key && (thd->lex->sql_command == SQLCOM_UPDATE ||
                           thd->lex->sql_command == SQLCOM_UPDATE_MULTI))
  {
    int peek_res= peek_indexed_rows(new_data, pk_update);
    
    if (!peek_res) 
    {
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  ha_statistic_increment(&SSV::ha_update_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
  {
    table->timestamp_field->set_time();
    bitmap_set_bit(table->write_set, table->timestamp_field->field_index);
  }

  if (m_use_partition_function &&
      (error= get_parts_for_update(old_data, new_data, table->record[0],
                                   m_part_info, &old_part_id, &new_part_id,
                                   &func_value)))
  {
    m_part_info->err_value= func_value;
    DBUG_RETURN(error);
  }

  /*
   * Check for update of primary key or partition change
   * for special handling
   */  
  if (pk_update || old_part_id != new_part_id)
  {
    int read_res, insert_res, delete_res;

    DBUG_PRINT("info", ("primary key update or partition change, "
                        "doing read+delete+insert"));
    // Get all old fields, since we optimize away fields not in query
    read_res= complemented_read(old_data, new_data, old_part_id);
    if (read_res)
    {
      DBUG_PRINT("info", ("read failed"));
      DBUG_RETURN(read_res);
    }
    // Delete old row
    delete_res= ndb_delete_row(old_data, TRUE);
    if (delete_res)
    {
      DBUG_PRINT("info", ("delete failed"));
      DBUG_RETURN(delete_res);
    }     
    // Insert new row
    DBUG_PRINT("info", ("delete succeded"));
    bool batched_update= (cursor != 0);
    insert_res= ndb_write_row(new_data, TRUE, batched_update);
    if (insert_res)
    {
      DBUG_PRINT("info", ("insert failed"));
      if (trans->commitStatus() == NdbConnection::Started)
      {
        trans->execute(NdbTransaction::Rollback);
#ifdef FIXED_OLD_DATA_TO_ACTUALLY_CONTAIN_GOOD_DATA
        int undo_res;
        // Undo delete_row(old_data)
        undo_res= ndb_write_row((byte *)old_data, TRUE, batched_update);
        if (undo_res)
          push_warning(current_thd, 
                       MYSQL_ERROR::WARN_LEVEL_WARN, 
                       undo_res, 
                       "NDB failed undoing delete at primary key update");
#endif
      }
      DBUG_RETURN(insert_res);
    }
    DBUG_PRINT("info", ("delete+insert succeeded"));
    DBUG_RETURN(0);
  }

  /*
    Set only non-primary-key attributes.
    We already checked that any primary key attribute in write_set has no
    real changes.
  */
  bitmap_copy(&m_bitmap, table->write_set);
  bitmap_subtract(&m_bitmap, &m_pk_bitmap);
  uchar *mask= (uchar *)(m_bitmap.bitmap);
  /* Need to initialize bits for any extra hidden columns. */
  if (table_share->primary_key == MAX_KEY || m_use_partition_function)
    clear_extended_column_set(mask);

  /* Need to set the value of any user-defined partitioning function. */
  char *row;
  bool need_execute;
  /*
    Batch update operation if we are doing a scan for update, unless
    there exist UPDATE AFTER triggers
  */
  if (cursor && !m_update_cannot_batch)
  {
    /* For a scan, we only need to execute() if the batch buffer is full. */
    row= batch_copy_row_to_buffer(thd_ndb, new_data, need_execute);
    if (unlikely(!row))
      DBUG_RETURN(ER_OUTOFMEMORY);
  }
  else
  {
    need_execute= TRUE;
    if (m_use_partition_function)
    {
      row= copy_row_to_buffer(thd_ndb, new_data);
      if (unlikely(!row))
        DBUG_RETURN(ER_OUTOFMEMORY);
    }
    else
      row= new_data;
  }

  if (m_use_partition_function)
  {
    if (func_value >= INT_MAX32)
      func_value= INT_MAX32;
    set_partition_function_value(row, (uint32)func_value);
    request_partition_function_value(mask);
  }

  if (cursor)
  {
    /*
      We are scanning records and want to update the record
      that was just found, call updateCurrentTuple on the cursor 
      to take over the lock to a new update operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling updateTuple on cursor, write_set=0x%x",
                        table->write_set->bitmap[0]));
    if (!(op= cursor->updateCurrentTuple(trans, m_ndb_record, row, mask)))
      ERR_RETURN(trans->getNdbError());

    m_lock_tuple= FALSE;
    m_ops_pending++;
  }
  else
  {  
    const NdbRecord *key_rec;
    const char *key_row;
    if (table_share->primary_key != MAX_KEY)
    {
      key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
      key_row= old_data;
    }
    else
    {
      /* Use hidden primary key previously read into m_ref. */
      key_rec= m_ndb_hidden_key_record;
      key_row= (const char *)(&m_ref);
    }

    if (!(op= trans->updateTuple(key_rec, key_row, m_ndb_record, row, mask)))
      ERR_RETURN(trans->getNdbError());  
  }

  if (m_use_partition_function)
    op->setPartitionId(new_part_id);

  uint blob_count;
  if (uses_blob_value(table->write_set))
  {
    int row_offset= new_data - table->record[0];
    if (set_blob_values(op, row_offset, table->write_set, &blob_count) != 0)
      ERR_RETURN(op->getNdbError());
    if (cursor && blob_count > 0)
      m_blobs_pending= TRUE;
  }

  eventSetAnyValue(thd, op);

  m_rows_changed++;

  if (need_execute && execute_no_commit(this,trans,FALSE) != 0) {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  
  DBUG_RETURN(0);
}


int ha_ndbcluster::delete_row(const byte *record)
{
  return ndb_delete_row(record, FALSE);
}

/*
  Delete one record from NDB, using primary key 
*/

int ha_ndbcluster::ndb_delete_row(const byte *record, bool primary_key_update)
{
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= m_active_trans;
  NdbScanOperation* cursor= m_active_cursor;
  NdbOperation *op;
  uint32 part_id;
  int error;
  DBUG_ENTER("ndb_delete_row");

  ha_statistic_increment(&SSV::ha_delete_count);
  m_rows_changed++;

  if (m_use_partition_function &&
      (error= get_part_for_delete(record, table->record[0], m_part_info,
                                  &part_id)))
  {
    DBUG_RETURN(error);
  }

  if (cursor)
  {
    /*
      We are scanning records and want to delete the record
      that was just found, call deleteTuple on the cursor 
      to take over the lock to a new delete operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling deleteTuple on cursor"));
    if ((op= cursor->deleteCurrentTuple(trans, m_ndb_record)) == 0)
      ERR_RETURN(trans->getNdbError());     
    m_lock_tuple= FALSE;
    m_ops_pending++;

    if (m_use_partition_function)
      op->setPartitionId(part_id);

    no_uncommitted_rows_update(-1);

    eventSetAnyValue(thd, op);

    if (!(primary_key_update || m_delete_cannot_batch))
      // If deleting from cursor, NoCommit will be handled in next_result
      DBUG_RETURN(0);
  }
  else
  {
    const NdbRecord *key_rec;
    const char *key_row;
    uint key_len;
    if (table_share->primary_key != MAX_KEY)
    {
      key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
      key_row= record;
      key_len= table->s->reclength;
    }
    else
    {
      key_rec= m_ndb_hidden_key_record;
      key_row= (const char *)(&m_ref);
      key_len= sizeof(m_ref);
    }
    /*
      Check if we can batch the delete; if so we need to buffer the key.

      We do not batch deletes on tables with no primary key. For such tables,
      replication uses full table scan to locate the row to delete. The
      problem is the following scenario when deleting 2 (or more) rows:

       1. Table scan to locate the first row.
       2. Delete the row, batched so no execute.
       3. Table scan to locate the second row is executed, along with the
          batched delete operation from step 2.
       4. The first row is returned from nextResult() (not deleted yet).
       5. The kernel deletes the row (operation from step 2).
       6. lockCurrentTuple() is called on the row returned in step 4. However,
          as that row is now deleted, the operation fails and the transaction
          is aborted.
       7. The delete of the second tuple now fails, as the transaction has
          been aborted.
    */
    bool need_execute;
    if ((thd->options & OPTION_ALLOW_BATCH) &&
        table_share->primary_key != MAX_KEY)
    {
      /*
        Poor approx. let delete ~ tabsize / 4
      */
      uint delete_size= 12 + m_bytes_per_write >> 2;
      key_row= batch_copy_key_to_buffer(thd_ndb, key_row, key_len,
                                        delete_size, need_execute);
      if (unlikely(!key_row))
        DBUG_RETURN(ER_OUTOFMEMORY);
    }
    else
      need_execute= TRUE;

    if (!(op=trans->deleteTuple(key_rec, key_row)))
      ERR_RETURN(trans->getNdbError());
    
    if (m_use_partition_function)
      op->setPartitionId(part_id);

    no_uncommitted_rows_update(-1);

    eventSetAnyValue(thd, op);

    if (!need_execute)
      DBUG_RETURN(0);
  }

  // Execute delete operation
  if (execute_no_commit(this,trans,FALSE) != 0) {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  DBUG_RETURN(0);
}
  
/*
  Unpack a record returned from a scan.
  We copy field-for-field to
   1. Avoid unnecessary copying for sparse rows.
   2. Properly initialize not used null bits.
  Note that we do not unpack all returned rows; some primary/unique key
  operations can read directly into the destination row.
*/
void ha_ndbcluster::unpack_record(byte *dst_row, const byte *src_row)
{
  int res;
  DBUG_ASSERT(src_row != NULL);

  my_ptrdiff_t dst_offset= dst_row - table->record[0];
  my_ptrdiff_t src_offset= src_row - table->record[0];

  /* Initialize the NULL bitmap. */
  memset(dst_row, 0xff, table->s->null_bytes);

  char *blob_ptr= m_blobs_buffer;

  for (uint i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (bitmap_is_set(table->read_set, i))
    {
      if (field->type() == MYSQL_TYPE_BIT)
      {
        Field_bit *field_bit= static_cast<Field_bit*>(field);
        if (!field->is_null_in_record_with_offset(src_offset))
        {
          field->move_field_offset(src_offset);
          longlong value= field_bit->val_int();
          field->move_field_offset(dst_offset-src_offset);
          field_bit->set_notnull();
          /* Field_bit in DBUG requires the bit set in write_set for store(). */
          my_bitmap_map *old_map=
            dbug_tmp_use_all_columns(table, table->write_set);
          IF_DBUG(int res=) field_bit->store(value, true);
          dbug_tmp_restore_column_map(table->write_set, old_map);
          DBUG_ASSERT(res == 0);
          field->move_field_offset(-dst_offset);
        }
      }
      else if (field->flags & BLOB_FLAG)
      {
        Field_blob *field_blob= (Field_blob *)field;
        NdbBlob *ndb_blob= m_value[i].blob;
        DBUG_ASSERT(ndb_blob != 0);
        int isNull;
        res= ndb_blob->getNull(isNull);
        DBUG_ASSERT(res == 0);                  // Already succeeded once
        Uint64 len64= 0;
        field_blob->move_field_offset(dst_offset);
        if (!isNull)
        {
          res= ndb_blob->getLength(len64);
          DBUG_ASSERT(res == 0 && len64 <= (Uint64)0xffffffff);
          field->set_notnull();
        }
        /* Need not set_null(), as we initialized null bits to 1 above. */
        field_blob->set_ptr((uint32)len64, blob_ptr);
        field_blob->move_field_offset(-dst_offset);
        blob_ptr+= (len64 + 7) & ~((Uint64)7);
      }
      else
      {
        field->move_field_offset(src_offset);
        /* Normal field (not blob or bit type). */
        if (!field->is_null())
        {
          /* Only copy actually used bytes of varstrings. */
          uint32 actual_length= field->used_length();
          char *src_ptr= field->ptr;
          field->move_field_offset(dst_offset - src_offset);
          field->set_notnull();
          memcpy(field->ptr, src_ptr, actual_length);
#ifdef HAVE_purify
          /*
            We get Valgrind warnings on uninitialised padding bytes in
            varstrings, for example when writing rows to temporary tables.
            So for valgrind builds we pad with zeros, not needed for
            production code.
          */
          if (actual_length < field->pack_length())
            bzero(field->ptr + actual_length,
                  field->pack_length() - actual_length);
#endif
          field->move_field_offset(-dst_offset);
        }
        else
          field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }
  }
}

/*
  Utility function to print/dump the fetched field
  to avoid unnecessary work, wrap in DBUG_EXECUTE as in:

    DBUG_EXECUTE("value", print_results(););
 */

void ha_ndbcluster::print_results()
{
  DBUG_ENTER("print_results");

#ifndef DBUG_OFF

  char buf_type[MAX_FIELD_WIDTH], buf_val[MAX_FIELD_WIDTH];
  String type(buf_type, sizeof(buf_type), &my_charset_bin);
  String val(buf_val, sizeof(buf_val), &my_charset_bin);
  for (uint f= 0; f < table_share->fields; f++)
  {
    /* Use DBUG_PRINT since DBUG_FILE cannot be filtered out */
    char buf[2000];
    Field *field;
    void* ptr;
    NdbValue value;

    buf[0]= 0;
    field= table->field[f];
    if (!(value= m_value[f]).ptr)
    {
      strmov(buf, "not read");
      goto print_value;
    }

    ptr= field->ptr;

    if (! (field->flags & BLOB_FLAG))
    {
      if (value.rec->isNULL())
      {
        strmov(buf, "NULL");
        goto print_value;
      }
      type.length(0);
      val.length(0);
      field->sql_type(type);
      field->val_str(&val);
      my_snprintf(buf, sizeof(buf), "%s %s", type.c_ptr(), val.c_ptr());
    }
    else
    {
      NdbBlob *ndb_blob= value.blob;
      bool isNull= TRUE;
      ndb_blob->getNull(isNull);
      if (isNull)
        strmov(buf, "NULL");
    }

print_value:
    DBUG_PRINT("value", ("%u,%s: %s", f, field->field_name, buf));
  }
#endif
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::index_init(uint index, bool sorted)
{
  DBUG_ENTER("ha_ndbcluster::index_init");
  DBUG_PRINT("enter", ("index: %u  sorted: %d", index, sorted));
  active_index= index;
  m_sorted= sorted;
  /*
    Locks are are explicitly released in scan
    unless m_lock.type == TL_READ_HIGH_PRIORITY
    and no sub-sequent call to unlock_row()
  */
  m_lock_tuple= FALSE;
  DBUG_RETURN(0);
}


int ha_ndbcluster::index_end()
{
  DBUG_ENTER("ha_ndbcluster::index_end");
  DBUG_RETURN(close_scan());
}

/**
 * Check if key contains null
 */
static
int
check_null_in_key(const KEY* key_info, const byte *key, uint key_len)
{
  KEY_PART_INFO *curr_part, *end_part;
  const byte* end_ptr= key + key_len;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->key_parts;

  for (; curr_part != end_part && key < end_ptr; curr_part++)
  {
    if (curr_part->null_bit && *key)
      return 1;

    key += curr_part->store_length;
  }
  return 0;
}

int ha_ndbcluster::index_read(byte *buf,
                              const byte *key, uint key_len, 
                              enum ha_rkey_function find_flag)
{
  key_range start_key;
  bool descending= FALSE;
  DBUG_ENTER("ha_ndbcluster::index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  start_key.key= key;
  start_key.length= key_len;
  start_key.flag= find_flag;
  descending= FALSE;
  switch (find_flag) {
  case HA_READ_KEY_OR_PREV:
  case HA_READ_BEFORE_KEY:
  case HA_READ_PREFIX_LAST:
  case HA_READ_PREFIX_LAST_OR_PREV:
    descending= TRUE;
    break;
  default:
    break;
  }
  DBUG_RETURN(read_range_first_to_buf(&start_key, 0, descending,
                                      m_sorted, buf));
}


int ha_ndbcluster::index_next(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_next");
  ha_statistic_increment(&SSV::ha_read_next_count);
  DBUG_RETURN(next_result(buf));
}


int ha_ndbcluster::index_prev(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_prev");
  ha_statistic_increment(&SSV::ha_read_prev_count);
  DBUG_RETURN(next_result(buf));
}


int ha_ndbcluster::index_first(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_first");
  ha_statistic_increment(&SSV::ha_read_first_count);
  // Start the ordered index scan and fetch the first row

  // Only HA_READ_ORDER indexes get called by index_first
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, FALSE, buf, NULL));
}


int ha_ndbcluster::index_last(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_last");
  ha_statistic_increment(&SSV::ha_read_last_count);
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, TRUE, buf, NULL));
}

int ha_ndbcluster::index_read_last(byte * buf, const byte * key, uint key_len)
{
  DBUG_ENTER("ha_ndbcluster::index_read_last");
  DBUG_RETURN(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));
}

int ha_ndbcluster::read_range_first_to_buf(const key_range *start_key,
                                           const key_range *end_key,
                                           bool desc, bool sorted,
                                           byte* buf)
{
  part_id_range part_spec;
  ndb_index_type type= get_index_type(active_index);
  const KEY* key_info= table->key_info+active_index;
  int error; 
  DBUG_ENTER("ha_ndbcluster::read_range_first_to_buf");
  DBUG_PRINT("info", ("desc: %d, sorted: %d", desc, sorted));

  if (m_use_partition_function)
  {
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part > part_spec.end_part)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    else if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      sorted= FALSE;
    }
  }

  switch (type){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    if (start_key && 
        start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT)
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      error= pk_read(start_key->key, start_key->length, buf,
		     part_spec.start_part);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    if (start_key && start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT && 
        !check_null_in_key(key_info, start_key->key, start_key->length))
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);

      error= unique_index_read(start_key->key, start_key->length, buf);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    else if (type == UNIQUE_INDEX)
      DBUG_RETURN(unique_index_scan(key_info, 
				    start_key->key, 
				    start_key->length, 
				    buf));
    break;
  default:
    break;
  }
  // Start the ordered index scan and fetch the first row
  DBUG_RETURN(ordered_index_scan(start_key, end_key, sorted, desc, buf,
                                 &part_spec));
}

int ha_ndbcluster::read_range_first(const key_range *start_key,
                                    const key_range *end_key,
                                    bool eq_r, bool sorted)
{
  byte* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  DBUG_RETURN(read_range_first_to_buf(start_key, end_key, FALSE,
                                      sorted, buf));
}

int ha_ndbcluster::read_range_next()
{
  DBUG_ENTER("ha_ndbcluster::read_range_next");
  DBUG_RETURN(next_result(table->record[0]));
}


int ha_ndbcluster::rnd_init(bool scan)
{
  int error;
  DBUG_ENTER("rnd_init");
  DBUG_PRINT("enter", ("scan: %d", scan));

  if (m_active_cursor && (error= close_scan()))
    DBUG_RETURN(error);
  index_init(table_share->primary_key, 0);
  DBUG_RETURN(0);
}

int ha_ndbcluster::close_scan()
{
  NdbTransaction *trans= m_active_trans;
  int error;
  DBUG_ENTER("close_scan");

  NdbScanOperation *cursor= m_active_cursor;
  
  if (!cursor)
    DBUG_RETURN(0);

  if ((error= scan_handle_lock_tuple(cursor, trans)) != 0)
    DBUG_RETURN(error);

  if (m_ops_pending)
  {
    /*
      Take over any pending transactions to the 
      deleteing/updating transaction before closing the scan    
    */
    DBUG_PRINT("info", ("ops_pending: %ld", (long) m_ops_pending));    
    if (execute_no_commit(this,trans,FALSE) != 0) {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  
  cursor->close(m_force_send, TRUE);
  m_active_cursor= NULL;
  DBUG_RETURN(0);
}

int ha_ndbcluster::rnd_end()
{
  DBUG_ENTER("rnd_end");
  DBUG_RETURN(close_scan());
}


int ha_ndbcluster::rnd_next(byte *buf)
{
  DBUG_ENTER("rnd_next");
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  if (!m_active_cursor)
    DBUG_RETURN(full_table_scan(buf));
  DBUG_RETURN(next_result(buf));
}


/*
  An "interesting" record has been found and it's pk 
  retrieved by calling position
  Now it's time to read the record from db once 
  again
*/

int ha_ndbcluster::rnd_pos(byte *buf, byte *pos)
{
  DBUG_ENTER("rnd_pos");
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  // The primary key for the record is stored in pos
  // Perform a pk_read using primary key "index"
  {
    part_id_range part_spec;
    uint key_length= ref_length;
    if (m_use_partition_function)
    {
      if (table_share->primary_key == MAX_KEY)
      {
        /*
          The partition id has been fetched from ndb
          and has been stored directly after the hidden key
        */
        DBUG_DUMP("key+part", (char *)pos, key_length);
        key_length= ref_length - sizeof(m_part_id);
        part_spec.start_part= part_spec.end_part= *(uint32 *)(pos + key_length);
      }
      else
      {
        key_range key_spec;
        KEY *key_info= table->key_info + table_share->primary_key;
        key_spec.key= pos;
        key_spec.length= key_length;
        key_spec.flag= HA_READ_KEY_EXACT;
        get_full_part_id_from_key(table, buf, key_info, 
                                  &key_spec, &part_spec);
        DBUG_ASSERT(part_spec.start_part == part_spec.end_part);
      }
      DBUG_PRINT("info", ("partition id %u", part_spec.start_part));
    }
    DBUG_DUMP("key", (char *)pos, key_length);
    DBUG_RETURN(pk_read(pos, key_length, buf, part_spec.start_part));
  }
}


/*
  Store the primary key of this record in ref 
  variable, so that the row can be retrieved again later
  using "reference" in rnd_pos
*/

void ha_ndbcluster::position(const byte *record)
{
  KEY *key_info;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  byte *buff;
  uint key_length;

  DBUG_ENTER("position");

  if (table_share->primary_key != MAX_KEY) 
  {
    key_length= ref_length;
    key_info= table->key_info + table_share->primary_key;
    key_part= key_info->key_part;
    end= key_part + key_info->key_parts;
    buff= ref;
    
    for (; key_part != end; key_part++) 
    {
      if (key_part->null_bit) {
        /* Store 0 if the key part is a NULL part */      
        if (record[key_part->null_offset]
            & key_part->null_bit) {
          *buff++= 1;
          continue;
        }      
        *buff++= 0;
      }

      size_t len = key_part->length;
      const byte * ptr = record + key_part->offset;
      Field *field = key_part->field;
      if (field->type() ==  MYSQL_TYPE_VARCHAR)
      {
        if (((Field_varstring*)field)->length_bytes == 1)
        {
          /**
           * Keys always use 2 bytes length
           */
          buff[0] = ptr[0];
          buff[1] = 0;
          memcpy(buff+2, ptr + 1, len);
        }
        else
        {
          memcpy(buff, ptr, len + 2);
        }
        len += 2;
      }
      else
      {
        memcpy(buff, ptr, len);
      }
      buff += len;
    }
  } 
  else 
  {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    // If table has user defined partition save the partition id as well
    if(m_use_partition_function)
    {
      DBUG_PRINT("info", ("Saving partition id %u", m_part_id));
      key_length= ref_length - sizeof(m_part_id);
      memcpy(ref+key_length, (void *)&m_part_id, sizeof(m_part_id));
    }
    else
      key_length= ref_length;
#ifndef DBUG_OFF
    int hidden_no= table->s->fields;
    const NDBTAB *tab= m_table;  
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    DBUG_ASSERT(hidden_col->getPrimaryKey() && 
                hidden_col->getAutoIncrement() &&
                key_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
#endif
    memcpy(ref, &m_ref, key_length);
  }
#ifndef DBUG_OFF
  if (table_share->primary_key == MAX_KEY && m_use_partition_function) 
    DBUG_DUMP("key+part", (char*)ref, key_length+sizeof(m_part_id));
#endif
  DBUG_DUMP("ref", (char*)ref, key_length);
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::info(uint flag)
{
  int result= 0;
  DBUG_ENTER("info");
  DBUG_PRINT("enter", ("flag: %d", flag));
  
  if (flag & HA_STATUS_POS)
    DBUG_PRINT("info", ("HA_STATUS_POS"));
  if (flag & HA_STATUS_NO_LOCK)
    DBUG_PRINT("info", ("HA_STATUS_NO_LOCK"));
  if (flag & HA_STATUS_TIME)
    DBUG_PRINT("info", ("HA_STATUS_TIME"));
  if (flag & HA_STATUS_VARIABLE)
  {
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));
    if (m_table_info)
    {
      if (m_ha_not_exact_count)
        stats.records= 100;
      else
	result= records_update();
    }
    else
    {
      if ((my_errno= check_ndb_connection()))
        DBUG_RETURN(my_errno);
      Ndb *ndb= get_ndb();
      ndb->setDatabaseName(m_dbname);
      struct Ndb_statistics stat;
      if (ndb->setDatabaseName(m_dbname))
      {
        DBUG_RETURN(my_errno= HA_ERR_OUT_OF_MEM);
      }
      if (current_thd->variables.ndb_use_exact_count &&
          (result= ndb_get_table_statistics
           (this, TRUE, ndb, m_ndb_statistics_record, &stat)) == 0)
      {
        stats.mean_rec_length= stat.row_size;
        stats.data_file_length= stat.fragment_memory;
        stats.records= stat.row_count;
      }
      else
      {
        stats.mean_rec_length= 0;
        stats.records= 100;
      }
    }
  }
  if (flag & HA_STATUS_CONST)
  {
    DBUG_PRINT("info", ("HA_STATUS_CONST"));
    set_rec_per_key();
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY"));
    errkey= m_dupkey;
  }
  if (flag & HA_STATUS_AUTO)
  {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (m_table && table->found_next_number_field)
    {
      Ndb *ndb= get_ndb();
      Ndb_tuple_id_range_guard g(m_share);
      
      Uint64 auto_increment_value64;
      if (ndb->readAutoIncrementValue(m_table, g.range,
                                      auto_increment_value64) == -1)
      {
        const NdbError err= ndb->getNdbError();
        sql_print_error("Error %lu in readAutoIncrementValue(): %s",
                        (ulong) err.code, err.message);
        stats.auto_increment_value= ~(ulonglong)0;
      }
      else
        stats.auto_increment_value= (ulonglong)auto_increment_value64;
    }
  }

  if(result == -1)
    result= HA_ERR_NO_CONNECTION;

  DBUG_RETURN(result);
}


void ha_ndbcluster::get_dynamic_partition_info(PARTITION_INFO *stat_info,
                                               uint part_id)
{
  /* 
     This functions should be fixed. Suggested fix: to
     implement ndb function which retrives the statistics
     about ndb partitions.
  */
  bzero((char*) stat_info, sizeof(PARTITION_INFO));
  return;
}


int ha_ndbcluster::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("extra");
  switch (operation) {
  case HA_EXTRA_IGNORE_DUP_KEY:       /* Dup keys don't rollback everything*/
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));
    DBUG_PRINT("info", ("Ignoring duplicate key"));
    m_ignore_dup_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    m_ignore_dup_key= FALSE;
    break;
  case HA_EXTRA_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= FALSE;
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CAN_REPLACE"));
    if (!m_has_unique_index ||
        current_thd->slave_thread) /* always set if slave, quick fix for bug 27378 */
    {
      DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
      m_use_write= TRUE;
    }
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CANNOT_REPLACE"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= FALSE;
    break;
  case HA_EXTRA_DELETE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_DELETE_CANNOT_BATCH"));
    m_delete_cannot_batch= TRUE;
    break;
  case HA_EXTRA_UPDATE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_UPDATE_CANNOT_BATCH"));
    m_update_cannot_batch= TRUE;
    break;
  default:
    break;
  }
  
  DBUG_RETURN(0);
}


int ha_ndbcluster::reset()
{
  DBUG_ENTER("ha_ndbcluster::reset");
  if (m_cond)
  {
    m_cond->cond_clear();
  }

  /*
    Regular partition pruning will set the bitmap appropriately.
    Some queries like ALTER TABLE doesn't use partition pruning and
    thus the 'used_partitions' bitmap needs to be initialized
  */
  if (m_part_info)
    bitmap_set_all(&m_part_info->used_partitions);

  /* reset flags set by extra calls */
  m_ignore_dup_key= FALSE;
  m_use_write= FALSE;
  m_ignore_no_key= FALSE;
  m_delete_cannot_batch= FALSE;
  m_update_cannot_batch= FALSE;

  DBUG_RETURN(0);
}


/* 
   Start of an insert, remember number of rows to be inserted, it will
   be used in write_row and get_autoincrement to send an optimal number
   of rows in each roundtrip to the server

   SYNOPSIS
   rows     number of rows to insert, 0 if unknown

*/

int
ha_ndbcluster::flush_bulk_insert()
{
  NdbTransaction *trans= m_active_trans;
  DBUG_ENTER("ha_ndbcluster::flush_bulk_insert");
  DBUG_PRINT("info", ("Sending inserts to NDB, rows_inserted: %d", 
                      (int)m_rows_inserted));
  if (m_transaction_on)
  {
    if (execute_no_commit(this,trans,FALSE) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else
  {
    if (execute_commit(this,trans) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
    if (trans->restart() != 0)
    {
      DBUG_ASSERT(0);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

void ha_ndbcluster::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("start_bulk_insert");
  DBUG_PRINT("enter", ("rows: %d", (int)rows));
  
  m_rows_inserted= (ha_rows) 0;
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in write_row
      we have a situation where peek_indexed_rows() will be called
      so we cannot batch
    */
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    m_rows_to_insert= 1;
    DBUG_VOID_RETURN;
  }
  if (rows == (ha_rows) 0)
  {
    /* We don't know how many will be inserted, guess */
    m_rows_to_insert= m_autoincrement_prefetch;
  }
  else
    m_rows_to_insert= rows; 

  DBUG_VOID_RETURN;
}

/*
  End of an insert
 */
int ha_ndbcluster::end_bulk_insert()
{
  int error= 0;

  DBUG_ENTER("end_bulk_insert");
  // Check if last inserts need to be flushed

  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  
  if ((thd->options & OPTION_ALLOW_BATCH) == 0 && thd_ndb->m_unsent_bytes)
  {
    error= flush_bulk_insert();
    if (error != 0)
      my_errno= error;
  }

  m_rows_inserted= (ha_rows) 0;
  m_rows_to_insert= (ha_rows) 1;
  DBUG_RETURN(error);
}


int ha_ndbcluster::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  DBUG_ENTER("extra_opt");
  DBUG_PRINT("enter", ("cache_size: %lu", cache_size));
  DBUG_RETURN(extra(operation));
}

static const char *ha_ndbcluster_exts[] = {
 ha_ndb_ext,
 NullS
};

const char** ha_ndbcluster::bas_ext() const
{
  return ha_ndbcluster_exts;
}

/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_ndbcluster::scan_time()
{
  DBUG_ENTER("ha_ndbcluster::scan_time()");
  double res= rows2double(stats.records*1000);
  DBUG_PRINT("exit", ("table: %s value: %f", 
                      m_tabname, res));
  DBUG_RETURN(res);
}

/*
  Convert MySQL table locks into locks supported by Ndb Cluster.
  Note that MySQL Cluster does currently not support distributed
  table locks, so to be safe one should set cluster in Single
  User Mode, before relying on table locks when updating tables
  from several MySQL servers
*/

THR_LOCK_DATA **ha_ndbcluster::store_lock(THD *thd,
                                          THR_LOCK_DATA **to,
                                          enum thr_lock_type lock_type)
{
  DBUG_ENTER("store_lock");
  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK) 
  {

    /* If we are not doing a LOCK TABLE, then allow multiple
       writers */
    
    /* Since NDB does not currently have table locks
       this is treated as a ordinary lock */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)      
      lock_type= TL_WRITE_ALLOW_WRITE;
    
    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
       MySQL would use the lock TL_READ_NO_INSERT on t2, and that
       would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
       to t2. Convert the lock to a normal read lock to allow
       concurrent inserts to t2. */
    
    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;
    
    m_lock.type=lock_type;
  }
  *to++= &m_lock;

  DBUG_PRINT("exit", ("lock_type: %d", lock_type));
  
  DBUG_RETURN(to);
}

#ifndef DBUG_OFF
#define PRINT_OPTION_FLAGS(t) { \
      if (t->options & OPTION_NOT_AUTOCOMMIT) \
        DBUG_PRINT("thd->options", ("OPTION_NOT_AUTOCOMMIT")); \
      if (t->options & OPTION_BEGIN) \
        DBUG_PRINT("thd->options", ("OPTION_BEGIN")); \
      if (t->options & OPTION_TABLE_LOCK) \
        DBUG_PRINT("thd->options", ("OPTION_TABLE_LOCK")); \
}
#else
#define PRINT_OPTION_FLAGS(t)
#endif


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement, this will be stored in thd_ndb.stmt.
  If not, we have to start a master transaction if there doesn't exist
  one from before, this will be stored in thd_ndb.all
 
  When a table lock is held one transaction will be started which holds
  the table lock and for each statement a hupp transaction will be started  
  If we are locking the table then:
  - save the NdbDictionary::Table for easy access
  - save reference to table statistics
  - refresh list of the indexes for the table if needed (if altered)
 */

#ifdef HAVE_NDB_BINLOG
extern MASTER_INFO *active_mi;
static int ndbcluster_update_apply_status(THD *thd, int do_update)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *ndbtab;
  NdbTransaction *trans= thd_ndb->all ? thd_ndb->all : thd_ndb->stmt;
  ndb->setDatabaseName(NDB_REP_DB);
  Ndb_table_guard ndbtab_g(dict, NDB_APPLY_TABLE);
  if (!(ndbtab= ndbtab_g.get_table()))
  {
    return -1;
  }
  NdbOperation *op= 0;
  int r= 0;
  r|= (op= trans->getNdbOperation(ndbtab)) == 0;
  DBUG_ASSERT(r == 0);
  if (do_update)
    r|= op->updateTuple();
  else
    r|= op->writeTuple();
  DBUG_ASSERT(r == 0);
  // server_id
  r|= op->equal(0u, (Uint32)thd->server_id);
  DBUG_ASSERT(r == 0);
  if (!do_update)
  {
    // epoch
    r|= op->setValue(1u, (Uint64)0);
    DBUG_ASSERT(r == 0);
  }
  // log_name
  char tmp_buf[FN_REFLEN];
  ndb_pack_varchar(ndbtab->getColumn(2u), tmp_buf,
                   active_mi->rli.group_master_log_name,
                   strlen(active_mi->rli.group_master_log_name));
  r|= op->setValue(2u, tmp_buf);
  DBUG_ASSERT(r == 0);
  // start_pos
  r|= op->setValue(3u, (Uint64)active_mi->rli.group_master_log_pos);
  DBUG_ASSERT(r == 0);
  // end_pos
  r|= op->setValue(4u, (Uint64)active_mi->rli.group_master_log_pos + 
                   ((Uint64)active_mi->rli.future_event_relay_log_pos -
                    (Uint64)active_mi->rli.group_relay_log_pos));
  DBUG_ASSERT(r == 0);
  return 0;
}
#endif /* HAVE_NDB_BINLOG */

int ha_ndbcluster::external_lock(THD *thd, int lock_type)
{
  int error=0;
  NdbTransaction* trans= NULL;
  DBUG_ENTER("external_lock");

  /*
    Check that this handler instance has a connection
    set up to the Ndb object of thd
   */
  if (check_ndb_connection(thd))
    DBUG_RETURN(1);

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;

  DBUG_PRINT("enter", ("this: 0x%lx  thd: 0x%lx  thd_ndb: %lx  "
                       "thd_ndb->lock_count: %d",
                       (long) this, (long) thd, (long) thd_ndb,
                       thd_ndb->lock_count));

  if (lock_type != F_UNLCK)
  {
    DBUG_PRINT("info", ("lock_type != F_UNLCK"));
    if (thd->lex->sql_command == SQLCOM_LOAD)
    {
      m_transaction_on= FALSE;
      /* Would be simpler if has_transactions() didn't always say "yes" */
      thd->no_trans_update.all= thd->no_trans_update.stmt= TRUE;
    }
    else if (!thd->transaction.on)
      m_transaction_on= FALSE;
    else
      m_transaction_on= thd->variables.ndb_use_transactions;
    if (!thd_ndb->lock_count++)
    {
      PRINT_OPTION_FLAGS(thd);
      if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) 
      {
        // Autocommit transaction
        DBUG_ASSERT(!thd_ndb->stmt);
        DBUG_PRINT("trans",("Starting transaction stmt"));      

        trans= ndb->startTransaction();
        if (trans == NULL)
          ERR_RETURN(ndb->getNdbError());
        thd_ndb->init_open_tables();
        thd_ndb->stmt= trans;
	thd_ndb->query_state&= NDB_QUERY_NORMAL;
        thd_ndb->trans_options= 0;
        thd_ndb->m_slow_path= FALSE;
        if (thd->slave_thread ||
            !(thd->options & OPTION_BIN_LOG))
          thd_ndb->m_slow_path= TRUE;
        trans_register_ha(thd, FALSE, ndbcluster_hton);
      } 
      else 
      { 
        if (!thd_ndb->all)
        {
          // Not autocommit transaction
          // A "master" transaction ha not been started yet
          DBUG_PRINT("trans",("starting transaction, all"));
          
          trans= ndb->startTransaction();
          if (trans == NULL)
            ERR_RETURN(ndb->getNdbError());
          thd_ndb->init_open_tables();
          thd_ndb->all= trans; 
	  thd_ndb->query_state&= NDB_QUERY_NORMAL;
          thd_ndb->trans_options= 0;
          thd_ndb->m_slow_path= FALSE;
          if (thd->slave_thread ||
              !(thd->options & OPTION_BIN_LOG))
            thd_ndb->m_slow_path= TRUE;
          trans_register_ha(thd, TRUE, ndbcluster_hton);

          /*
            If this is the start of a LOCK TABLE, a table look 
            should be taken on the table in NDB
           
            Check if it should be read or write lock
           */
          if (thd->options & (OPTION_TABLE_LOCK))
          {
            //lockThisTable();
            DBUG_PRINT("info", ("Locking the table..." ));
          }

        }
      }
    }
    /*
      This is the place to make sure this handler instance
      has a started transaction.
     
      The transaction is started by the first handler on which 
      MySQL Server calls external lock
     
      Other handlers in the same stmt or transaction should use 
      the same NDB transaction. This is done by setting up the m_active_trans
      pointer to point to the NDB transaction. 
     */

    // store thread specific data first to set the right context
    m_force_send=          thd->variables.ndb_force_send;
    m_ha_not_exact_count= !thd->variables.ndb_use_exact_count;
    m_autoincrement_prefetch= 
      (ha_rows) thd->variables.ndb_autoincrement_prefetch_sz;

    m_active_trans= thd_ndb->all ? thd_ndb->all : thd_ndb->stmt;
    DBUG_ASSERT(m_active_trans);
    // Start of transaction
    m_rows_changed= 0;
    reset_state_at_execute();
    m_slow_path= thd_ndb->m_slow_path;
#ifdef HAVE_NDB_BINLOG
    if (unlikely(m_slow_path))
    {
      if (m_share == ndb_apply_status_share && thd->slave_thread)
        thd_ndb->trans_options|= TNTO_INJECTED_APPLY_STATUS;
    }
#endif
    // TODO remove double pointers...
    m_thd_ndb_share= thd_ndb->get_open_table(thd, m_table);
    m_table_info= &m_thd_ndb_share->stat;
  }
  else
  {
    DBUG_PRINT("info", ("lock_type == F_UNLCK"));

    if (ndb_cache_check_time && m_rows_changed)
    {
      DBUG_PRINT("info", ("Rows has changed and util thread is running"));
      if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
      {
        DBUG_PRINT("info", ("Add share to list of tables to be invalidated"));
        /* NOTE push_back allocates memory using transactions mem_root! */
        thd_ndb->changed_tables.push_back(m_share, &thd->transaction.mem_root);
      }

      pthread_mutex_lock(&m_share->mutex);
      DBUG_PRINT("info", ("Invalidating commit_count"));
      m_share->commit_count= 0;
      m_share->commit_count_lock++;
      pthread_mutex_unlock(&m_share->mutex);
    }

    if (!--thd_ndb->lock_count)
    {
      DBUG_PRINT("trans", ("Last external_lock"));
      PRINT_OPTION_FLAGS(thd);

      if (thd_ndb->stmt)
      {
        /*
          Unlock is done without a transaction commit / rollback.
          This happens if the thread didn't update any rows
          We must in this case close the transaction to release resources
        */
        DBUG_PRINT("trans",("ending non-updating transaction"));
        ndb->closeTransaction(m_active_trans);
        thd_ndb->stmt= NULL;
      }
    }
    m_table_info= NULL;

    /*
      This is the place to make sure this handler instance
      no longer are connected to the active transaction.

      And since the handler is no longer part of the transaction 
      it can't have open cursors, ops or blobs pending.
    */
    m_active_trans= NULL;    

    if (m_active_cursor)
      DBUG_PRINT("warning", ("m_active_cursor != NULL"));
    m_active_cursor= NULL;

    if (m_blobs_pending)
      DBUG_PRINT("warning", ("blobs_pending != 0"));
    m_blobs_pending= 0;
    
    if (m_ops_pending)
      DBUG_PRINT("warning", ("ops_pending != 0L"));
    m_ops_pending= 0;
  }
  thd->set_current_stmt_binlog_row_based_if_mixed();
  DBUG_RETURN(error);
}

/*
  Unlock the last row read in an open scan.
  Rows are unlocked by default in ndb, but
  for SELECT FOR UPDATE and SELECT LOCK WIT SHARE MODE
  locks are kept if unlock_row() is not called.
*/

void ha_ndbcluster::unlock_row() 
{
  DBUG_ENTER("unlock_row");

  DBUG_PRINT("info", ("Unlocking row"));
  m_lock_tuple= FALSE;
  DBUG_VOID_RETURN;
}

/*
  Start a transaction for running a statement if one is not
  already running in a transaction. This will be the case in
  a BEGIN; COMMIT; block
  When using LOCK TABLE's external_lock will start a transaction
  since ndb does not currently does not support table locking
*/

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type lock_type)
{
  int error=0;
  DBUG_ENTER("start_stmt");
  PRINT_OPTION_FLAGS(thd);

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= (thd_ndb->stmt)?thd_ndb->stmt:thd_ndb->all;
  if (!trans){
    Ndb *ndb= thd_ndb->ndb;
    DBUG_PRINT("trans",("Starting transaction stmt"));  
    trans= ndb->startTransaction();
    if (trans == NULL)
      ERR_RETURN(ndb->getNdbError());
    reset_state_at_execute();
    no_uncommitted_rows_reset(thd);
    thd_ndb->stmt= trans;
    thd_ndb->query_state&= NDB_QUERY_NORMAL;
    trans_register_ha(thd, FALSE, ndbcluster_hton);
  }
  m_active_trans= trans;
  // Start of statement
  reset_state_at_execute();
  thd->set_current_stmt_binlog_row_based_if_mixed();

  DBUG_RETURN(error);
}


/*
  Commit a transaction started in NDB
 */

static int ndbcluster_commit(handlerton *hton, THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= all ? thd_ndb->all : thd_ndb->stmt;

  DBUG_ENTER("ndbcluster_commit");
  DBUG_PRINT("transaction",("%s",
                            trans == thd_ndb->stmt ?
                            "stmt" : "all"));
  DBUG_ASSERT(ndb);
  if (trans == NULL)
    DBUG_RETURN(0);

#ifdef HAVE_NDB_BINLOG
  if (unlikely(thd_ndb->m_slow_path))
  {
    if (thd->slave_thread)
      ndbcluster_update_apply_status
        (thd, thd_ndb->trans_options & TNTO_INJECTED_APPLY_STATUS);
  }
#endif /* HAVE_NDB_BINLOG */

  if (thd->options & OPTION_ALLOW_BATCH)
  {
    res= execute_commit(trans, 1, TRUE);
  }
  else
  {
    res= execute_commit(thd,trans);
  }

  if (res != 0)
  {
    const NdbError err= trans->getNdbError();
    const NdbOperation *error_op= trans->getNdbErrorOperation();
    ERR_PRINT(err);
    res= ndb_to_mysql_error(&err);
    if (res != -1)
      ndbcluster_print_error(res, error_op);
  }
  ndb->closeTransaction(trans);

  if (all)
    thd_ndb->all= NULL;
  else
    thd_ndb->stmt= NULL;

  /* Clear commit_count for tables changed by transaction */
  NDB_SHARE* share;
  List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
  while ((share= it++))
  {
    pthread_mutex_lock(&share->mutex);
    DBUG_PRINT("info", ("Invalidate commit_count for %s, share->commit_count: %lu",
                        share->table_name, (ulong) share->commit_count));
    share->commit_count= 0;
    share->commit_count_lock++;
    pthread_mutex_unlock(&share->mutex);
  }
  thd_ndb->changed_tables.empty();

  DBUG_RETURN(res);
}


/*
  Rollback a transaction started in NDB
 */

static int ndbcluster_rollback(handlerton *hton, THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= all ? thd_ndb->all : thd_ndb->stmt;

  DBUG_ENTER("ndbcluster_rollback");
  DBUG_PRINT("transaction",("%s",
                            trans == thd_ndb->stmt ? 
                            "stmt" : "all"));
  DBUG_ASSERT(ndb && trans);

  if (trans->execute(NdbTransaction::Rollback) != 0)
  {
    const NdbError err= trans->getNdbError();
    const NdbOperation *error_op= trans->getNdbErrorOperation();
    ERR_PRINT(err);     
    res= ndb_to_mysql_error(&err);
    if (res != -1) 
      ndbcluster_print_error(res, error_op);
  }
  ndb->closeTransaction(trans);

  if (all)
    thd_ndb->all= NULL;
  else
    thd_ndb->stmt= NULL;

  /* Clear list of tables changed by transaction */
  thd_ndb->changed_tables.empty();

  DBUG_RETURN(res);
}


/*
  Define NDB column based on Field.
  Returns 0 or mysql error code.
  Not member of ha_ndbcluster because NDBCOL cannot be declared.

  MySQL text types with character set "binary" are mapped to true
  NDB binary types without a character set.

  Blobs are V2 and striping from mysql level is not supported
  due to lack of syntax and lack of support for partitioning.
 */

static bool
ndb_blob_striping()
{
#ifndef DBUG_OFF
  const char* p= getenv("NDB_BLOB_STRIPING");
  if (p != 0 && *p != 0 && *p != '0' && *p != 'n' && *p != 'N')
    return true;
#endif
  return false;
}

static int create_ndb_column(NDBCOL &col,
                             Field *field,
                             HA_CREATE_INFO *info)
{
  // Set name
  if (col.setName(field->field_name))
  {
    return (my_errno= errno);
  }
  // Get char set
  CHARSET_INFO *cs= field->charset();
  // Set type and sizes
  const enum enum_field_types mysql_type= field->real_type();
  switch (mysql_type) {
  // Numeric types
  case MYSQL_TYPE_TINY:        
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Tinyunsigned);
    else
      col.setType(NDBCOL::Tinyint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_SHORT:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Smallunsigned);
    else
      col.setType(NDBCOL::Smallint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Unsigned);
    else
      col.setType(NDBCOL::Int);
    col.setLength(1);
    break;
  case MYSQL_TYPE_INT24:       
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Mediumunsigned);
    else
      col.setType(NDBCOL::Mediumint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONGLONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Bigunsigned);
    else
      col.setType(NDBCOL::Bigint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_FLOAT:
    col.setType(NDBCOL::Float);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DOUBLE:
    col.setType(NDBCOL::Double);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DECIMAL:    
    {
      Field_decimal *f= (Field_decimal*)field;
      uint precision= f->pack_length();
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Olddecimalunsigned);
        precision-= (scale > 0);
      }
      else
      {
        col.setType(NDBCOL::Olddecimal);
        precision-= 1 + (scale > 0);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  case MYSQL_TYPE_NEWDECIMAL:    
    {
      Field_new_decimal *f= (Field_new_decimal*)field;
      uint precision= f->precision;
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Decimalunsigned);
      }
      else
      {
        col.setType(NDBCOL::Decimal);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  // Date types
  case MYSQL_TYPE_DATETIME:    
    col.setType(NDBCOL::Datetime);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DATE: // ?
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_NEWDATE:
    col.setType(NDBCOL::Date);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIME:        
    col.setType(NDBCOL::Time);
    col.setLength(1);
    break;
  case MYSQL_TYPE_YEAR:
    col.setType(NDBCOL::Year);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    col.setType(NDBCOL::Timestamp);
    col.setLength(1);
    break;
  // Char types
  case MYSQL_TYPE_STRING:      
    if (field->pack_length() == 0)
    {
      col.setType(NDBCOL::Bit);
      col.setLength(1);
    }
    else if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
    {
      col.setType(NDBCOL::Binary);
      col.setLength(field->pack_length());
    }
    else
    {
      col.setType(NDBCOL::Char);
      col.setCharset(cs);
      col.setLength(field->pack_length());
    }
    break;
  case MYSQL_TYPE_VAR_STRING: // ?
  case MYSQL_TYPE_VARCHAR:
    {
      Field_varstring* f= (Field_varstring*)field;
      if (f->length_bytes == 1)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Varbinary);
        else {
          col.setType(NDBCOL::Varchar);
          col.setCharset(cs);
        }
      }
      else if (f->length_bytes == 2)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Longvarbinary);
        else {
          col.setType(NDBCOL::Longvarchar);
          col.setCharset(cs);
        }
      }
      else
      {
        return HA_ERR_UNSUPPORTED;
      }
      col.setLength(field->field_length);
    }
    break;
  // Blob types (all come in as MYSQL_TYPE_BLOB)
  mysql_type_tiny_blob:
  case MYSQL_TYPE_TINY_BLOB:
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    // No parts
    col.setPartSize(0);
    col.setStripeSize(ndb_blob_striping() ? 0 : 0);
    break;
  //mysql_type_blob:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_BLOB:    
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    {
      Field_blob *field_blob= (Field_blob *)field;
      /*
       * max_data_length is 2^8-1, 2^16-1, 2^24-1 for tiny, blob, medium.
       * Tinyblob gets no blob parts.  The other cases are just a crude
       * way to control part size and striping.
       *
       * In mysql blob(256) is promoted to blob(65535) so it does not
       * in fact fit "inline" in NDB.
       */
      if (field_blob->max_data_length() < (1 << 8))
        goto mysql_type_tiny_blob;
      else if (field_blob->max_data_length() < (1 << 16))
      {
        col.setInlineSize(256);
        col.setPartSize(2000);
        col.setStripeSize(ndb_blob_striping() ? 16 : 0);
      }
      else if (field_blob->max_data_length() < (1 << 24))
        goto mysql_type_medium_blob;
      else
        goto mysql_type_long_blob;
    }
    break;
  mysql_type_medium_blob:
  case MYSQL_TYPE_MEDIUM_BLOB:   
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(4000);
    col.setStripeSize(ndb_blob_striping() ? 8 : 0);
    break;
  mysql_type_long_blob:
  case MYSQL_TYPE_LONG_BLOB:  
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(8000);
    col.setStripeSize(ndb_blob_striping() ? 4 : 0);
    break;
  // Other types
  case MYSQL_TYPE_ENUM:
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_SET:         
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_BIT:
  {
    int no_of_bits= field->field_length;
    col.setType(NDBCOL::Bit);
    if (!no_of_bits)
      col.setLength(1);
      else
        col.setLength(no_of_bits);
    break;
  }
  case MYSQL_TYPE_NULL:        
    goto mysql_type_unsupported;
  mysql_type_unsupported:
  default:
    return HA_ERR_UNSUPPORTED;
  }
  // Set nullable and pk
  col.setNullable(field->maybe_null());
  col.setPrimaryKey(field->flags & PRI_KEY_FLAG);
  // Set autoincrement
  if (field->flags & AUTO_INCREMENT_FLAG) 
  {
#ifndef DBUG_OFF
    char buff[22];
#endif
    col.setAutoIncrement(TRUE);
    ulonglong value= info->auto_increment_value ?
      info->auto_increment_value : (ulonglong) 1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %s", llstr(value, buff)));
    col.setAutoIncrementInitialValue(value);
  }
  else
    col.setAutoIncrement(FALSE);
  return 0;
}

/*
  Create a table in NDB Cluster
*/

int ha_ndbcluster::create(const char *name, 
                          TABLE *form, 
                          HA_CREATE_INFO *create_info)
{
  THD *thd= current_thd;
  NDBTAB tab;
  NDBCOL col;
  uint pack_length, length, i, pk_length= 0;
  const void *data= NULL, *pack_data= NULL;
  bool create_from_engine= (create_info->table_options & HA_OPTION_CREATE_FROM_ENGINE);
  bool is_truncate= (thd->lex->sql_command == SQLCOM_TRUNCATE);
  char tablespace[FN_LEN];
  NdbDictionary::Table::SingleUserMode single_user_mode= NdbDictionary::Table::SingleUserModeLocked;

  DBUG_ENTER("ha_ndbcluster::create");
  DBUG_PRINT("enter", ("name: %s", name));

  DBUG_ASSERT(*fn_rext((char*)name) == 0);
  set_dbname(name);
  set_tabname(name);

  if ((my_errno= check_ndb_connection()))
    DBUG_RETURN(my_errno);
  
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();

  if (is_truncate)
  {
    {
      Ndb_table_guard ndbtab_g(dict, m_tabname);
      if (!(m_table= ndbtab_g.get_table()))
	ERR_RETURN(dict->getNdbError());
      if ((get_tablespace_name(thd, tablespace, FN_LEN)))
	create_info->tablespace= tablespace;    
      m_table= NULL;
    }
    DBUG_PRINT("info", ("Dropping and re-creating table for TRUNCATE"));
    if ((my_errno= delete_table(name)))
      DBUG_RETURN(my_errno);
  }
  table= form;
  if (create_from_engine)
  {
    /*
      Table already exists in NDB and frm file has been created by 
      caller.
      Do Ndb specific stuff, such as create a .ndb file
    */
    if ((my_errno= write_ndb_file(name)))
      DBUG_RETURN(my_errno);
#ifdef HAVE_NDB_BINLOG
    ndbcluster_create_binlog_setup(get_ndb(), name, strlen(name),
                                   m_dbname, m_tabname, FALSE);
#endif /* HAVE_NDB_BINLOG */
    DBUG_RETURN(my_errno);
  }

#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow table creation unless
    schema distribution table is setup
    ( unless it is a creation of the schema dist table itself )
  */
  if (!ndb_schema_share)
  {
    if (!(strcmp(m_dbname, NDB_REP_DB) == 0 &&
          strcmp(m_tabname, NDB_SCHEMA_TABLE) == 0))
    {
      DBUG_PRINT("info", ("Schema distribution table not setup"));
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
    single_user_mode = NdbDictionary::Table::SingleUserModeReadWrite;
  }
#endif /* HAVE_NDB_BINLOG */

  DBUG_PRINT("table", ("name: %s", m_tabname));  
  if (tab.setName(m_tabname))
  {
    DBUG_RETURN(my_errno= errno);
  }
  tab.setLogging(!(create_info->options & HA_LEX_CREATE_TMP_TABLE));    
  tab.setSingleUserMode(single_user_mode);

  // Save frm data for this table
  if (readfrm(name, &data, &length))
    DBUG_RETURN(1);
  if (packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(0));
    DBUG_RETURN(2);
  }
  DBUG_PRINT("info", ("setFrm data: 0x%lx  len: %d", (long) pack_data, pack_length));
  tab.setFrm(pack_data, pack_length);      
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));
  
  /*
    Check for disk options
  */
  if (create_info->storage_media == HA_SM_DISK)
  { 
    if (create_info->tablespace)
      tab.setTablespaceName(create_info->tablespace);
    else
      tab.setTablespaceName("DEFAULT-TS");
  }
  else if (create_info->tablespace)
  {
    if (create_info->storage_media == HA_SM_MEMORY)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			  ER_ILLEGAL_HA_CREATE_OPTION,
			  ER(ER_ILLEGAL_HA_CREATE_OPTION),
			  ndbcluster_hton_name,
			  "TABLESPACE currently only supported for "
			  "STORAGE DISK");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
    tab.setTablespaceName(create_info->tablespace);
    create_info->storage_media = HA_SM_DISK;  //if use tablespace, that also means store on disk
  }

  /*
    Handle table row type

    Default is to let table rows have var part reference so that online 
    add column can be performed in the future.  Explicitly setting row 
    type to fixed will omit var part reference, which will save data 
    memory in ndb, but at the cost of not being able to online add 
    column to this table
  */
  switch (create_info->row_type) {
  case ROW_TYPE_FIXED:
    tab.setForceVarPart(FALSE);
    break;
  case ROW_TYPE_DYNAMIC:
    /* fall through, treat as default */
  default:
    /* fall through, treat as default */
  case ROW_TYPE_DEFAULT:
    tab.setForceVarPart(TRUE);
    break;
  }

  /*
    Setup columns
  */
  for (i= 0; i < form->s->fields; i++) 
  {
    Field *field= form->field[i];
    DBUG_PRINT("info", ("name: %s  type: %u  pack_length: %d", 
                        field->field_name, field->real_type(),
                        field->pack_length()));
    if ((my_errno= create_ndb_column(col, field, create_info)))
      DBUG_RETURN(my_errno);
 
    if (create_info->storage_media == HA_SM_DISK)
      col.setStorageType(NdbDictionary::Column::StorageTypeDisk);
    else
      col.setStorageType(NdbDictionary::Column::StorageTypeMemory);

    switch (create_info->row_type) {
    case ROW_TYPE_FIXED:
      if (field_type_forces_var_part(field->type()))
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            ER(ER_ILLEGAL_HA_CREATE_OPTION),
                            ndbcluster_hton_name,
                            "Row format FIXED incompatible with "
                            "variable sized attribute");
        DBUG_RETURN(HA_ERR_UNSUPPORTED);
      }
      break;
    case ROW_TYPE_DYNAMIC:
      /*
        Future: make columns dynamic in this case
      */
      break;
    default:
      break;
    }
    if (tab.addColumn(col))
    {
      DBUG_RETURN(my_errno= errno);
    }
    if (col.getPrimaryKey())
      pk_length += (field->pack_length() + 3) / 4;
  }

  KEY* key_info;
  for (i= 0, key_info= form->key_info; i < form->s->keys; i++, key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    KEY_PART_INFO *end= key_part + key_info->key_parts;
    for (; key_part != end; key_part++)
      tab.getColumn(key_part->fieldnr-1)->setStorageType(
                             NdbDictionary::Column::StorageTypeMemory);
  }

  // No primary key, create shadow key as 64 bit, auto increment  
  if (form->s->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Generating shadow key"));
    if (col.setName("$PK"))
    {
      DBUG_RETURN(my_errno= errno);
    }
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(FALSE);
    col.setPrimaryKey(TRUE);
    col.setAutoIncrement(TRUE);
    if (tab.addColumn(col))
    {
      DBUG_RETURN(my_errno= errno);
    }
    pk_length += 2;
  }
 
  // Make sure that blob tables don't have too big part size
  for (i= 0; i < form->s->fields; i++) 
  {
    /**
     * The extra +7 concists
     * 2 - words from pk in blob table
     * 5 - from extra words added by tup/dict??
     */
    switch (form->field[i]->real_type()) {
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:    
    case MYSQL_TYPE_MEDIUM_BLOB:   
    case MYSQL_TYPE_LONG_BLOB: 
    {
      NdbDictionary::Column * column= tab.getColumn(i);
      int size= pk_length + (column->getPartSize()+3)/4 + 7;
      if (size > NDB_MAX_TUPLE_SIZE_IN_WORDS && 
         (pk_length+7) < NDB_MAX_TUPLE_SIZE_IN_WORDS)
      {
        size= NDB_MAX_TUPLE_SIZE_IN_WORDS - pk_length - 7;
        column->setPartSize(4*size);
      }
      /**
       * If size > NDB_MAX and pk_length+7 >= NDB_MAX
       *   then the table can't be created anyway, so skip
       *   changing part size, and have error later
       */ 
    }
    default:
      break;
    }
  }

  // Check partition info
  partition_info *part_info= form->part_info;
  if ((my_errno= set_up_partition_info(part_info, form, (void*)&tab)))
  {
    DBUG_RETURN(my_errno);
  }

  // Create the table in NDB     
  if (dict->createTable(tab) != 0) 
  {
    const NdbError err= dict->getNdbError();
    ERR_PRINT(err);
    my_errno= ndb_to_mysql_error(&err);
    DBUG_RETURN(my_errno);
  }

  Ndb_table_guard ndbtab_g(dict, m_tabname);
  // temporary set m_table during create
  // reset at return
  m_table= ndbtab_g.get_table();
  // TODO check also that we have the same frm...
  if (!m_table)
  {
    /* purecov: begin deadcode */
    const NdbError err= dict->getNdbError();
    ERR_PRINT(err);
    my_errno= ndb_to_mysql_error(&err);
    DBUG_RETURN(my_errno);
    /* purecov: end */
  }

  DBUG_PRINT("info", ("Table %s/%s created successfully", 
                      m_dbname, m_tabname));

  // Create secondary indexes
  my_errno= create_indexes(ndb, form);

  if (!my_errno)
    my_errno= write_ndb_file(name);
  else
  {
    /*
      Failed to create an index,
      drop the table (and all it's indexes)
    */
    while (dict->dropTableGlobal(*m_table))
    {
      switch (dict->getNdbError().status)
      {
        case NdbError::TemporaryError:
          if (!thd->killed) 
            continue; // retry indefinitly
          break;
        default:
          break;
      }
      break;
    }
    m_table = 0;
    DBUG_RETURN(my_errno);
  }

#ifdef HAVE_NDB_BINLOG
  if (!my_errno)
  {
    NDB_SHARE *share= 0;
    pthread_mutex_lock(&ndbcluster_mutex);
    /*
      First make sure we get a "fresh" share here, not an old trailing one...
    */
    {
      uint length= (uint) strlen(name);
      if ((share= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                           (byte*) name, length)))
        handle_trailing_share(share);
    }
    /*
      get a new share
    */

    /* ndb_share reference create */
    if (!(share= get_share(name, form, TRUE, TRUE)))
    {
      sql_print_error("NDB: allocating table share for %s failed", name);
      /* my_errno is set */
    }
    else
    {
      DBUG_PRINT("NDB_SHARE", ("%s binlog create  use_count: %u",
                               share->key, share->use_count));
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    while (!IS_TMP_PREFIX(m_tabname))
    {
      String event_name(INJECTOR_EVENT_LEN);
      ndb_rep_event_name(&event_name,m_dbname,m_tabname);
      int do_event_op= ndb_binlog_running;

      if (!ndb_schema_share &&
          strcmp(share->db, NDB_REP_DB) == 0 &&
          strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
        do_event_op= 1;

      /*
        Always create an event for the table, as other mysql servers
        expect it to be there.
      */
      if (!ndbcluster_create_event(ndb, m_table, event_name.c_ptr(), share,
                                   share && do_event_op ? 2 : 1/* push warning */))
      {
        if (ndb_extra_logging)
          sql_print_information("NDB Binlog: CREATE TABLE Event: %s",
                                event_name.c_ptr());
        if (share && 
            ndbcluster_create_event_ops(share, m_table, event_name.c_ptr()))
        {
          sql_print_error("NDB Binlog: FAILED CREATE TABLE event operations."
                          " Event: %s", name);
          /* a warning has been issued to the client */
        }
      }
      /*
        warning has been issued if ndbcluster_create_event failed
        and (share && do_event_op)
      */
      if (share && !do_event_op)
        share->flags|= NSF_NO_BINLOG;
      ndbcluster_log_schema_op(thd, share,
                               thd->query, thd->query_length,
                               share->db, share->table_name,
                               m_table->getObjectId(),
                               m_table->getObjectVersion(),
                               (is_truncate) ?
			       SOT_TRUNCATE_TABLE : SOT_CREATE_TABLE, 
			       0, 0, 1);
      break;
    }
  }
#endif /* HAVE_NDB_BINLOG */

  m_table= 0;
  DBUG_RETURN(my_errno);
}

int ha_ndbcluster::create_handler_files(const char *file,
                                        const char *old_name,
                                        int action_flag,
                                        HA_CREATE_INFO *create_info)
{ 
  Ndb* ndb;
  const NDBTAB *tab;
  const void *data= NULL, *pack_data= NULL;
  uint length, pack_length;
  int error= 0;

  DBUG_ENTER("create_handler_files");

  if (action_flag != CHF_INDEX_FLAG)
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("enter", ("file: %s", file));
  if (!(ndb= get_ndb()))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  NDBDICT *dict= ndb->getDictionary();
  if (!create_info->frm_only)
    DBUG_RETURN(0); // Must be a create, ignore since frm is saved in create

  // TODO handle this
  DBUG_ASSERT(m_table != 0);

  set_dbname(file);
  set_tabname(file);
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  DBUG_PRINT("info", ("m_dbname: %s, m_tabname: %s", m_dbname, m_tabname));
  if (!(tab= ndbtab_g.get_table()))
    DBUG_RETURN(0); // Unkown table, must be temporary table

  DBUG_ASSERT(get_ndb_share_state(m_share) == NSS_ALTERED);
  if (readfrm(file, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    DBUG_PRINT("info", ("Missing frm for %s", m_tabname));
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    error= 1;
  }
  else
  {
    DBUG_PRINT("info", ("Table %s has changed, altering frm in ndb", 
                        m_tabname));
    NdbDictionary::Table new_tab= *tab;
    new_tab.setFrm(pack_data, pack_length);
    if (dict->alterTableGlobal(*tab, new_tab))
    {
      error= ndb_to_mysql_error(&dict->getNdbError());
    }
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
  }
  
  set_ndb_share_state(m_share, NSS_INITIAL);
  /* ndb_share reference schema(?) free */
  DBUG_PRINT("NDB_SHARE", ("%s binlog schema(?) free  use_count: %u",
                           m_share->key, m_share->use_count));
  free_share(&m_share); // Decrease ref_count

  DBUG_RETURN(error);
}

int ha_ndbcluster::create_index(const char *name, KEY *key_info, 
                                NDB_INDEX_TYPE idx_type, uint idx_no)
{
  int error= 0;
  char unique_name[FN_LEN];
  static const char* unique_suffix= "$unique";
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_PRINT("info", ("Creating index %u: %s", idx_no, name));  

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    strxnmov(unique_name, FN_LEN, name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Created unique index name \'%s\' for index %d",
                        unique_name, idx_no));
  }
    
  switch (idx_type){
  case PRIMARY_KEY_INDEX:
    // Do nothing, already created
    break;
  case PRIMARY_KEY_ORDERED_INDEX:
    error= create_ordered_index(name, key_info);
    break;
  case UNIQUE_ORDERED_INDEX:
    if (!(error= create_ordered_index(name, key_info)))
      error= create_unique_index(unique_name, key_info);
    break;
  case UNIQUE_INDEX:
    if (check_index_fields_not_null(key_info))
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			  ER_NULL_COLUMN_IN_INDEX,
			  "Ndb does not support unique index on NULL valued attributes, index access with NULL value will become full table scan");
    }
    error= create_unique_index(unique_name, key_info);
    break;
  case ORDERED_INDEX:
    if (key_info->algorithm == HA_KEY_ALG_HASH)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			  ER_ILLEGAL_HA_CREATE_OPTION,
			  ER(ER_ILLEGAL_HA_CREATE_OPTION),
			  ndbcluster_hton_name,
			  "Ndb does not support non-unique "
			  "hash based indexes");
      error= HA_ERR_UNSUPPORTED;
      break;
    }
    error= create_ordered_index(name, key_info);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }
  
  DBUG_RETURN(error);
}

int ha_ndbcluster::create_ordered_index(const char *name, 
                                        KEY *key_info)
{
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_RETURN(create_ndb_index(name, key_info, FALSE));
}

int ha_ndbcluster::create_unique_index(const char *name, 
                                       KEY *key_info)
{

  DBUG_ENTER("ha_ndbcluster::create_unique_index");
  DBUG_RETURN(create_ndb_index(name, key_info, TRUE));
}


/*
  Create an index in NDB Cluster
 */

int ha_ndbcluster::create_ndb_index(const char *name, 
                                     KEY *key_info,
                                     bool unique)
{
  Ndb *ndb= get_ndb();
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part + key_info->key_parts;
  
  DBUG_ENTER("ha_ndbcluster::create_index");
  DBUG_PRINT("enter", ("name: %s ", name));

  NdbDictionary::Index ndb_index(name);
  if (unique)
    ndb_index.setType(NdbDictionary::Index::UniqueHashIndex);
  else 
  {
    ndb_index.setType(NdbDictionary::Index::OrderedIndex);
    // TODO Only temporary ordered indexes supported
    ndb_index.setLogging(FALSE); 
  }
  if (ndb_index.setTable(m_tabname))
  {
    DBUG_RETURN(my_errno= errno);
  }

  for (; key_part != end; key_part++) 
  {
    Field *field= key_part->field;
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    if (ndb_index.addColumnName(field->field_name))
    {
      DBUG_RETURN(my_errno= errno);
    }
  }
  
  if (dict->createIndex(ndb_index, *m_table))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}

/*
 Prepare for an on-line alter table
*/ 
void ha_ndbcluster::prepare_for_alter()
{
  /* ndb_share reference schema */
  ndbcluster_get_share(m_share); // Increase ref_count
  DBUG_PRINT("NDB_SHARE", ("%s binlog schema  use_count: %u",
                           m_share->key, m_share->use_count));
  set_ndb_share_state(m_share, NSS_ALTERED);
}

/*
  Add an index on-line to a table
*/
int ha_ndbcluster::add_index(TABLE *table_arg, 
                             KEY *key_info, uint num_of_keys)
{
  int error= 0;
  uint idx;
  DBUG_ENTER("ha_ndbcluster::add_index");
  DBUG_PRINT("enter", ("table %s", table_arg->s->table_name.str));
  DBUG_ASSERT(m_share->state == NSS_ALTERED);

  for (idx= 0; idx < num_of_keys; idx++)
  {
    KEY *key= key_info + idx;
    KEY_PART_INFO *key_part= key->key_part;
    KEY_PART_INFO *end= key_part + key->key_parts;
    NDB_INDEX_TYPE idx_type= get_index_type_from_key(idx, key_info, false);
    DBUG_PRINT("info", ("Adding index: '%s'", key_info[idx].name));
    // Add fields to key_part struct
    for (; key_part != end; key_part++)
      key_part->field= table->field[key_part->fieldnr];
    // Check index type
    // Create index in ndb
    if((error= create_index(key_info[idx].name, key, idx_type, idx)))
      break;
  }
  if (error)
  {
    set_ndb_share_state(m_share, NSS_INITIAL);
    /* ndb_share reference schema free */
    DBUG_PRINT("NDB_SHARE", ("%s binlog schema free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share); // Decrease ref_count
  }
  DBUG_RETURN(error);  
}

/*
  Mark one or several indexes for deletion. and
  renumber the remaining indexes
*/
int ha_ndbcluster::prepare_drop_index(TABLE *table_arg, 
                                      uint *key_num, uint num_of_keys)
{
  DBUG_ENTER("ha_ndbcluster::prepare_drop_index");
  DBUG_ASSERT(m_share->state == NSS_ALTERED);
  // Mark indexes for deletion
  uint idx;
  for (idx= 0; idx < num_of_keys; idx++)
  {
    DBUG_PRINT("info", ("ha_ndbcluster::prepare_drop_index %u", *key_num));
    m_index[*key_num++].status= TO_BE_DROPPED;
  }
  // Renumber indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  renumber_indexes(ndb, table_arg);
  DBUG_RETURN(0);
}
 
/*
  Really drop all indexes marked for deletion
*/
int ha_ndbcluster::final_drop_index(TABLE *table_arg)
{
  int error;
  DBUG_ENTER("ha_ndbcluster::final_drop_index");
  DBUG_PRINT("info", ("ha_ndbcluster::final_drop_index"));
  // Really drop indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  if((error= drop_indexes(ndb, table_arg)))
  {
    m_share->state= NSS_INITIAL;
    /* ndb_share reference schema free */
    DBUG_PRINT("NDB_SHARE", ("%s binlog schema free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share); // Decrease ref_count
  }
  DBUG_RETURN(error);
}

/*
  Rename a table in NDB Cluster
*/

int ha_ndbcluster::rename_table(const char *from, const char *to)
{
  NDBDICT *dict;
  char old_dbname[FN_HEADLEN];
  char new_dbname[FN_HEADLEN];
  char new_tabname[FN_HEADLEN];
  const NDBTAB *orig_tab;
  int result;
  bool recreate_indexes= FALSE;
  NDBDICT::List index_list;

  DBUG_ENTER("ha_ndbcluster::rename_table");
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));
  set_dbname(from, old_dbname);
  set_dbname(to, new_dbname);
  set_tabname(from);
  set_tabname(to, new_tabname);

  if (check_ndb_connection())
    DBUG_RETURN(my_errno= HA_ERR_NO_CONNECTION);

  Ndb *ndb= get_ndb();
  ndb->setDatabaseName(old_dbname);
  dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(orig_tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

#ifdef HAVE_NDB_BINLOG
  int ndb_table_id= orig_tab->getObjectId();
  int ndb_table_version= orig_tab->getObjectVersion();

  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(from, 0, FALSE);
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
    IF_DBUG(int r=) rename_share(share, to);
    DBUG_ASSERT(r == 0);
  }
#endif
  if (my_strcasecmp(system_charset_info, new_dbname, old_dbname))
  {
    dict->listIndexes(index_list, *orig_tab);    
    recreate_indexes= TRUE;
  }
  // Change current database to that of target table
  set_dbname(to);
  if (ndb->setDatabaseName(m_dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }

  NdbDictionary::Table new_tab= *orig_tab;
  new_tab.setName(new_tabname);
  if (dict->alterTableGlobal(*orig_tab, new_tab) != 0)
  {
    NdbError ndb_error= dict->getNdbError();
#ifdef HAVE_NDB_BINLOG
    if (share)
    {
      IF_DBUG(int ret=) rename_share(share, from);
      DBUG_ASSERT(ret == 0);
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
    }
#endif
    ERR_RETURN(ndb_error);
  }
  
  // Rename .ndb file
  if ((result= handler::rename_table(from, to)))
  {
    // ToDo in 4.1 should rollback alter table...
#ifdef HAVE_NDB_BINLOG
    if (share)
    {
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
    }
#endif
    DBUG_RETURN(result);
  }

#ifdef HAVE_NDB_BINLOG
  int is_old_table_tmpfile= 1;
  if (share && share->op)
    dict->forceGCPWait();

  /* handle old table */
  if (!IS_TMP_PREFIX(m_tabname))
  {
    is_old_table_tmpfile= 0;
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, from + sizeof(share_prefix) - 1, 0);
    ndbcluster_handle_drop_table(ndb, event_name.c_ptr(), share,
                                 "rename table");
  }

  if (!result && !IS_TMP_PREFIX(new_tabname))
  {
    /* always create an event for the table */
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, to + sizeof(share_prefix) - 1, 0);
    Ndb_table_guard ndbtab_g2(dict, new_tabname);
    const NDBTAB *ndbtab= ndbtab_g2.get_table();

    if (!ndbcluster_create_event(ndb, ndbtab, event_name.c_ptr(), share,
                                 share && ndb_binlog_running ? 2 : 1/* push warning */))
    {
      if (ndb_extra_logging)
        sql_print_information("NDB Binlog: RENAME Event: %s",
                              event_name.c_ptr());
      if (share &&
          ndbcluster_create_event_ops(share, ndbtab, event_name.c_ptr()))
      {
        sql_print_error("NDB Binlog: FAILED create event operations "
                        "during RENAME. Event %s", event_name.c_ptr());
        /* a warning has been issued to the client */
      }
    }
    /*
      warning has been issued if ndbcluster_create_event failed
      and (share && ndb_binlog_running)
    */
    if (!is_old_table_tmpfile)
      ndbcluster_log_schema_op(current_thd, share,
                               current_thd->query, current_thd->query_length,
                               old_dbname, m_tabname,
                               ndb_table_id, ndb_table_version,
                               SOT_RENAME_TABLE,
                               m_dbname, new_tabname, 1);
  }

  // If we are moving tables between databases, we need to recreate
  // indexes
  if (recreate_indexes)
  {
    for (unsigned i = 0; i < index_list.count; i++) 
    {
        NDBDICT::List::Element& index_el = index_list.elements[i];
	// Recreate any indexes not stored in the system database
	if (my_strcasecmp(system_charset_info, 
			  index_el.database, NDB_SYSTEM_DATABASE))
	{
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  const NDBINDEX * index= dict->getIndexGlobal(index_el.name,  new_tab);
	  DBUG_PRINT("info", ("Creating index %s/%s",
			      index_el.database, index->getName()));
	  dict->createIndex(*index, new_tab);
	  DBUG_PRINT("info", ("Dropping index %s/%s",
			      index_el.database, index->getName()));
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  dict->dropIndexGlobal(*index);
	}
    }
  }
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }
#endif

  DBUG_RETURN(result);
}


/*
  Delete table from NDB Cluster

 */

/* static version which does not need a handler */

int
ha_ndbcluster::delete_table(ha_ndbcluster *h, Ndb *ndb,
                            const char *path,
                            const char *db,
                            const char *table_name)
{
  THD *thd= current_thd;
  DBUG_ENTER("ha_ndbcluster::ndbcluster_delete_table");
  NDBDICT *dict= ndb->getDictionary();
  int ndb_table_id= 0;
  int ndb_table_version= 0;
#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop table unless
    schema distribution table is setup
  */
  if (!ndb_schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(path, 0, FALSE);
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
  }
#endif

  /* Drop the table from NDB */
  
  int res= 0;
  if (h && h->m_table)
  {
retry_temporary_error1:
    if (dict->dropTableGlobal(*h->m_table) == 0)
    {
      ndb_table_id= h->m_table->getObjectId();
      ndb_table_version= h->m_table->getObjectVersion();
      DBUG_PRINT("info", ("success 1"));
    }
    else
    {
      switch (dict->getNdbError().status)
      {
        case NdbError::TemporaryError:
          if (!thd->killed) 
            goto retry_temporary_error1; // retry indefinitly
          break;
        default:
          break;
      }
      res= ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info", ("error(1) %u", res));
    }
    h->release_metadata(thd, ndb);
  }
  else
  {
    ndb->setDatabaseName(db);
    while (1)
    {
      Ndb_table_guard ndbtab_g(dict, table_name);
      if (ndbtab_g.get_table())
      {
    retry_temporary_error2:
        if (dict->dropTableGlobal(*ndbtab_g.get_table()) == 0)
        {
          ndb_table_id= ndbtab_g.get_table()->getObjectId();
          ndb_table_version= ndbtab_g.get_table()->getObjectVersion();
          DBUG_PRINT("info", ("success 2"));
          break;
        }
        else
        {
          switch (dict->getNdbError().status)
          {
            case NdbError::TemporaryError:
              if (!thd->killed) 
                goto retry_temporary_error2; // retry indefinitly
              break;
            default:
              if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT)
              {
                ndbtab_g.invalidate();
                continue;
              }
              break;
          }
        }
      }
      res= ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info", ("error(2) %u", res));
      break;
    }
  }

  if (res)
  {
#ifdef HAVE_NDB_BINLOG
    /* the drop table failed for some reason, drop the share anyways */
    if (share)
    {
      pthread_mutex_lock(&ndbcluster_mutex);
      if (share->state != NSS_DROPPED)
      {
        /*
          The share kept by the server has not been freed, free it
        */
        share->state= NSS_DROPPED;
        /* ndb_share reference create free */
        DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                                 share->key, share->use_count));
        free_share(&share, TRUE);
      }
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share, TRUE);
      pthread_mutex_unlock(&ndbcluster_mutex);
    }
#endif
    DBUG_RETURN(res);
  }

#ifdef HAVE_NDB_BINLOG
  /* stop the logging of the dropped table, and cleanup */

  /*
    drop table is successful even if table does not exist in ndb
    and in case table was actually not dropped, there is no need
    to force a gcp, and setting the event_name to null will indicate
    that there is no event to be dropped
  */
  int table_dropped= dict->getNdbError().code != 709;

  if (!IS_TMP_PREFIX(table_name) && share &&
      current_thd->lex->sql_command != SQLCOM_TRUNCATE)
  {
    ndbcluster_log_schema_op(thd, share,
                             thd->query, thd->query_length,
                             share->db, share->table_name,
                             ndb_table_id, ndb_table_version,
                             SOT_DROP_TABLE, 0, 0, 1);
  }
  else if (table_dropped && share && share->op) /* ndbcluster_log_schema_op
                                                   will do a force GCP */
    dict->forceGCPWait();

  if (!IS_TMP_PREFIX(table_name))
  {
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, path + sizeof(share_prefix) - 1, 0);
    ndbcluster_handle_drop_table(ndb,
                                 table_dropped ? event_name.c_ptr() : 0,
                                 share, "delete table");
  }

  if (share)
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    if (share->state != NSS_DROPPED)
    {
      /*
        The share kept by the server has not been freed, free it
      */
      share->state= NSS_DROPPED;
      /* ndb_share reference create free */
      DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share, TRUE);
    }
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share, TRUE);
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
#endif
  DBUG_RETURN(0);
}

int ha_ndbcluster::delete_table(const char *name)
{
  DBUG_ENTER("ha_ndbcluster::delete_table");
  DBUG_PRINT("enter", ("name: %s", name));
  set_dbname(name);
  set_tabname(name);

#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop table unless
    schema distribution table is setup
  */
  if (!ndb_schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
#endif

  if (check_ndb_connection())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  /* Call ancestor function to delete .ndb file */
  handler::delete_table(name);

  DBUG_RETURN(delete_table(this, get_ndb(),name, m_dbname, m_tabname));
}


void ha_ndbcluster::get_auto_increment(ulonglong offset, ulonglong increment,
                                       ulonglong nb_desired_values,
                                       ulonglong *first_value,
                                       ulonglong *nb_reserved_values)
{  
  int cache_size;
  Uint64 auto_value;
  DBUG_ENTER("get_auto_increment");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));
  Ndb *ndb= get_ndb();
   
  if (m_rows_inserted > m_rows_to_insert)
  {
    /* We guessed too low */
    m_rows_to_insert+= m_autoincrement_prefetch;
  }
  cache_size= 
    (int) ((m_rows_to_insert - m_rows_inserted < m_autoincrement_prefetch) ?
           m_rows_to_insert - m_rows_inserted :
           ((m_rows_to_insert > m_autoincrement_prefetch) ?
            m_rows_to_insert : m_autoincrement_prefetch));
  uint retries= NDB_AUTO_INCREMENT_RETRIES;
  int retry_sleep= 30; /* 30 milliseconds, transaction */
  for (;;)
  {
    Ndb_tuple_id_range_guard g(m_share);
    if (m_skip_auto_increment &&
        ndb->readAutoIncrementValue(m_table, g.range, auto_value) ||
        ndb->getAutoIncrementValue(m_table, g.range, auto_value, cache_size))
    {
      if (--retries &&
          ndb->getNdbError().status == NdbError::TemporaryError);
      {
        my_sleep(retry_sleep);
        continue;
      }
      const NdbError err= ndb->getNdbError();
      sql_print_error("Error %lu in ::get_auto_increment(): %s",
                      (ulong) err.code, err.message);
      *first_value= ~(ulonglong) 0;
      DBUG_VOID_RETURN;
    }
    break;
  }
  *first_value= (longlong)auto_value;
  /* From the point of view of MySQL, NDB reserves one row at a time */
  *nb_reserved_values= 1;
  DBUG_VOID_RETURN;
}


/*
  Constructor for the NDB Cluster table handler 
 */

#define HA_NDBCLUSTER_TABLE_FLAGS \
                HA_REC_NOT_IN_SEQ | \
                HA_NULL_IN_KEY | \
                HA_AUTO_PART_KEY | \
                HA_NO_PREFIX_CHAR_KEYS | \
                HA_NEED_READ_RANGE_BUFFER | \
                HA_CAN_GEOMETRY | \
                HA_CAN_BIT_FIELD | \
                HA_PRIMARY_KEY_REQUIRED_FOR_POSITION | \
                HA_PRIMARY_KEY_REQUIRED_FOR_DELETE | \
                HA_PARTIAL_COLUMN_READ | \
                HA_HAS_OWN_BINLOGGING | \
                HA_HAS_RECORDS

ha_ndbcluster::ha_ndbcluster(handlerton *hton, TABLE_SHARE *table_arg):
  handler(hton, table_arg),
  m_active_trans(NULL),
  m_active_cursor(NULL),
  m_table(NULL),
  m_table_info(NULL),
  m_table_flags(HA_NDBCLUSTER_TABLE_FLAGS),
  m_share(0),
  m_part_info(NULL),
  m_use_partition_function(FALSE),
  m_sorted(FALSE),
  m_use_write(FALSE),
  m_ignore_dup_key(FALSE),
  m_has_unique_index(FALSE),
  m_ignore_no_key(FALSE),
  m_rows_to_insert((ha_rows) 1),
  m_rows_inserted((ha_rows) 0),
  m_rows_changed((ha_rows) 0),
  m_delete_cannot_batch(FALSE),
  m_update_cannot_batch(FALSE),
  m_ops_pending(0),
  m_skip_auto_increment(TRUE),
  m_row_buffer_current(NULL),
  m_blobs_pending(0),
  m_blobs_buffer(0),
  m_blobs_buffer_size(0),
  m_row_buffer(0),
  m_row_buffer_size(0),
  m_extra_reclength(0),
  m_ndb_record(0),
  m_ndb_record_fragment(0),
  m_ndb_hidden_key_record(0),
  m_ndb_statistics_record(0),
  m_dupkey((uint) -1),
  m_ha_not_exact_count(FALSE),
  m_force_send(TRUE),
  m_autoincrement_prefetch((ha_rows) 32),
  m_transaction_on(TRUE),
  m_cond(NULL)
{
  int i;
 
  DBUG_ENTER("ha_ndbcluster");

  m_tabname[0]= '\0';
  m_dbname[0]= '\0';

  stats.records= ~(ha_rows)0; // uninitialized
  stats.block_size= 1024;

  for (i= 0; i < MAX_KEY; i++)
    ndb_init_index(m_index[i]);

  DBUG_VOID_RETURN;
}


int ha_ndbcluster::ha_initialise()
{
  DBUG_ENTER("ha_ndbcluster::ha_initialise");
  if (check_ndb_in_thd(current_thd))
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_RETURN(TRUE);
}

/*
  Destructor for NDB Cluster table handler
 */

ha_ndbcluster::~ha_ndbcluster() 
{
  THD *thd= current_thd;
  Ndb *ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
  DBUG_ENTER("~ha_ndbcluster");

  if (m_share)
  {
    /* ndb_share reference handler free */
    DBUG_PRINT("NDB_SHARE", ("%s handler free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share);
  }
  release_metadata(thd, ndb);
  my_free(m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
  m_blobs_buffer= 0;

  my_free(m_row_buffer, MYF(MY_ALLOW_ZERO_PTR));
  m_row_buffer= 0;
  m_row_buffer_current= 0;
  m_row_buffer_size= 0;    

  // Check for open cursor/transaction
  if (m_active_cursor) {
  }
  DBUG_ASSERT(m_active_cursor == NULL);
  if (m_active_trans) {
  }
  DBUG_ASSERT(m_active_trans == NULL);

  // Discard any generated condition
  DBUG_PRINT("info", ("Deleting generated condition"));
  if (m_cond)
  {
    delete m_cond;
    m_cond= NULL;
  }

  DBUG_VOID_RETURN;
}



void
ha_ndbcluster::column_bitmaps_signal()
{
  DBUG_ENTER("ha_ndbcluster::column_bitmaps_signal");
  /*
    We need to make sure we always read all of the primary key.
    Otherwise we cannot support position() and rnd_pos().
  */
  bitmap_union(table->read_set, &m_pk_bitmap);
  DBUG_VOID_RETURN;

  /*
    Alternatively, we could just set a flag, and in the reader methods set the
    extra bits as required if the flag is set, followed by clearing the flag.
    This to save doing the work of setting bits twice or more.
    On the other hand this is quite fast in itself.
  */
}

/*
  Open a table for further use
  - fetch metadata for this table from NDB
  - check that table exists

  RETURN
    0    ok
    < 0  Table has changed
*/

int ha_ndbcluster::open(const char *name, int mode, uint test_if_locked)
{
  int res;
  KEY *key;
  DBUG_ENTER("ha_ndbcluster::open");
  DBUG_PRINT("enter", ("name: %s  mode: %d  test_if_locked: %d",
                       name, mode, test_if_locked));
  
  /*
    Setup ref_length to make room for the whole 
    primary key to be written in the ref variable
  */
  
  if (table_share->primary_key != MAX_KEY) 
  {
    key= table->key_info+table_share->primary_key;
    ref_length= key->key_length;
  }
  else // (table_share->primary_key == MAX_KEY) 
  {
    if (m_use_partition_function)
    {
      ref_length+= sizeof(m_part_id);
    }
  }

  DBUG_PRINT("info", ("ref_length: %d", ref_length));

  // Init table lock structure 
  /* ndb_share reference handler */
  if (!(m_share=get_share(name, table)))
    DBUG_RETURN(1);
  DBUG_PRINT("NDB_SHARE", ("%s handler  use_count: %u",
                           m_share->key, m_share->use_count));
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);
  
  set_dbname(name);
  set_tabname(name);
  
  if ((res= check_ndb_connection()) ||
      (res= get_metadata(name)))
  {
    /* ndb_share reference handler free */
    DBUG_PRINT("NDB_SHARE", ("%s handler free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share);
    m_share= 0;
    DBUG_RETURN(res);
  }
  while (1)
  {
    Ndb *ndb= get_ndb();
    if (ndb->setDatabaseName(m_dbname))
    {
      res= ndb_to_mysql_error(&ndb->getNdbError());
      break;
    }
    struct Ndb_statistics stat;
    res= ndb_get_table_statistics(NULL, FALSE, ndb, m_ndb_statistics_record,
                                  &stat);
    stats.mean_rec_length= stat.row_size;
    stats.data_file_length= stat.fragment_memory;
    stats.records= stat.row_count;
    if(!res)
      res= info(HA_STATUS_CONST);
    break;
  }
  if (res)
  {
    free_share(&m_share);
    m_share= 0;
    release_metadata(current_thd, get_ndb());
    DBUG_RETURN(res);
  }
#ifdef HAVE_NDB_BINLOG
  if (!ndb_binlog_tables_inited && ndb_binlog_running)
    table->db_stat|= HA_READ_ONLY;
#endif
  DBUG_RETURN(0);
}

/*
  Set partition info

  SYNOPSIS
    set_part_info()
    part_info

  RETURN VALUE
    NONE

  DESCRIPTION
    Set up partition info when handler object created
*/

void ha_ndbcluster::set_part_info(partition_info *part_info)
{
  m_part_info= part_info;
  if (!(m_part_info->part_type == HASH_PARTITION &&
        m_part_info->list_of_part_fields &&
        !m_part_info->is_sub_partitioned()))
    m_use_partition_function= TRUE;
}

/*
  Close the table
  - release resources setup by open()
 */

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");
  THD *thd= table->in_use;
  Ndb *ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
  /* ndb_share reference handler free */
  DBUG_PRINT("NDB_SHARE", ("%s handler free  use_count: %u",
                           m_share->key, m_share->use_count));
  free_share(&m_share);
  m_share= 0;
  release_metadata(thd, ndb);
  DBUG_RETURN(0);
}


Thd_ndb* ha_ndbcluster::seize_thd_ndb()
{
  Thd_ndb *thd_ndb;
  DBUG_ENTER("seize_thd_ndb");

  thd_ndb= new Thd_ndb();
  if (thd_ndb == NULL)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    return NULL;
  }
  if (thd_ndb->ndb->init(max_transactions) != 0)
  {
    ERR_PRINT(thd_ndb->ndb->getNdbError());
    /*
      TODO 
      Alt.1 If init fails because to many allocated Ndb 
      wait on condition for a Ndb object to be released.
      Alt.2 Seize/release from pool, wait until next release 
    */
    delete thd_ndb;
    thd_ndb= NULL;
  }
  DBUG_RETURN(thd_ndb);
}


void ha_ndbcluster::release_thd_ndb(Thd_ndb* thd_ndb)
{
  DBUG_ENTER("release_thd_ndb");
  delete thd_ndb;
  DBUG_VOID_RETURN;
}


/*
  If this thread already has a Thd_ndb object allocated
  in current THD, reuse it. Otherwise
  seize a Thd_ndb object, assign it to current THD and use it.
 
*/

Ndb* check_ndb_in_thd(THD* thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb)
  {
    if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
      return NULL;
    set_thd_ndb(thd, thd_ndb);
  }
  return thd_ndb->ndb;
}



int ha_ndbcluster::check_ndb_connection(THD* thd)
{
  Ndb *ndb;
  DBUG_ENTER("check_ndb_connection");
  
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  if (ndb->setDatabaseName(m_dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  DBUG_RETURN(0);
}


static int ndbcluster_close_connection(handlerton *hton, THD *thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ENTER("ndbcluster_close_connection");
  if (thd_ndb)
  {
    ha_ndbcluster::release_thd_ndb(thd_ndb);
    set_thd_ndb(thd, NULL); // not strictly required but does not hurt either
  }
  DBUG_RETURN(0);
}


/*
  Try to discover one table from NDB
 */

int ndbcluster_discover(handlerton *hton, THD* thd, const char *db, 
                        const char *name,
                        const void** frmblob, 
                        uint* frmlen)
{
  int error= 0;
  NdbError ndb_error;
  uint len;
  const void* data= NULL;
  Ndb* ndb;
  char key[FN_REFLEN];
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name)); 

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);  
  if (ndb->setDatabaseName(db))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  NDBDICT* dict= ndb->getDictionary();
  build_table_filename(key, sizeof(key), db, name, "", 0);
  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(key, 0, FALSE);
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
  }
  if (share && get_ndb_share_state(share) == NSS_ALTERED)
  {
    // Frm has been altered on disk, but not yet written to ndb
    if (readfrm(key, &data, &len))
    {
      DBUG_PRINT("error", ("Could not read frm"));
      error= 1;
      goto err;
    }
  }
  else
  {
    Ndb_table_guard ndbtab_g(dict, name);
    const NDBTAB *tab= ndbtab_g.get_table();
    if (!tab)
    {
      const NdbError err= dict->getNdbError();
      if (err.code == 709 || err.code == 723)
      {
        error= -1;
        DBUG_PRINT("info", ("ndb_error.code: %u", ndb_error.code));
      }
      else
      {
        error= -1;
        ndb_error= err;
        DBUG_PRINT("info", ("ndb_error.code: %u", ndb_error.code));
      }
      goto err;
    }
    DBUG_PRINT("info", ("Found table %s", tab->getName()));
    
    len= tab->getFrmLength();  
    if (len == 0 || tab->getFrmData() == NULL)
    {
      DBUG_PRINT("error", ("No frm data found."));
      error= 1;
      goto err;
    }
    
    if (unpackfrm(&data, &len, tab->getFrmData()))
    {
      DBUG_PRINT("error", ("Could not unpack table"));
      error= 1;
      goto err;
    }
  }

  *frmlen= len;
  *frmblob= data;
  
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }

  DBUG_RETURN(0);
err:
  my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }
  if (ndb_error.code)
  {
    ERR_RETURN(ndb_error);
  }
  DBUG_RETURN(error);
}

/*
  Check if a table exists in NDB

 */

int ndbcluster_table_exists_in_engine(handlerton *hton, THD* thd, 
                                      const char *db,
                                      const char *name)
{
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s  name: %s", db, name));

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  NDBDICT* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());
  for (uint i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    if (my_strcasecmp(system_charset_info, elmt.database, db))
      continue;
    if (my_strcasecmp(system_charset_info, elmt.name, name))
      continue;
    DBUG_PRINT("info", ("Found table"));
    DBUG_RETURN(HA_ERR_TABLE_EXIST);
  }
  DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
}



extern "C" byte* tables_get_key(const char *entry, uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= strlen(entry);
  return (byte*) entry;
}


/*
  Drop a database in NDB Cluster
  NOTE add a dummy void function, since stupid handlerton is returning void instead of int...
*/

int ndbcluster_drop_database_impl(const char *path)
{
  DBUG_ENTER("ndbcluster_drop_database");
  THD *thd= current_thd;
  char dbname[FN_HEADLEN];
  Ndb* ndb;
  NdbDictionary::Dictionary::List list;
  uint i;
  char *tabname;
  List<char> drop_list;
  int ret= 0;
  ha_ndbcluster::set_dbname(path, (char *)&dbname);
  DBUG_PRINT("enter", ("db: %s", dbname));
  
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(-1);
  
  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    DBUG_RETURN(-1);
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));     
    
    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, elmt.database, dbname))
      continue;
    DBUG_PRINT("info", ("%s must be dropped", elmt.name));     
    drop_list.push_back(thd->strdup(elmt.name));
  }
  // Drop any tables belonging to database
  char full_path[FN_REFLEN];
  char *tmp= full_path +
    build_table_filename(full_path, sizeof(full_path), dbname, "", "", 0);
  if (ndb->setDatabaseName(dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  List_iterator_fast<char> it(drop_list);
  while ((tabname=it++))
  {
    tablename_to_filename(tabname, tmp, FN_REFLEN - (tmp - full_path)-1);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (ha_ndbcluster::delete_table(0, ndb, full_path, dbname, tabname))
    {
      const NdbError err= dict->getNdbError();
      if (err.code != 709 && err.code != 723)
      {
        ERR_PRINT(err);
        ret= ndb_to_mysql_error(&err);
      }
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  DBUG_RETURN(ret);      
}

static void ndbcluster_drop_database(handlerton *hton, char *path)
{
  DBUG_ENTER("ndbcluster_drop_database");
#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop database unless
    schema distribution table is setup
  */
  if (!ndb_schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_VOID_RETURN;
    //DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
#endif
  ndbcluster_drop_database_impl(path);
#ifdef HAVE_NDB_BINLOG
  char db[FN_REFLEN];
  THD *thd= current_thd;
  ha_ndbcluster::set_dbname(path, db);
  ndbcluster_log_schema_op(thd, 0,
                           thd->query, thd->query_length,
                           db, "", 0, 0, SOT_DROP_DB, 0, 0, 0);
#endif
  DBUG_VOID_RETURN;
}

int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name)
{
  LEX *old_lex= thd->lex, newlex;
  thd->lex= &newlex;
  newlex.current_select= NULL;
  lex_start(thd);
  int res= ha_create_table_from_engine(thd, db, table_name);
  thd->lex= old_lex;
  return res;
}

/*
  find all tables in ndb and discover those needed
*/
int ndbcluster_find_all_files(THD *thd)
{
  Ndb* ndb;
  char key[FN_REFLEN];
  NDBDICT *dict;
  int unhandled, retries= 5, skipped;
  DBUG_ENTER("ndbcluster_find_all_files");

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  dict= ndb->getDictionary();

  LINT_INIT(unhandled);
  LINT_INIT(skipped);
  do
  {
    NdbDictionary::Dictionary::List list;
    if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
      ERR_RETURN(dict->getNdbError());
    unhandled= 0;
    skipped= 0;
    retries--;
    for (uint i= 0 ; i < list.count ; i++)
    {
      NDBDICT::List::Element& elmt= list.elements[i];
      if (IS_TMP_PREFIX(elmt.name) || IS_NDB_BLOB_PREFIX(elmt.name))
      {
        DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
        continue;
      }
      DBUG_PRINT("info", ("Found %s.%s in NDB", elmt.database, elmt.name));
      if (elmt.state != NDBOBJ::StateOnline &&
          elmt.state != NDBOBJ::StateBackup &&
          elmt.state != NDBOBJ::StateBuilding)
      {
        sql_print_information("NDB: skipping setup table %s.%s, in state %d",
                              elmt.database, elmt.name, elmt.state);
        skipped++;
        continue;
      }

      ndb->setDatabaseName(elmt.database);
      Ndb_table_guard ndbtab_g(dict, elmt.name);
      const NDBTAB *ndbtab= ndbtab_g.get_table();
      if (!ndbtab)
      {
        if (retries == 0)
          sql_print_error("NDB: failed to setup table %s.%s, error: %d, %s",
                          elmt.database, elmt.name,
                          dict->getNdbError().code,
                          dict->getNdbError().message);
        unhandled++;
        continue;
      }

      if (ndbtab->getFrmLength() == 0)
        continue;
    
      /* check if database exists */
      char *end= key +
        build_table_filename(key, sizeof(key), elmt.database, "", "", 0);
      if (my_access(key, F_OK))
      {
        /* no such database defined, skip table */
        continue;
      }
      /* finalize construction of path */
      end+= tablename_to_filename(elmt.name, end,
                                  sizeof(key)-(end-key));
      const void *data= 0, *pack_data= 0;
      uint length, pack_length;
      int discover= 0;
      if (readfrm(key, &data, &length) ||
          packfrm(data, length, &pack_data, &pack_length))
      {
        discover= 1;
        sql_print_information("NDB: missing frm for %s.%s, discovering...",
                              elmt.database, elmt.name);
      }
      else if (cmp_frm(ndbtab, pack_data, pack_length))
      {
        /* ndb_share reference temporary */
        NDB_SHARE *share= get_share(key, 0, FALSE);
        if (share)
        {
          DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                                   share->key, share->use_count));
        }
        if (!share || get_ndb_share_state(share) != NSS_ALTERED)
        {
          discover= 1;
          sql_print_information("NDB: mismatch in frm for %s.%s, discovering...",
                                elmt.database, elmt.name);
        }
        if (share)
        {
          /* ndb_share reference temporary free */
          DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                   share->key, share->use_count));
          free_share(&share);
        }
      }
      my_free((char*) data, MYF(MY_ALLOW_ZERO_PTR));
      my_free((char*) pack_data, MYF(MY_ALLOW_ZERO_PTR));

      pthread_mutex_lock(&LOCK_open);
      if (discover)
      {
        /* ToDo 4.1 database needs to be created if missing */
        if (ndb_create_table_from_engine(thd, elmt.database, elmt.name))
        {
          /* ToDo 4.1 handle error */
        }
      }
#ifdef HAVE_NDB_BINLOG
      else
      {
        /* set up replication for this table */
        ndbcluster_create_binlog_setup(ndb, key, end-key,
                                       elmt.database, elmt.name,
                                       TRUE);
      }
#endif
      pthread_mutex_unlock(&LOCK_open);
    }
  }
  while (unhandled && retries);

  DBUG_RETURN(-(skipped + unhandled));
}

int ndbcluster_find_files(handlerton *hton, THD *thd,
                          const char *db,
                          const char *path,
                          const char *wild, bool dir, List<char> *files)
{
  DBUG_ENTER("ndbcluster_find_files");
  DBUG_PRINT("enter", ("db: %s", db));
  { // extra bracket to avoid gcc 2.95.3 warning
  uint i;
  Ndb* ndb;
  char name[FN_REFLEN];
  HASH ndb_tables, ok_tables;
  NDBDICT::List list;

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  if (dir)
    DBUG_RETURN(0); // Discover of databases not yet supported

  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());

  if (hash_init(&ndb_tables, system_charset_info,list.count,0,0,
                (hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ndb_tables"));
    DBUG_RETURN(-1);
  }

  if (hash_init(&ok_tables, system_charset_info,32,0,0,
                (hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ok_tables"));
    hash_free(&ndb_tables);
    DBUG_RETURN(-1);
  }  

  for (i= 0 ; i < list.count ; i++)
  {
    NDBDICT::List::Element& elmt= list.elements[i];
    if (IS_TMP_PREFIX(elmt.name) || IS_NDB_BLOB_PREFIX(elmt.name))
    {
      DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
      continue;
    }
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));

    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, elmt.database, db))
      continue;

    // Apply wildcard to list of tables in NDB
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info, elmt.name, wild))
          continue;
      }
      else if (wild_compare(elmt.name,wild,0))
        continue;
    }
    DBUG_PRINT("info", ("Inserting %s into ndb_tables hash", elmt.name));     
    my_hash_insert(&ndb_tables, (byte*)thd->strdup(elmt.name));
  }

  char *file_name;
  List_iterator<char> it(*files);
  List<char> delete_list;
  while ((file_name=it++))
  {
    bool file_on_disk= FALSE;
    DBUG_PRINT("info", ("%s", file_name));     
    if (hash_search(&ndb_tables, file_name, strlen(file_name)))
    {
      DBUG_PRINT("info", ("%s existed in NDB _and_ on disk ", file_name));
      file_on_disk= TRUE;
    }
    
    // Check for .ndb file with this name
    build_table_filename(name, sizeof(name), db, file_name, ha_ndb_ext, 0);
    DBUG_PRINT("info", ("Check access for %s", name));
    if (my_access(name, F_OK))
    {
      DBUG_PRINT("info", ("%s did not exist on disk", name));     
      // .ndb file did not exist on disk, another table type
      if (file_on_disk)
      {
	// Ignore this ndb table
	gptr record=  hash_search(&ndb_tables, file_name, strlen(file_name));
	DBUG_ASSERT(record);
	hash_delete(&ndb_tables, record);
	push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			    ER_TABLE_EXISTS_ERROR,
			    "Local table %s.%s shadows ndb table",
			    db, file_name);
      }
      continue;
    }
    if (file_on_disk) 
    {
      // File existed in NDB and as frm file, put in ok_tables list
      my_hash_insert(&ok_tables, (byte*)file_name);
      continue;
    }
    DBUG_PRINT("info", ("%s existed on disk", name));     
    // The .ndb file exists on disk, but it's not in list of tables in ndb
    // Verify that handler agrees table is gone.
    if (ndbcluster_table_exists_in_engine(hton, thd, db, file_name) == HA_ERR_NO_SUCH_TABLE)    
    {
      DBUG_PRINT("info", ("NDB says %s does not exists", file_name));     
      it.remove();
      // Put in list of tables to remove from disk
      delete_list.push_back(thd->strdup(file_name));
    }
  }

#ifdef HAVE_NDB_BINLOG
  /* setup logging to binlog for all discovered tables */
  {
    char *end, *end1= name +
      build_table_filename(name, sizeof(name), db, "", "", 0);
    for (i= 0; i < ok_tables.records; i++)
    {
      file_name= (char*)hash_element(&ok_tables, i);
      end= end1 +
        tablename_to_filename(file_name, end1, sizeof(name) - (end1 - name));
      pthread_mutex_lock(&LOCK_open);
      ndbcluster_create_binlog_setup(ndb, name, end-name,
                                     db, file_name, TRUE);
      pthread_mutex_unlock(&LOCK_open);
    }
  }
#endif

  // Check for new files to discover
  DBUG_PRINT("info", ("Checking for new files to discover"));       
  List<char> create_list;
  for (i= 0 ; i < ndb_tables.records ; i++)
  {
    file_name= hash_element(&ndb_tables, i);
    if (!hash_search(&ok_tables, file_name, strlen(file_name)))
    {
      build_table_filename(name, sizeof(name), db, file_name, reg_ext, 0);
      if (my_access(name, F_OK))
      {
        DBUG_PRINT("info", ("%s must be discovered", file_name));
        // File is in list of ndb tables and not in ok_tables
        // This table need to be created
        create_list.push_back(thd->strdup(file_name));
      }
    }
  }

  // Lock mutex before deleting and creating frm files
  pthread_mutex_lock(&LOCK_open);

  if (!global_read_lock)
  {
    // Delete old files
    List_iterator_fast<char> it3(delete_list);
    while ((file_name=it3++))
    {
      DBUG_PRINT("info", ("Remove table %s/%s", db, file_name));
      // Delete the table and all related files
      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.db= (char*) db;
      table_list.alias= table_list.table_name= (char*)file_name;
      (void)mysql_rm_table_part2(thd, &table_list,
                                                                 /* if_exists */ FALSE,
                                                                 /* drop_temporary */ FALSE,
                                                                 /* drop_view */ FALSE,
                                                                 /* dont_log_query*/ TRUE);
      /* Clear error message that is returned when table is deleted */
      thd->clear_error();
    }
  }

  // Create new files
  List_iterator_fast<char> it2(create_list);
  while ((file_name=it2++))
  {  
    DBUG_PRINT("info", ("Table %s need discovery", file_name));
    if (ndb_create_table_from_engine(thd, db, file_name) == 0)
      files->push_back(thd->strdup(file_name)); 
  }

  pthread_mutex_unlock(&LOCK_open);
  
  hash_free(&ok_tables);
  hash_free(&ndb_tables);

  // Delete schema file from files
  if (!strcmp(db, NDB_REP_DB))
  {
    uint count = 0;
    while (count++ < files->elements)
    {
      file_name = (char *)files->pop();
      if (!strcmp(file_name, NDB_SCHEMA_TABLE))
      {
        DBUG_PRINT("info", ("skip %s.%s table, it should be hidden to user",
                   NDB_REP_DB, NDB_SCHEMA_TABLE));
        continue;
      }
      files->push_back(file_name); 
    }
  }
  } // extra bracket to avoid gcc 2.95.3 warning
  DBUG_RETURN(0);    
}


/*
  Initialise all gloal variables before creating 
  a NDB Cluster table handler
 */

/* Call back after cluster connect */
static int connect_callback()
{
  pthread_mutex_lock(&LOCK_ndb_util_thread);
  update_status_variables(&g_ndb_status,
                          g_ndb_cluster_connection);

  uint node_id, i= 0;
  Ndb_cluster_connection_node_iter node_iter;
  memset((void *)g_node_id_map, 0xFFFF, sizeof(g_node_id_map));
  while ((node_id= g_ndb_cluster_connection->get_next_node(node_iter)))
    g_node_id_map[node_id]= i++;

  pthread_cond_signal(&COND_ndb_util_thread);
  pthread_mutex_unlock(&LOCK_ndb_util_thread);
  return 0;
}

extern int ndb_dictionary_is_mysqld;
extern pthread_mutex_t LOCK_plugin;

static int ndbcluster_init(void *p)
{
  int res;
  DBUG_ENTER("ndbcluster_init");

  if (ndbcluster_inited)
    DBUG_RETURN(FALSE);

  /*
    Below we create new THD's. They'll need LOCK_plugin, but it's taken now by
    plugin initialization code. Release it to avoid deadlocks.  It's safe, as
    there're no threads that may concurrently access plugin control structures.
  */
  pthread_mutex_unlock(&LOCK_plugin);

  pthread_mutex_init(&ndbcluster_mutex,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_ndb_util_thread, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_ndb_util_thread, NULL);
  pthread_cond_init(&COND_ndb_util_ready, NULL);
  ndb_util_thread_running= -1;
  ndbcluster_terminating= 0;
  ndb_dictionary_is_mysqld= 1;
  ndbcluster_hton= (handlerton *)p;

  {
    handlerton *h= ndbcluster_hton;
    h->state=            SHOW_OPTION_YES;
    h->db_type=          DB_TYPE_NDBCLUSTER;
    h->close_connection= ndbcluster_close_connection;
    h->commit=           ndbcluster_commit;
    h->rollback=         ndbcluster_rollback;
    h->create=           ndbcluster_create_handler; /* Create a new handler */
    h->drop_database=    ndbcluster_drop_database;  /* Drop a database */
    h->panic=            ndbcluster_end;            /* Panic call */
    h->show_status=      ndbcluster_show_status;    /* Show status */
    h->alter_tablespace= ndbcluster_alter_tablespace;    /* Show status */
    h->partition_flags=  ndbcluster_partition_flags; /* Partition flags */
    h->alter_table_flags=ndbcluster_alter_table_flags; /* Alter table flags */
    h->fill_files_table= ndbcluster_fill_files_table;
#ifdef HAVE_NDB_BINLOG
    ndbcluster_binlog_init_handlerton();
#endif
    h->flags=            HTON_CAN_RECREATE | HTON_TEMPORARY_NOT_SUPPORTED;
    h->discover=         ndbcluster_discover;
    h->find_files= ndbcluster_find_files;
    h->table_exists_in_engine= ndbcluster_table_exists_in_engine;
  }

  // Initialize ndb interface
  ndb_init_internal();

  // Set connectstring if specified
  if (opt_ndbcluster_connectstring != 0)
    DBUG_PRINT("connectstring", ("%s", opt_ndbcluster_connectstring));     
  if ((g_ndb_cluster_connection=
       new Ndb_cluster_connection(opt_ndbcluster_connectstring)) == 0)
  {
    sql_print_error("NDB: failed to allocate global "
                    "ndb cluster connection object");
    DBUG_PRINT("error",("Ndb_cluster_connection(%s)",
                        opt_ndbcluster_connectstring));
    my_errno= HA_ERR_OUT_OF_MEM;
    goto ndbcluster_init_error;
  }
  {
    char buf[128];
    my_snprintf(buf, sizeof(buf), "mysqld --server-id=%lu", server_id);
    g_ndb_cluster_connection->set_name(buf);
  }
  g_ndb_cluster_connection->set_optimized_node_selection
    (opt_ndb_optimized_node_selection);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    sql_print_error("NDB: failed to allocate global ndb object");
    DBUG_PRINT("error", ("failed to create global ndb object"));
    my_errno= HA_ERR_OUT_OF_MEM;
    goto ndbcluster_init_error;
  }
  if (g_ndb->init() != 0)
  {
    ERR_PRINT (g_ndb->getNdbError());
    goto ndbcluster_init_error;
  }

  /* Connect to management server */

  struct timeval end_time;
  gettimeofday(&end_time, 0);
  end_time.tv_sec+= opt_ndb_wait_connected;

  while ((res= g_ndb_cluster_connection->connect(0,0,0)) == 1)
  {
    struct timeval now_time;
    gettimeofday(&now_time, 0);
    if (now_time.tv_sec > end_time.tv_sec ||
        (now_time.tv_sec == end_time.tv_sec &&
         now_time.tv_usec >= end_time.tv_usec))
      break;
    sleep(1);
  }

  {
    g_ndb_cluster_connection_pool_alloc= opt_ndb_cluster_connection_pool;
    g_ndb_cluster_connection_pool= (Ndb_cluster_connection**)
      my_malloc(g_ndb_cluster_connection_pool_alloc *
                sizeof(Ndb_cluster_connection*),
                MYF(MY_WME | MY_ZEROFILL));
    pthread_mutex_init(&g_ndb_cluster_connection_pool_mutex,
                       MY_MUTEX_INIT_FAST);
    g_ndb_cluster_connection_pool[0]= g_ndb_cluster_connection;
    for (unsigned i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if ((g_ndb_cluster_connection_pool[i]=
           new Ndb_cluster_connection(opt_ndbcluster_connectstring,
                                      g_ndb_cluster_connection)) == 0)
      {
        sql_print_error("NDB[%u]: failed to allocate cluster connect object",
                        i);
        DBUG_PRINT("error",("Ndb_cluster_connection[%u](%s)",
                            i, opt_ndbcluster_connectstring));
        goto ndbcluster_init_error;
      }
      {
        char buf[128];
        my_snprintf(buf, sizeof(buf), "mysqld --server-id=%lu (connection %u)",
                    server_id, i+1);
        g_ndb_cluster_connection_pool[i]->set_name(buf);
      }
      g_ndb_cluster_connection_pool[i]->set_optimized_node_selection
        (opt_ndb_optimized_node_selection);
    }
  }

  if (res == 0)
  {
    connect_callback();
    for (unsigned i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if (g_ndb_cluster_connection_pool[i]->node_id() == 0)
      {
        // not connected to mgmd yet, try again
        g_ndb_cluster_connection_pool[i]->connect(0,0,0);
        if (g_ndb_cluster_connection_pool[i]->node_id() == 0)
        {
          sql_print_warning("NDB[%u]: starting connect thread", i);
          g_ndb_cluster_connection_pool[i]->start_connect_thread();
          continue;
        }
      }
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine (%u) at %s on port %d", i,
                  g_ndb_cluster_connection_pool[i]->get_connected_host(),
                  g_ndb_cluster_connection_pool[i]->get_connected_port()));

      struct timeval now_time;
      gettimeofday(&now_time, 0);
      ulong wait_until_ready_time = (end_time.tv_sec > now_time.tv_sec) ?
        end_time.tv_sec - now_time.tv_sec : 1;
      res= g_ndb_cluster_connection_pool[i]->
        wait_until_ready(wait_until_ready_time,3);
      if (res == 0)
      {
        sql_print_information("NDB[%u]: all storage nodes connected", i);
      }
      else if (res > 0)
      {
        sql_print_information("NDB[%u]: some storage nodes connected", i);
      }
      else if (res < 0)
      {
        sql_print_information("NDB[%u]: no storage nodes connected (timed out)", i);
      }
    }
  } 
  else if (res == 1)
  {
    for (unsigned i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if (g_ndb_cluster_connection_pool[i]->
          start_connect_thread(i == 0 ? connect_callback :  NULL)) 
      {
        sql_print_error("NDB[%u]: failed to start connect thread", i);
        DBUG_PRINT("error", ("g_ndb_cluster_connection->start_connect_thread()"));
        goto ndbcluster_init_error;
      }
    }
#ifndef DBUG_OFF
    {
      char buf[1024];
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine not started, "
                  "will connect using %s",
                  g_ndb_cluster_connection->
                  get_connectstring(buf,sizeof(buf))));
    }
#endif
  }
  else
  {
    DBUG_ASSERT(res == -1);
    DBUG_PRINT("error", ("permanent error"));
    goto ndbcluster_init_error;
  }
  
  (void) hash_init(&ndbcluster_open_tables,system_charset_info,32,0,0,
                   (hash_get_key) ndbcluster_get_key,0,0);
#ifdef HAVE_NDB_BINLOG
  /* start the ndb injector thread */
  if (ndbcluster_binlog_start())
    goto ndbcluster_init_error;
#endif /* HAVE_NDB_BINLOG */

  ndb_cache_check_time = opt_ndb_cache_check_time;
  // Create utility thread
  pthread_t tmp;
  if (pthread_create(&tmp, &connection_attrib, ndb_util_thread_func, 0))
  {
    DBUG_PRINT("error", ("Could not create ndb utility thread"));
    hash_free(&ndbcluster_open_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    pthread_mutex_destroy(&LOCK_ndb_util_thread);
    pthread_cond_destroy(&COND_ndb_util_thread);
    pthread_cond_destroy(&COND_ndb_util_ready);
    goto ndbcluster_init_error;
  }

  /* Wait for the util thread to start */
  pthread_mutex_lock(&LOCK_ndb_util_thread);
  while (ndb_util_thread_running < 0)
    pthread_cond_wait(&COND_ndb_util_ready, &LOCK_ndb_util_thread);
  pthread_mutex_unlock(&LOCK_ndb_util_thread);
  
  if (!ndb_util_thread_running)
  {
    DBUG_PRINT("error", ("ndb utility thread exited prematurely"));
    hash_free(&ndbcluster_open_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    pthread_mutex_destroy(&LOCK_ndb_util_thread);
    pthread_cond_destroy(&COND_ndb_util_thread);
    pthread_cond_destroy(&COND_ndb_util_ready);
    goto ndbcluster_init_error;
  }

  pthread_mutex_lock(&LOCK_plugin);

  ndbcluster_inited= 1;
  DBUG_RETURN(FALSE);

ndbcluster_init_error:
  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  {
    if (g_ndb_cluster_connection_pool)
    {
      /* first in pool is the main one, wait with release */
      for (unsigned i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
      {
        if (g_ndb_cluster_connection_pool[i])
          delete g_ndb_cluster_connection_pool[i];
      }
      my_free((gptr) g_ndb_cluster_connection_pool, MYF(MY_ALLOW_ZERO_PTR));
      pthread_mutex_destroy(&g_ndb_cluster_connection_pool_mutex);
      g_ndb_cluster_connection_pool= 0;
    }
    g_ndb_cluster_connection_pool_alloc= 0;
    g_ndb_cluster_connection_pool_pos= 0;
  }
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;
  ndbcluster_hton->state= SHOW_OPTION_DISABLED;               // If we couldn't use handler

  pthread_mutex_lock(&LOCK_plugin);

  DBUG_RETURN(TRUE);
}

static int ndbcluster_end(handlerton *hton, ha_panic_function type)
{
  DBUG_ENTER("ndbcluster_end");

  if (!ndbcluster_inited)
    DBUG_RETURN(0);
  ndbcluster_inited= 0;

  /* wait for util thread to finish */
  sql_print_information("Stopping Cluster Utility thread");
  pthread_mutex_lock(&LOCK_ndb_util_thread);
  ndbcluster_terminating= 1;
  pthread_cond_signal(&COND_ndb_util_thread);
  while (ndb_util_thread_running > 0)
    pthread_cond_wait(&COND_ndb_util_ready, &LOCK_ndb_util_thread);
  pthread_mutex_unlock(&LOCK_ndb_util_thread);


#ifdef HAVE_NDB_BINLOG
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    while (ndbcluster_open_tables.records)
    {
      NDB_SHARE *share=
        (NDB_SHARE*) hash_element(&ndbcluster_open_tables, 0);
#ifndef DBUG_OFF
      fprintf(stderr, "NDB: table share %s with use_count %d not freed\n",
              share->key, share->use_count);
#endif
      ndbcluster_real_free_share(&share);
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
#endif
  hash_free(&ndbcluster_open_tables);

  if (g_ndb)
  {
#ifndef DBUG_OFF
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (g_ndb->get_free_list_usage(&tmp))
    {
      uint leaked= (uint) tmp.m_created - tmp.m_free;
      if (leaked)
        fprintf(stderr, "NDB: Found %u %s%s that %s not been released\n",
                leaked, tmp.m_name,
                (leaked == 1)?"":"'s",
                (leaked == 1)?"has":"have");
    }
#endif
    delete g_ndb;
    g_ndb= NULL;
  }
  {
    if (g_ndb_cluster_connection_pool)
    {
      /* first in pool is the main one, wait with release */
      for (unsigned i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
      {
        if (g_ndb_cluster_connection_pool[i])
          delete g_ndb_cluster_connection_pool[i];
      }
      my_free((gptr) g_ndb_cluster_connection_pool, MYF(MY_ALLOW_ZERO_PTR));
      pthread_mutex_destroy(&g_ndb_cluster_connection_pool_mutex);
      g_ndb_cluster_connection_pool= 0;
    }
    g_ndb_cluster_connection_pool_alloc= 0;
    g_ndb_cluster_connection_pool_pos= 0;
  }
  delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;

  // cleanup ndb interface
  ndb_end_internal();

  pthread_mutex_destroy(&ndbcluster_mutex);
  pthread_mutex_destroy(&LOCK_ndb_util_thread);
  pthread_cond_destroy(&COND_ndb_util_thread);
  pthread_cond_destroy(&COND_ndb_util_ready);
  DBUG_RETURN(0);
}

void ha_ndbcluster::print_error(int error, myf errflag)
{
  DBUG_ENTER("ha_ndbcluster::print_error");
  DBUG_PRINT("enter", ("error: %d", error));

  if (error == HA_ERR_NO_PARTITION_FOUND)
    m_part_info->print_no_partition_found(table);
  else
    handler::print_error(error, errflag);
  DBUG_VOID_RETURN;
}


/*
  Static error print function called from
  static handler method ndbcluster_commit
  and ndbcluster_rollback
*/

void ndbcluster_print_error(int error, const NdbOperation *error_op)
{
  DBUG_ENTER("ndbcluster_print_error");
  TABLE_SHARE share;
  const char *tab_name= (error_op) ? error_op->getTableName() : "";
  share.db.str= (char*) "";
  share.db.length= 0;
  share.table_name.str= (char *) tab_name;
  share.table_name.length= strlen(tab_name);
  ha_ndbcluster error_handler(ndbcluster_hton, &share);
  error_handler.print_error(error, MYF(0));
  DBUG_VOID_RETURN;
}

/**
 * Set a given location from full pathname to database name
 *
 */
void ha_ndbcluster::set_dbname(const char *path_name, char *dbname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN];
 
  tmp_name= tmp_buff;
  /* Scan name from the end */
  ptr= strend(path_name)-1;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end= ptr;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(tmp_name, ptr + 1, name_len);
  tmp_name[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  
  ptr= tmp_name;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
  filename_to_tablename(tmp_name, dbname, FN_REFLEN);
}

/*
  Set m_dbname from full pathname to table file
 */

void ha_ndbcluster::set_dbname(const char *path_name)
{
  set_dbname(path_name, m_dbname);
}

/**
 * Set a given location from full pathname to table file
 *
 */
void
ha_ndbcluster::set_tabname(const char *path_name, char * tabname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN];

  tmp_name= tmp_buff;
  /* Scan name from the end */
  end= strend(path_name)-1;
  ptr= end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  ptr= tmp_name;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
  filename_to_tablename(tmp_name, tabname, FN_REFLEN);
}

/*
  Set m_tabname from full pathname to table file 
 */

void ha_ndbcluster::set_tabname(const char *path_name)
{
  set_tabname(path_name, m_tabname);
}


/* ToDo: convert to NdbRecord? */
ha_rows 
ha_ndbcluster::records_in_range(uint inx, key_range *min_key,
                                key_range *max_key)
{
  KEY *key_info= table->key_info + inx;
  uint key_length= key_info->key_length;
  NDB_INDEX_TYPE idx_type= get_index_type(inx);  

  DBUG_ENTER("records_in_range");
  // Prevent partial read of hash indexes by returning HA_POS_ERROR
  if ((idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX) &&
      ((min_key && min_key->length < key_length) ||
       (max_key && max_key->length < key_length)))
    DBUG_RETURN(HA_POS_ERROR);
  
  // Read from hash index with full key
  // This is a "const" table which returns only one record!      
  if ((idx_type != ORDERED_INDEX) &&
      ((min_key && min_key->length == key_length) || 
       (max_key && max_key->length == key_length)))
    DBUG_RETURN(1);
  
  if ((idx_type == PRIMARY_KEY_ORDERED_INDEX ||
       idx_type == UNIQUE_ORDERED_INDEX ||
       idx_type == ORDERED_INDEX) &&
    m_index[inx].index_stat != NULL)
  {
    NDB_INDEX_DATA& d=m_index[inx];
    const NDBINDEX* index= d.index;
    Ndb* ndb=get_ndb();
    NdbTransaction* trans=NULL;
    NdbIndexScanOperation* op=NULL;
    int res=0;
    Uint64 rows;

    do
    {
      // We must provide approx table rows
      Uint64 table_rows=0;
      Ndb_local_table_statistics *ndb_info= m_table_info;
      if (ndb_info->records != ~(ha_rows)0 && ndb_info->records != 0)
      {
        table_rows = ndb_info->records;
        DBUG_PRINT("info", ("use info->records: %lu", (ulong) table_rows));
      }
      else
      {
        Ndb_statistics stat;
        if ((res=ndb_get_table_statistics(this, TRUE, ndb,
                                          m_ndb_statistics_record, &stat)))
          break;
        table_rows=stat.row_count;
        DBUG_PRINT("info", ("use db row_count: %lu", (ulong) table_rows));
        if (table_rows == 0) {
          // Problem if autocommit=0
#ifdef ndb_get_table_statistics_uses_active_trans
          rows=0;
          break;
#endif
        }
      }

      // Define scan op for the range
      if ((trans=m_active_trans) == NULL || 
	  trans->commitStatus() != NdbTransaction::Started)
      {
        DBUG_PRINT("info", ("no active trans"));
        if (! (trans=ndb->startTransaction()))
          ERR_BREAK(ndb->getNdbError(), res);
      }
      if (! (op=trans->getNdbIndexScanOperation(index, (NDBTAB*)m_table)))
        ERR_BREAK(trans->getNdbError(), res);
      if ((op->readTuples(NdbOperation::LM_CommittedRead)) == -1)
        ERR_BREAK(op->getNdbError(), res);
      const key_range *keys[2]={ min_key, max_key };
      if ((res=set_bounds(op, inx, TRUE, keys)) != 0)
        break;

      // Decide if db should be contacted
      int flags=0;
      if (d.index_stat_query_count < d.index_stat_cache_entries ||
          (d.index_stat_update_freq != 0 &&
           d.index_stat_query_count % d.index_stat_update_freq == 0))
      {
        DBUG_PRINT("info", ("force stat from db"));
        flags|=NdbIndexStat::RR_UseDb;
      }
      if (d.index_stat->records_in_range(index, op, table_rows, &rows, flags) == -1)
        ERR_BREAK(d.index_stat->getNdbError(), res);
      d.index_stat_query_count++;
    } while (0);

    if (trans != m_active_trans && rows == 0)
      rows = 1;
    if (trans != m_active_trans && trans != NULL)
      ndb->closeTransaction(trans);
    if (res != 0)
      DBUG_RETURN(HA_POS_ERROR);
    DBUG_RETURN(rows);
  }

  DBUG_RETURN(10); /* Good guess when you don't know anything */
}

ulonglong ha_ndbcluster::table_flags(void) const
{
  if (m_ha_not_exact_count)
    return m_table_flags & ~HA_STATS_RECORDS_IS_EXACT;
  return m_table_flags;
}
const char * ha_ndbcluster::table_type() const 
{
  return("NDBCLUSTER");
}
uint ha_ndbcluster::max_supported_record_length() const
{ 
  return NDB_MAX_TUPLE_SIZE;
}
uint ha_ndbcluster::max_supported_keys() const
{
  return MAX_KEY;
}
uint ha_ndbcluster::max_supported_key_parts() const 
{
  return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY;
}
uint ha_ndbcluster::max_supported_key_length() const
{
  return NDB_MAX_KEY_SIZE;
}
uint ha_ndbcluster::max_supported_key_part_length() const
{
  return NDB_MAX_KEY_SIZE;
}
bool ha_ndbcluster::low_byte_first() const
{ 
#ifdef WORDS_BIGENDIAN
  return FALSE;
#else
  return TRUE;
#endif
}
const char* ha_ndbcluster::index_type(uint key_number)
{
  switch (get_index_type(key_number)) {
  case ORDERED_INDEX:
  case UNIQUE_ORDERED_INDEX:
  case PRIMARY_KEY_ORDERED_INDEX:
    return "BTREE";
  case UNIQUE_INDEX:
  case PRIMARY_KEY_INDEX:
  default:
    return "HASH";
  }
}

uint8 ha_ndbcluster::table_cache_type()
{
  DBUG_ENTER("ha_ndbcluster::table_cache_type=HA_CACHE_TBL_ASKTRANSACT");
  DBUG_RETURN(HA_CACHE_TBL_ASKTRANSACT);
}


uint ndb_get_commitcount(THD *thd, char *dbname, char *tabname,
                         Uint64 *commit_count)
{
  char name[FN_REFLEN];
  NDB_SHARE *share;
  DBUG_ENTER("ndb_get_commitcount");

  build_table_filename(name, sizeof(name), dbname, tabname, "", 0);
  DBUG_PRINT("enter", ("name: %s", name));
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share=(NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                       (byte*) name,
                                       strlen(name))))
  {
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_PRINT("info", ("Table %s not found in ndbcluster_open_tables", name));
    DBUG_RETURN(1);
  }
  /* ndb_share reference temporary, free below */
  share->use_count++;
  DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                           share->key, share->use_count));
  pthread_mutex_unlock(&ndbcluster_mutex);

  pthread_mutex_lock(&share->mutex);
  if (ndb_cache_check_time > 0)
  {
    if (share->commit_count != 0)
    {
      *commit_count= share->commit_count;
#ifndef DBUG_OFF
      char buff[22];
#endif
      DBUG_PRINT("info", ("Getting commit_count: %s from share",
                          llstr(share->commit_count, buff)));
      pthread_mutex_unlock(&share->mutex);
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("info", ("Get commit_count from NDB"));
  Ndb *ndb;
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(1);
  if (ndb->setDatabaseName(dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  uint lock= share->commit_count_lock;
  pthread_mutex_unlock(&share->mutex);

  struct Ndb_statistics stat;
  {
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), tabname);
    if (ndbtab_g.get_table() == 0
        || ndb_get_table_statistics(NULL, FALSE, ndb, ndbtab_g.get_table(), &stat))
    {
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      DBUG_RETURN(1);
    }
  }

  pthread_mutex_lock(&share->mutex);
  if (share->commit_count_lock == lock)
  {
#ifndef DBUG_OFF
    char buff[22];
#endif
    DBUG_PRINT("info", ("Setting commit_count to %s",
                        llstr(stat.commit_count, buff)));
    share->commit_count= stat.commit_count;
    *commit_count= stat.commit_count;
  }
  else
  {
    DBUG_PRINT("info", ("Discarding commit_count, comit_count_lock changed"));
    *commit_count= 0;
  }
  pthread_mutex_unlock(&share->mutex);
  /* ndb_share reference temporary free */
  DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                           share->key, share->use_count));
  free_share(&share);
  DBUG_RETURN(0);
}


/*
  Check if a cached query can be used.
  This is done by comparing the supplied engine_data to commit_count of
  the table.
  The commit_count is either retrieved from the share for the table, where
  it has been cached by the util thread. If the util thread is not started,
  NDB has to be contacetd to retrieve the commit_count, this will introduce
  a small delay while waiting for NDB to answer.


  SYNOPSIS
  ndbcluster_cache_retrieval_allowed
    thd            thread handle
    full_name      concatenation of database name,
                   the null character '\0', and the table
                   name
    full_name_len  length of the full name,
                   i.e. len(dbname) + len(tablename) + 1

    engine_data    parameter retrieved when query was first inserted into
                   the cache. If the value of engine_data is changed,
                   all queries for this table should be invalidated.

  RETURN VALUE
    TRUE  Yes, use the query from cache
    FALSE No, don't use the cached query, and if engine_data
          has changed, all queries for this table should be invalidated

*/

static my_bool
ndbcluster_cache_retrieval_allowed(THD *thd,
                                   char *full_name, uint full_name_len,
                                   ulonglong *engine_data)
{
  Uint64 commit_count;
  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
  char *dbname= full_name;
  char *tabname= dbname+strlen(dbname)+1;
#ifndef DBUG_OFF
  char buff[22], buff2[22];
#endif
  DBUG_ENTER("ndbcluster_cache_retrieval_allowed");
  DBUG_PRINT("enter", ("dbname: %s, tabname: %s, is_autocommit: %d",
                       dbname, tabname, is_autocommit));

  if (!is_autocommit)
  {
    DBUG_PRINT("exit", ("No, don't use cache in transaction"));
    DBUG_RETURN(FALSE);
  }

  if (ndb_get_commitcount(thd, dbname, tabname, &commit_count))
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, could not retrieve commit_count"));
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("info", ("*engine_data: %s, commit_count: %s",
                      llstr(*engine_data, buff), llstr(commit_count, buff2)));
  if (commit_count == 0)
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, local commit has been performed"));
    DBUG_RETURN(FALSE);
  }
  else if (*engine_data != commit_count)
  {
    *engine_data= commit_count; /* invalidate */
     DBUG_PRINT("exit", ("No, commit_count has changed"));
     DBUG_RETURN(FALSE);
   }

  DBUG_PRINT("exit", ("OK to use cache, engine_data: %s",
                      llstr(*engine_data, buff)));
  DBUG_RETURN(TRUE);
}


/**
   Register a table for use in the query cache. Fetch the commit_count
   for the table and return it in engine_data, this will later be used
   to check if the table has changed, before the cached query is reused.

   SYNOPSIS
   ha_ndbcluster::can_query_cache_table
    thd            thread handle
    full_name      concatenation of database name,
                   the null character '\0', and the table
                   name
    full_name_len  length of the full name,
                   i.e. len(dbname) + len(tablename) + 1
    qc_engine_callback  function to be called before using cache on this table
    engine_data    out, commit_count for this table

  RETURN VALUE
    TRUE  Yes, it's ok to cahce this query
    FALSE No, don't cach the query

*/

my_bool
ha_ndbcluster::register_query_cache_table(THD *thd,
                                          char *full_name, uint full_name_len,
                                          qc_engine_callback *engine_callback,
                                          ulonglong *engine_data)
{
  Uint64 commit_count;
#ifndef DBUG_OFF
  char buff[22];
#endif
  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
  DBUG_ENTER("ha_ndbcluster::register_query_cache_table");
  DBUG_PRINT("enter",("dbname: %s, tabname: %s, is_autocommit: %d",
		      m_dbname, m_tabname, is_autocommit));

  if (!is_autocommit)
  {
    DBUG_PRINT("exit", ("Can't register table during transaction"));
    DBUG_RETURN(FALSE);
  }

  if (ndb_get_commitcount(thd, m_dbname, m_tabname, &commit_count))
  {
    *engine_data= 0;
    DBUG_PRINT("exit", ("Error, could not get commitcount"));
    DBUG_RETURN(FALSE);
  }
  *engine_data= commit_count;
  *engine_callback= ndbcluster_cache_retrieval_allowed;
  DBUG_PRINT("exit", ("commit_count: %s", llstr(commit_count, buff)));
  DBUG_RETURN(commit_count > 0);
}


/*
  Handling the shared NDB_SHARE structure that is needed to
  provide table locking.
  It's also used for sharing data with other NDB handlers
  in the same MySQL Server. There is currently not much
  data we want to or can share.
 */

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= share->key_length;
  return (byte*) share->key;
}


#ifndef DBUG_OFF

static void print_share(const char* where, NDB_SHARE* share)
{
  fprintf(DBUG_FILE,
          "%s %s.%s: use_count: %u, commit_count: %lu\n",
          where, share->db, share->table_name, share->use_count,
          (ulong) share->commit_count);
  fprintf(DBUG_FILE,
          "  - key: %s, key_length: %d\n",
          share->key, share->key_length);

#ifdef HAVE_NDB_BINLOG
  if (share->table)
    fprintf(DBUG_FILE,
            "  - share->table: %p %s.%s\n",
            share->table, share->table->s->db.str,
            share->table->s->table_name.str);
#endif
}


static void print_ndbcluster_open_tables()
{
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, ">ndbcluster_open_tables\n");
  for (uint i= 0; i < ndbcluster_open_tables.records; i++)
    print_share("",
                (NDB_SHARE*)hash_element(&ndbcluster_open_tables, i));
  fprintf(DBUG_FILE, "<ndbcluster_open_tables\n");
  DBUG_UNLOCK_FILE;
}

#endif


#define dbug_print_open_tables()                \
  DBUG_EXECUTE("info",                          \
               print_ndbcluster_open_tables(););

#define dbug_print_share(t, s)                  \
  DBUG_LOCK_FILE;                               \
  DBUG_EXECUTE("info",                          \
               print_share((t), (s)););         \
  DBUG_UNLOCK_FILE;


#ifdef HAVE_NDB_BINLOG
/*
  For some reason a share is still around, try to salvage the situation
  by closing all cached tables. If the share still exists, there is an
  error somewhere but only report this to the error log.  Keep this
  "trailing share" but rename it since there are still references to it
  to avoid segmentation faults.  There is a risk that the memory for
  this trailing share leaks.
  
  Must be called with previous pthread_mutex_lock(&ndbcluster_mutex)
*/
int handle_trailing_share(NDB_SHARE *share)
{
  THD *thd= current_thd;
  static ulong trailing_share_id= 0;
  DBUG_ENTER("handle_trailing_share");

  /* ndb_share reference temporary, free below */
  ++share->use_count;
  DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                           share->key, share->use_count));
  pthread_mutex_unlock(&ndbcluster_mutex);

  TABLE_LIST table_list;
  bzero((char*) &table_list,sizeof(table_list));
  table_list.db= share->db;
  table_list.alias= table_list.table_name= share->table_name;
  safe_mutex_assert_owner(&LOCK_open);
  close_cached_tables(thd, 0, &table_list, TRUE);

  pthread_mutex_lock(&ndbcluster_mutex);
  /* ndb_share reference temporary free */
  DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                           share->key, share->use_count));
  if (!--share->use_count)
  {
    if (ndb_extra_logging)
      sql_print_information("NDB_SHARE: trailing share "
                            "%s(connect_count: %u) "
                            "released by close_cached_tables at "
                            "connect_count: %u",
                            share->key,
                            share->connect_count,
                            g_ndb_cluster_connection->get_connect_count());
    ndbcluster_real_free_share(&share);
    DBUG_RETURN(0);
  }

  /*
    share still exists, if share has not been dropped by server
    release that share
  */
  if (share->state != NSS_DROPPED)
  {
    share->state= NSS_DROPPED;
    /* ndb_share reference create free */
    DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                             share->key, share->use_count));
    --share->use_count;

    if (share->use_count == 0)
    {
      if (ndb_extra_logging)
        sql_print_information("NDB_SHARE: trailing share "
                              "%s(connect_count: %u) "
                              "released after NSS_DROPPED check "
                              "at connect_count: %u",
                              share->key,
                              share->connect_count,
                              g_ndb_cluster_connection->get_connect_count());
      ndbcluster_real_free_share(&share);
      DBUG_RETURN(0);
    }
  }

  sql_print_error("NDB_SHARE: %s already exists  use_count=%d."
                  " Moving away for safety, but possible memleak.",
                  share->key, share->use_count);
  dbug_print_open_tables();

  /*
    Ndb share has not been released as it should
  */
#ifdef NOT_YET
  DBUG_ASSERT(FALSE);
#endif

  /*
    This is probably an error.  We can however save the situation
    at the cost of a possible mem leak, by "renaming" the share
    - First remove from hash
  */
  hash_delete(&ndbcluster_open_tables, (byte*) share);

  /*
    now give it a new name, just a running number
    if space is not enough allocate some more
  */
  {
    const uint min_key_length= 10;
    if (share->key_length < min_key_length)
    {
      share->key= alloc_root(&share->mem_root, min_key_length + 1);
      share->key_length= min_key_length;
    }
    share->key_length=
      my_snprintf(share->key, min_key_length + 1, "#leak%lu",
                  trailing_share_id++);
  }
  /* Keep it for possible the future trailing free */
  my_hash_insert(&ndbcluster_open_tables, (byte*) share);

  DBUG_RETURN(0);
}

/*
  Rename share is used during rename table.
*/
static int rename_share(NDB_SHARE *share, const char *new_key)
{
  NDB_SHARE *tmp;
  pthread_mutex_lock(&ndbcluster_mutex);
  uint new_length= (uint) strlen(new_key);
  DBUG_PRINT("rename_share", ("old_key: %s  old__length: %d",
                              share->key, share->key_length));
  if ((tmp= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                     (byte*) new_key, new_length)))
    handle_trailing_share(tmp);

  /* remove the share from hash */
  hash_delete(&ndbcluster_open_tables, (byte*) share);
  dbug_print_open_tables();

  /* save old stuff if insert should fail */
  uint old_length= share->key_length;
  char *old_key= share->key;

  /*
    now allocate and set the new key, db etc
    enough space for key, db, and table_name
  */
  share->key= alloc_root(&share->mem_root, 2 * (new_length + 1));
  strmov(share->key, new_key);
  share->key_length= new_length;

  if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
  {
    // ToDo free the allocated stuff above?
    DBUG_PRINT("error", ("rename_share: my_hash_insert %s failed",
                         share->key));
    share->key= old_key;
    share->key_length= old_length;
    if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
    {
      sql_print_error("rename_share: failed to recover %s", share->key);
      DBUG_PRINT("error", ("rename_share: my_hash_insert %s failed",
                           share->key));
    }
    dbug_print_open_tables();
    pthread_mutex_unlock(&ndbcluster_mutex);
    return -1;
  }
  dbug_print_open_tables();

  share->db= share->key + new_length + 1;
  ha_ndbcluster::set_dbname(new_key, share->db);
  share->table_name= share->db + strlen(share->db) + 1;
  ha_ndbcluster::set_tabname(new_key, share->table_name);

  dbug_print_share("rename_share:", share);
  if (share->table)
  {
    if (share->op == 0)
    {
      share->table->s->db.str= share->db;
      share->table->s->db.length= strlen(share->db);
      share->table->s->table_name.str= share->table_name;
      share->table->s->table_name.length= strlen(share->table_name);
    }
  }
  /* else rename will be handled when the ALTER event comes */
  share->old_names= old_key;
  // ToDo free old_names after ALTER EVENT

  pthread_mutex_unlock(&ndbcluster_mutex);
  return 0;
}
#endif

/*
  Increase refcount on existing share.
  Always returns share and cannot fail.
*/
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndbcluster_mutex);
  share->use_count++;

  dbug_print_open_tables();
  dbug_print_share("ndbcluster_get_share:", share);
  pthread_mutex_unlock(&ndbcluster_mutex);
  return share;
}


/*
  Get a share object for key

  Returns share for key, and increases the refcount on the share.

  create_if_not_exists == TRUE:
    creates share if it does not alreade exist
    returns 0 only due to out of memory, and then sets my_error

  create_if_not_exists == FALSE:
    returns 0 if share does not exist

  have_lock == TRUE, pthread_mutex_lock(&ndbcluster_mutex) already taken
*/

NDB_SHARE *ndbcluster_get_share(const char *key, TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock)
{
  NDB_SHARE *share;
  uint length= (uint) strlen(key);
  DBUG_ENTER("ndbcluster_get_share");
  DBUG_PRINT("enter", ("key: '%s'", key));

  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                        (byte*) key,
                                        length)))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("error", ("get_share: %s does not exist", key));
      if (!have_lock)
        pthread_mutex_unlock(&ndbcluster_mutex);
      DBUG_RETURN(0);
    }
    if ((share= (NDB_SHARE*) my_malloc(sizeof(*share),
                                       MYF(MY_WME | MY_ZEROFILL))))
    {
      MEM_ROOT **root_ptr=
        my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
      MEM_ROOT *old_root= *root_ptr;
      init_sql_alloc(&share->mem_root, 1024, 0);
      *root_ptr= &share->mem_root; // remember to reset before return
      share->state= NSS_INITIAL;
      /* enough space for key, db, and table_name */
      share->key= alloc_root(*root_ptr, 2 * (length + 1));
      share->key_length= length;
      strmov(share->key, key);
      if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
      {
        free_root(&share->mem_root, MYF(0));
        my_free((gptr) share, 0);
        *root_ptr= old_root;
        if (!have_lock)
          pthread_mutex_unlock(&ndbcluster_mutex);
        DBUG_RETURN(0);
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
      share->commit_count= 0;
      share->commit_count_lock= 0;
      share->db= share->key + length + 1;
      ha_ndbcluster::set_dbname(key, share->db);
      share->table_name= share->db + strlen(share->db) + 1;
      ha_ndbcluster::set_tabname(key, share->table_name);
#ifdef HAVE_NDB_BINLOG
      ndbcluster_binlog_init_share(share, table);
#endif
      *root_ptr= old_root;
    }
    else
    {
      DBUG_PRINT("error", ("get_share: failed to alloc share"));
      if (!have_lock)
        pthread_mutex_unlock(&ndbcluster_mutex);
      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(*share));
      DBUG_RETURN(0);
    }
  }
  share->use_count++;

  dbug_print_open_tables();
  dbug_print_share("ndbcluster_get_share:", share);
  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);
  DBUG_RETURN(share);
}


void ndbcluster_real_free_share(NDB_SHARE **share)
{
  DBUG_ENTER("ndbcluster_real_free_share");
  dbug_print_share("ndbcluster_real_free_share:", *share);

  hash_delete(&ndbcluster_open_tables, (byte*) *share);
  thr_lock_delete(&(*share)->lock);
  pthread_mutex_destroy(&(*share)->mutex);

#ifdef HAVE_NDB_BINLOG
  if ((*share)->table)
  {
    // (*share)->table->mem_root is freed by closefrm
    closefrm((*share)->table, 0);
    // (*share)->table_share->mem_root is freed by free_table_share
    free_table_share((*share)->table_share);
#ifndef DBUG_OFF
    bzero((gptr)(*share)->table_share, sizeof(*(*share)->table_share));
    bzero((gptr)(*share)->table, sizeof(*(*share)->table));
    (*share)->table_share= 0;
    (*share)->table= 0;
#endif
  }
#endif
  free_root(&(*share)->mem_root, MYF(0));
  my_free((gptr) *share, MYF(0));
  *share= 0;

  dbug_print_open_tables();
  DBUG_VOID_RETURN;
}


void ndbcluster_free_share(NDB_SHARE **share, bool have_lock)
{
  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);
  if ((*share)->util_lock == current_thd)
    (*share)->util_lock= 0;
  if (!--(*share)->use_count)
  {
    ndbcluster_real_free_share(share);
  }
  else
  {
    dbug_print_open_tables();
    dbug_print_share("ndbcluster_free_share:", *share);
  }
  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);
}


struct ndb_table_statistics_row {
  Uint64 rows;
  Uint64 commits;
  Uint32 size;
  Uint64 fixed_mem;
  Uint64 var_mem;
};

static
NdbRecord *
ndb_get_table_statistics_ndbrecord(NDBDICT *dict, const NDBTAB *table)
{
  NdbDictionary::RecordSpecification spec[5];
  spec[0].column= NdbDictionary::Column::ROW_COUNT;
  spec[0].offset= offsetof(struct ndb_table_statistics_row, rows);
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  spec[1].column= NdbDictionary::Column::COMMIT_COUNT;
  spec[1].offset= offsetof(struct ndb_table_statistics_row, commits);
  spec[1].nullbit_byte_offset= 0;
  spec[1].nullbit_bit_in_byte= 0;
  spec[2].column= NdbDictionary::Column::ROW_SIZE;
  spec[2].offset= offsetof(struct ndb_table_statistics_row, size);
  spec[2].nullbit_byte_offset= 0;
  spec[2].nullbit_bit_in_byte= 0;
  spec[3].column= NdbDictionary::Column::FRAGMENT_FIXED_MEMORY;
  spec[3].offset= offsetof(struct ndb_table_statistics_row, fixed_mem);
  spec[3].nullbit_byte_offset= 0;
  spec[3].nullbit_bit_in_byte= 0;
  spec[4].column= NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY;
  spec[4].offset= offsetof(struct ndb_table_statistics_row, var_mem);
  spec[4].nullbit_byte_offset= 0;
  spec[4].nullbit_bit_in_byte= 0;

  return dict->createRecord(table, spec,
                            sizeof(spec)/sizeof(spec[0]), sizeof(spec[0]), 0);
}

static 
int
ndb_get_table_statistics(ha_ndbcluster* file, bool report_error, Ndb* ndb,
                         const NdbRecord *record,
                         struct Ndb_statistics * ndbstat)
{
  NdbTransaction* pTrans;
  NdbError error;
  int retries= 10;
  int reterr= 0;
  int retry_sleep= 30 * 1000; /* 30 milliseconds */
  const char *row;
#ifndef DBUG_OFF
  char buff[22], buff2[22], buff3[22], buff4[22];
#endif
  DBUG_ENTER("ndb_get_table_statistics");

  DBUG_ASSERT(record != 0);

  do
  {
    Uint32 count= 0;
    Uint64 sum_rows= 0;
    Uint64 sum_commits= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    NdbScanOperation*pOp;
    int check;

    if ((pTrans= ndb->startTransaction()) == NULL)
    {
      error= ndb->getNdbError();
      goto retry;
    }

    /* Set batch_size=1, as we need only one row per fragment. */
    if ((pOp= pTrans->scanTable(record, NdbOperation::LM_CommittedRead,
                                NULL, 0, 0, 1)) == NULL)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    if (pOp->interpret_exit_last_row() == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }
    
    if (pTrans->execute(NdbTransaction::NoCommit,
                        NdbOperation::AbortOnError,
                        TRUE) == -1)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    while ((check= pOp->nextResult(row, TRUE, TRUE)) == 0)
    {
      /* NDB API ensures proper alignment of rows to make the cast valid. */
      const ndb_table_statistics_row *stat=
        (const ndb_table_statistics_row *)row;
      sum_rows+= stat->rows;
      sum_commits+= stat->commits;
      if (sum_row_size < stat->size)
        sum_row_size= stat->size;
      sum_mem+= stat->fixed_mem + stat->var_mem;
      count++;
    }
    
    if (check == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }

    pOp->close(TRUE);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->commit_count= sum_commits;
    ndbstat->row_size= sum_row_size;
    ndbstat->fragment_memory= sum_mem;

    DBUG_PRINT("exit", ("records: %s  commits: %s "
                        "row_size: %s  mem: %s count: %u",
			llstr(sum_rows, buff),
                        llstr(sum_commits, buff2),
                        llstr(sum_row_size, buff3),
                        llstr(sum_mem, buff4),
                        count));

    DBUG_RETURN(0);
retry:
    if(report_error)
    {
      if (file && pTrans)
      {
        reterr= file->ndb_err(pTrans);
      }
      else
      {
        const NdbError& tmp= error;
        ERR_PRINT(tmp);
        reterr= ndb_to_mysql_error(&tmp);
      }
    }
    else
      reterr= error.code;

    if (pTrans)
    {
      ndb->closeTransaction(pTrans);
      pTrans= NULL;
    }
    if (error.status == NdbError::TemporaryError && retries--)
    {
      my_sleep(retry_sleep);
      continue;
    }
    break;
  } while(1);
  DBUG_PRINT("exit", ("failed, reterr: %u, NdbError %u(%s)", reterr,
                      error.code, error.message));
  DBUG_RETURN(reterr);
}

/*
  Query cache stuff still call this NdbRecAttr-based version, as they
  have no easy access to a pre-computed NdbRecord for the table. Could be
  fixed to also use the NdbRecord version.
*/
static 
int
ndb_get_table_statistics(ha_ndbcluster* file, bool report_error, Ndb* ndb, const NDBTAB *ndbtab,
                         struct Ndb_statistics * ndbstat)
{
  NdbTransaction* pTrans;
  NdbError error;
  int retries= 10;
  int reterr= 0;
  int retry_sleep= 30; /* 30 milliseconds, transaction */
#ifndef DBUG_OFF
  char buff[22], buff2[22], buff3[22], buff4[22];
#endif
  DBUG_ENTER("ndb_get_table_statistics");
  DBUG_PRINT("enter", ("table: %s", ndbtab->getName()));

  DBUG_ASSERT(ndbtab != 0);

  do
  {
    Uint64 rows, commits, fixed_mem, var_mem;
    Uint32 size;
    Uint32 count= 0;
    Uint64 sum_rows= 0;
    Uint64 sum_commits= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    NdbScanOperation*pOp;
    int check;

    if ((pTrans= ndb->startTransaction()) == NULL)
    {
      error= ndb->getNdbError();
      goto retry;
    }
      
    if ((pOp= pTrans->getNdbScanOperation(ndbtab)) == NULL)
    {
      error= pTrans->getNdbError();
      goto retry;
    }

    /*
      Set batch_size = 1 to avoid allocating unnecessary NdbRecAttr's.
      We will in any case only read a single row from each fragment.
    */
    if (pOp->readTuples(NdbOperation::LM_CommittedRead, 0, 0, 1))
    {
      error= pOp->getNdbError();
      goto retry;
    }
    
    if (pOp->interpret_exit_last_row() == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }
    
    pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&rows);
    pOp->getValue(NdbDictionary::Column::COMMIT_COUNT, (char*)&commits);
    pOp->getValue(NdbDictionary::Column::ROW_SIZE, (char*)&size);
    pOp->getValue(NdbDictionary::Column::FRAGMENT_FIXED_MEMORY, 
		  (char*)&fixed_mem);
    pOp->getValue(NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY, 
		  (char*)&var_mem);
    
    if (pTrans->execute(NdbTransaction::NoCommit,
                        NdbOperation::AbortOnError,
                        TRUE) == -1)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    while ((check= pOp->nextResult(TRUE, TRUE)) == 0)
    {
      sum_rows+= rows;
      sum_commits+= commits;
      if (sum_row_size < size)
        sum_row_size= size;
      sum_mem+= fixed_mem + var_mem;
      count++;
    }
    
    if (check == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }

    pOp->close(TRUE);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->commit_count= sum_commits;
    ndbstat->row_size= sum_row_size;
    ndbstat->fragment_memory= sum_mem;

    DBUG_PRINT("exit", ("records: %s  commits: %s "
                        "row_size: %s  mem: %s count: %u",
			llstr(sum_rows, buff),
                        llstr(sum_commits, buff2),
                        llstr(sum_row_size, buff3),
                        llstr(sum_mem, buff4),
                        count));

    DBUG_RETURN(0);
retry:
    if(report_error)
    {
      if (file && pTrans)
      {
        reterr= file->ndb_err(pTrans);
      }
      else
      {
        const NdbError& tmp= error;
        ERR_PRINT(tmp);
        reterr= ndb_to_mysql_error(&tmp);
      }
    }
    else
      reterr= error.code;

    if (pTrans)
    {
      ndb->closeTransaction(pTrans);
      pTrans= NULL;
    }
    if (error.status == NdbError::TemporaryError && retries--)
    {
      my_sleep(retry_sleep);
      continue;
    }
    break;
  } while(1);
  DBUG_PRINT("exit", ("failed, reterr: %u, NdbError %u(%s)", reterr,
                      error.code, error.message));
  DBUG_RETURN(reterr);
}

/*
  Create a .ndb file to serve as a placeholder indicating 
  that the table with this name is a ndb table
*/

int ha_ndbcluster::write_ndb_file(const char *name)
{
  File file;
  bool error=1;
  char path[FN_REFLEN];
  
  DBUG_ENTER("write_ndb_file");
  DBUG_PRINT("enter", ("name: %s", name));

  (void)strxnmov(path, FN_REFLEN-1, 
                 mysql_data_home,"/",name,ha_ndb_ext,NullS);

  if ((file=my_create(path, CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    // It's an empty file
    error=0;
    my_close(file,MYF(0));
  }
  DBUG_RETURN(error);
}

void 
ha_ndbcluster::release_completed_operations(NdbTransaction *trans,
					    bool force_release)
{
  if (trans->hasBlobOperation())
  {
    /* We are reading/writing BLOB fields, 
       releasing operation records is unsafe
    */
    return;
  }
  if (!force_release)
  {
    if (get_thd_ndb(current_thd)->query_state & NDB_QUERY_MULTI_READ_RANGE)
    {
      /* We are batching reads and have not consumed all fetched
	 rows yet, releasing operation records is unsafe 
      */
      return;
    }
  }
  trans->releaseCompletedOperations();
}

bool 
ha_ndbcluster::null_value_index_search(KEY_MULTI_RANGE *ranges,
				       KEY_MULTI_RANGE *end_range,
				       HANDLER_BUFFER *buffer)
{
  DBUG_ENTER("null_value_index_search");
  KEY* key_info= table->key_info + active_index;
  KEY_MULTI_RANGE *range= ranges;
  ulong reclength= table->s->reclength;
  byte *curr= (byte*)buffer->buffer;
  byte *end_of_buffer= (byte*)buffer->buffer_end;
  
  for (; range<end_range && curr+reclength <= end_of_buffer; 
       range++)
  {
    const byte *key= range->start_key.key;
    uint key_len= range->start_key.length;
    if (check_null_in_key(key_info, key, key_len))
      DBUG_RETURN(TRUE);
    curr += reclength;
  }
  DBUG_RETURN(FALSE);
}

/*
  This is used to check if an ordered index scan is needed for a range in
  a multi range read.
  If a scan is not needed, we use a faster primary/unique key operation
  instead.
*/
static my_bool
read_multi_needs_scan(NDB_INDEX_TYPE cur_index_type, const KEY *key_info,
                      const KEY_MULTI_RANGE *r)
{
  if (cur_index_type == ORDERED_INDEX)
    return TRUE;
  if (cur_index_type == PRIMARY_KEY_INDEX ||
      cur_index_type == UNIQUE_INDEX)
    return FALSE;
  DBUG_ASSERT(cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
              cur_index_type == UNIQUE_ORDERED_INDEX);
  if (r->start_key.length != key_info->key_length ||
      r->start_key.flag != HA_READ_KEY_EXACT)
    return TRUE;                                // Not exact match, need scan
  if (cur_index_type == UNIQUE_ORDERED_INDEX &&
      check_null_in_key(key_info, r->start_key.key,r->start_key.length))
    return TRUE;                                // Can't use for NULL values
  return FALSE;
}

struct read_multi_callback_data {
  const KEY *key_info;
  const KEY_MULTI_RANGE *first_range;
  const KEY_MULTI_RANGE *range;
};

/* Callback to set up scan bounds for read multi range. */
static int
read_multi_bounds_callback(void *arg, Uint32 i,
                           NdbIndexScanOperation::IndexBound & bound)
{
  struct read_multi_callback_data *data=
    (struct read_multi_callback_data *)arg;

  /* Skip any ranges not to be included in the scan. */
  while (data->range->range_flag & (SKIP_RANGE|UNIQUE_RANGE))
    data->range++;

  compute_index_bounds(bound, data->key_info,
                       &data->range->start_key, &data->range->end_key);
  bound.range_no= data->range - data->first_range;

  data->range++;

  return 0;                                     // Success
}

int
ha_ndbcluster::read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                      KEY_MULTI_RANGE *ranges, 
                                      uint range_count,
                                      bool sorted, 
                                      HANDLER_BUFFER *buffer)
{
  KEY* key_info= table->key_info + active_index;
  NDB_INDEX_TYPE cur_index_type= get_index_type(active_index);
  ulong reclength= table_share->reclength;
  NdbOperation* op;
  Thd_ndb *thd_ndb= get_thd_ndb(current_thd);
  struct read_multi_callback_data data;

  DBUG_ENTER("ha_ndbcluster::read_multi_range_first");
  DBUG_PRINT("info", ("blob fields=%d read_set=0x%x", table_share->blob_fields, table->read_set->bitmap[0]));

  /**
   * blobs and unique hash index with NULL can't be batched currently
   */
  if (uses_blob_value(table->read_set) ||
      (cur_index_type ==  UNIQUE_INDEX &&
       has_null_in_unique_index(active_index) &&
       null_value_index_search(ranges, ranges+range_count, buffer)))
  {
    DBUG_PRINT("info", ("read_multi_range not possible, falling back to default handler implementation"));
    m_disable_multi_read= TRUE;
    DBUG_RETURN(handler::read_multi_range_first(found_range_p, 
                                                ranges, 
                                                range_count,
                                                sorted, 
                                                buffer));
  }
  thd_ndb->query_state|= NDB_QUERY_MULTI_READ_RANGE;
  m_disable_multi_read= FALSE;

  /**
   * Copy arguments into member variables
   */
  m_multi_ranges= ranges;
  multi_range_curr= ranges;
  multi_range_end= ranges+range_count;
  multi_range_sorted= sorted;
  multi_range_buffer= buffer;

  /**
   * read multi range will read ranges as follows (if not ordered)
   *
   * input    read order
   * ======   ==========
   * pk-op 1  pk-op 1
   * pk-op 2  pk-op 2
   * range 3  range (3,5) NOTE result rows will be intermixed
   * pk-op 4  pk-op 4
   * range 5
   * pk-op 6  pk-op 6
   */   

  /*
    We first loop over all ranges, converting into primary/unique key
    operations if possible, and counting ranges that require an
    ordered index scan. If the supplied HANDLER_BUFFER is too small, we
    may also need to do only part of the multi read at once.

    Afterwards, we create the ordered index scan cursor (if needed).
  */

  DBUG_ASSERT(cur_index_type != UNDEFINED_INDEX);

  const NdbOperation* lastOp= m_active_trans->getLastDefinedOperation();
  NdbOperation::LockMode lm= 
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type, table->read_set);
  byte *row_buf= (byte*)buffer->buffer;
  byte *end_of_buffer= (byte*)buffer->buffer_end;
  uint num_scan_ranges= 0;
  uint i;
  for (i= 0; i < range_count; i++)
  {
    KEY_MULTI_RANGE *r= &ranges[i];

    part_id_range part_spec;
    if (m_use_partition_function)
    {
      get_partition_set(table, table->record[0], active_index, &r->start_key,
                        &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                          part_spec.start_part, part_spec.end_part));
      /*
        If partition pruning has found no partition in set
        we can skip this scan
      */
      if (part_spec.start_part > part_spec.end_part)
      {
        /*
          We can skip this partition since the key won't fit into any
          partition
        */
        r->range_flag|= SKIP_RANGE;
        continue;
      }
    }
    r->range_flag&= ~(uint)SKIP_RANGE;

    if (read_multi_needs_scan(cur_index_type, key_info, r))
    {
      /*
        If we reach the limit of ranges allowed in a single scan: stop
        here, send what we have so far, and continue when done with that.
      */
      if (i > NdbIndexScanOperation::MaxRangeNo)
        break;

      /* Include this range in the ordered index scan. */
      r->range_flag&= ~(uint)UNIQUE_RANGE;
      num_scan_ranges++;
    }
    else
    {
      /*
        Convert to primary/unique key operation.

        If there is not enough buffer for reading the row: stop here, send
        what we have so far, and continue when done with that.
      */
      if (row_buf + reclength > end_of_buffer)
        break;

      r->range_flag|= UNIQUE_RANGE;

      if (!(op= pk_unique_index_read_key(active_index,
                                         r->start_key.key,
                                         row_buf, lm)))
        ERR_RETURN(m_active_trans->getNdbError());

      if (m_use_partition_function &&
          (cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
           cur_index_type == PRIMARY_KEY_INDEX))
        op->setPartitionId(part_spec.start_part);

      row_buf+= reclength;
    }
  }
  DBUG_ASSERT(i > 0 || i == range_count);       // Require progress
  m_multi_range_defined_end= ranges + i;
  if (num_scan_ranges > 0)
  {
    /* Do a multi-range index scan for ranges not done by primary/unique key. */
    uchar *mask;

    data.key_info= key_info;
    data.first_range= ranges;
    data.range= data.first_range;

    Uint32 flags= NdbScanOperation::SF_ReadRangeNo;
    if (lm == NdbOperation::LM_Read)
      flags|= NdbScanOperation::SF_KeyInfo;
    if (sorted)
      flags|= NdbScanOperation::SF_OrderBy;

    if (m_use_partition_function || table_share->primary_key == MAX_KEY)
    {
      mask= copy_column_set(table->read_set);
      if (table_share->primary_key == MAX_KEY)
        request_hidden_key(mask);
    }
    else
      mask= (uchar *)(table->read_set->bitmap);

    NdbIndexScanOperation *scanOp= m_active_trans->scanIndex
      (m_index[active_index].ndb_record_key, read_multi_bounds_callback,
       &data, num_scan_ranges, m_index[active_index].ndb_record_row, lm,
       mask, flags, parallelism, 0);
    if (!scanOp)
      ERR_RETURN(m_active_trans->getNdbError());
    m_active_cursor= scanOp;

    /*
      We do not get_blob_values() here, as when using blobs we always
      fallback to non-batched multi range read (see if statement at
      top of this function).
    */

    if (m_cond && m_cond->generate_scan_filter(scanOp))
      ERR_RETURN(scanOp->getNdbError());

    /* We set m_next_row=0 to say that no row was fetched from the scan yet. */
    m_next_row= 0;
  }
  else
  {
    m_active_cursor= 0;
  }

  buffer->end_of_used_area= row_buf;

  /**
   * Set first operation in multi range
   */
  m_current_multi_operation= 
    lastOp ? lastOp->next() : m_active_trans->getFirstDefinedOperation();
  if (execute_no_commit_ie(this, m_active_trans, true))
    ERR_RETURN(m_active_trans->getNdbError());

  m_multi_range_result_ptr= (byte*)buffer->buffer;

  DBUG_RETURN(read_multi_range_next(found_range_p));
}

int
ha_ndbcluster::read_multi_range_next(KEY_MULTI_RANGE ** multi_range_found_p)
{
  DBUG_ENTER("ha_ndbcluster::read_multi_range_next");
  if (m_disable_multi_read)
  {
    DBUG_RETURN(handler::read_multi_range_next(multi_range_found_p));
  }

  while (multi_range_curr < m_multi_range_defined_end)
  {
    if (multi_range_curr->range_flag & SKIP_RANGE)
    {
      /* Nothing in this range, move to next one. */
      multi_range_curr++;
    }
    else if (multi_range_curr->range_flag & UNIQUE_RANGE)
    {
      /*
        Move to next range; we can have at most one record from a unique range.
      */
      KEY_MULTI_RANGE *old_multi_range_curr= multi_range_curr;
      multi_range_curr= old_multi_range_curr + 1;
      const NdbOperation *op= m_current_multi_operation;
      m_current_multi_operation= m_active_trans->getNextCompletedOperation(op);
      byte *src_row= m_multi_range_result_ptr;
      m_multi_range_result_ptr= src_row + table_share->reclength;

      const NdbError &error= op->getNdbError();
      if (error.code == 0)
      {
        *multi_range_found_p= old_multi_range_curr;
        memcpy(table->record[0], src_row, table_share->reclength);
        if (table_share->primary_key == MAX_KEY)
        {
          m_ref= get_hidden_key(src_row);
          if (m_use_partition_function)
            m_part_id= get_partition_fragment(src_row);
        }
        DBUG_RETURN(0);
      }
      else if (error.classification != NdbError::NoDataFound)
      {
        DBUG_RETURN(ndb_err(m_active_trans));
      }

      /* No row found, so fall through to try the next range. */
    }
    else
    {
      /* An index scan range. */
      {
        int res;
        if ((res= read_multi_range_fetch_next()) != 0)
          DBUG_RETURN(res);
      }
      if (!m_next_row)
      {
        /*
          The whole scan is done, and the cursor has been closed.
          So nothing more for this range. Move to next.
        */
        multi_range_curr++;
      }
      else
      {
        int current_range_no= m_current_range_no;
        int expected_range_no;
        /*
          For a sorted index scan, we will receive rows in increasing range_no
          order, so we can return ranges in order, pausing when range_no
          indicate that the currently processed range (multi_range_curr) is
          done.

          But for unsorted scan, we may receive a high range_no from one
          fragment followed by a low range_no from another fragment. So we
          need to process all index scan ranges together.
        */
        if (!multi_range_sorted ||
            (expected_range_no= multi_range_curr - m_multi_ranges)
                == current_range_no)
        {
          *multi_range_found_p= m_multi_ranges + current_range_no;
          /* Copy out data from the new row. */
          if (table_share->primary_key == MAX_KEY)
          {
            m_ref= get_hidden_key(m_next_row);
            if (m_use_partition_function)
              m_part_id= get_partition_fragment(m_next_row);
          }
          unpack_record(table->record[0], m_next_row);
          /*
            Mark that we have used this row, so we need to fetch a new
            one on the next call.
          */
          m_next_row= 0;
          DBUG_RETURN(0);
        }
        else if (current_range_no > expected_range_no)
        {
          /* Nothing more in scan for this range. Move to next. */
          multi_range_curr++;
        }
        else
        {
          /*
            Should not happen. Ranges should be returned from NDB API in
            the order we requested them.
          */
          DBUG_ASSERT(0);
          multi_range_curr++;                     // Attempt to carry on
        }
      }
    }
  }

  if (multi_range_curr == multi_range_end)
  {
    Thd_ndb *thd_ndb= get_thd_ndb(current_thd);
    thd_ndb->query_state&= NDB_QUERY_NORMAL;
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }

  /*
    Read remaining ranges
  */
  DBUG_RETURN(read_multi_range_first(multi_range_found_p, 
                                     multi_range_curr,
                                     multi_range_end - multi_range_curr, 
                                     multi_range_sorted,
                                     multi_range_buffer));
}

/*
  Fetch next row from the ordered index cursor in multi range scan.

  We keep the next row in m_next_row, and the range_no of the
  next row in m_current_range_no. This is used in sorted index scan
  to correctly interleave rows from primary/unique key operations with
  rows from the scan.
*/
int
ha_ndbcluster::read_multi_range_fetch_next()
{
  NdbIndexScanOperation *cursor= (NdbIndexScanOperation *)m_active_cursor;

  if (!cursor)
    return 0;                                   // Scan already done.

  if (!m_next_row)
  {
    int res= fetch_next(cursor);
    if (res == 0)
    {
      m_current_range_no= cursor->get_range_no();
    }
    else if (res == 1)
    {
      /* We have fetched the last row from the scan. */
      cursor->close(FALSE, TRUE);
      m_active_cursor= 0;
      m_next_row= 0;
      return 0;
    }
    else
    {
      /* An error. */
      return res;
    }
  }
  return 0;
}

char*
ha_ndbcluster::update_table_comment(
                                /* out: table comment + additional */
        const char*     comment)/* in:  table comment defined by user */
{
  uint length= strlen(comment);
  if (length > 64000 - 3)
  {
    return((char*)comment); /* string too long */
  }

  Ndb* ndb;
  if (!(ndb= get_ndb()))
  {
    return((char*)comment);
  }

  if (ndb->setDatabaseName(m_dbname))
  {
    return((char*)comment);
  }
  const NDBTAB* tab= m_table;
  DBUG_ASSERT(tab != NULL);

  char *str;
  const char *fmt="%s%snumber_of_replicas: %d";
  const unsigned fmt_len_plus_extra= length + strlen(fmt);
  if ((str= my_malloc(fmt_len_plus_extra, MYF(0))) == NULL)
  {
    sql_print_error("ha_ndbcluster::update_table_comment: "
                    "my_malloc(%u) failed", (unsigned int)fmt_len_plus_extra);
    return (char*)comment;
  }

  my_snprintf(str,fmt_len_plus_extra,fmt,comment,
              length > 0 ? " ":"",
              tab->getReplicaCount());
  return str;
}


// Utility thread main loop
pthread_handler_t ndb_util_thread_func(void *arg __attribute__((unused)))
{
  THD *thd; /* needs to be first for thread_stack */
  struct timespec abstime;
  Thd_ndb *thd_ndb;
  uint share_list_size= 0;
  NDB_SHARE **share_list= NULL;

  my_thread_init();
  DBUG_ENTER("ndb_util_thread");
  DBUG_PRINT("enter", ("ndb_cache_check_time: %lu", ndb_cache_check_time));
 
   pthread_mutex_lock(&LOCK_ndb_util_thread);

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  if (thd == NULL)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(NULL);
  }
  THD_CHECK_SENTRY(thd);
  pthread_detach_this_thread();
  ndb_util_thread= pthread_self();

  thd->thread_stack= (char*)&thd; /* remember where our stack is */
  if (thd->store_globals())
    goto ndb_util_thread_fail;
  thd->init_for_queries();
  thd->version=refresh_version;
  thd->main_security_ctx.host_or_ip= "";
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->main_security_ctx.master_access= ~0;
  thd->main_security_ctx.priv_user = 0;
  thd->current_stmt_binlog_row_based= TRUE;     // If in mixed mode

  /* Signal successful initialization */
  ndb_util_thread_running= 1;
  pthread_cond_signal(&COND_ndb_util_ready);
  pthread_mutex_unlock(&LOCK_ndb_util_thread);

  /*
    wait for mysql server to start
  */
  pthread_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
  {
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&COND_server_started, &LOCK_server_started,
	                       &abstime);
    if (ndbcluster_terminating)
    {
      pthread_mutex_unlock(&LOCK_server_started);
      pthread_mutex_lock(&LOCK_ndb_util_thread);
      goto ndb_util_thread_end;
    }
  }
  pthread_mutex_unlock(&LOCK_server_started);

  /*
    Wait for cluster to start
  */
  pthread_mutex_lock(&LOCK_ndb_util_thread);
  while (!g_ndb_status.cluster_node_id && (ndbcluster_hton->slot != ~(uint)0))
  {
    /* ndb not connected yet */
    pthread_cond_wait(&COND_ndb_util_thread, &LOCK_ndb_util_thread);
    if (ndbcluster_terminating)
      goto ndb_util_thread_end;
  }
  pthread_mutex_unlock(&LOCK_ndb_util_thread);

  /* Get thd_ndb for this thread */
  if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
  {
    sql_print_error("Could not allocate Thd_ndb object");
    pthread_mutex_lock(&LOCK_ndb_util_thread);
    goto ndb_util_thread_end;
  }
  set_thd_ndb(thd, thd_ndb);
  thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;

#ifdef HAVE_NDB_BINLOG
  if (ndb_extra_logging && ndb_binlog_running)
    sql_print_information("NDB Binlog: Ndb tables initially read only.");
  /* create tables needed by the replication */
  ndbcluster_setup_binlog_table_shares(thd);
#else
  /*
    Get all table definitions from the storage node
  */
  ndbcluster_find_all_files(thd);
#endif

  set_timespec(abstime, 0);
  for (;;)
  {
    pthread_mutex_lock(&LOCK_ndb_util_thread);
    if (!ndbcluster_terminating)
      pthread_cond_timedwait(&COND_ndb_util_thread,
                             &LOCK_ndb_util_thread,
                             &abstime);
    if (ndbcluster_terminating) /* Shutting down server */
      goto ndb_util_thread_end;
    pthread_mutex_unlock(&LOCK_ndb_util_thread);
#ifdef NDB_EXTRA_DEBUG_UTIL_THREAD
    DBUG_PRINT("ndb_util_thread", ("Started, ndb_cache_check_time: %lu",
                                   ndb_cache_check_time));
#endif

#ifdef HAVE_NDB_BINLOG
    /*
      Check that the ndb_apply_status_share and ndb_schema_share 
      have been created.
      If not try to create it
    */
    if (!ndb_binlog_tables_inited)
      ndbcluster_setup_binlog_table_shares(thd);
#endif

    if (ndb_cache_check_time == 0)
    {
      /* Wake up in 1 second to check if value has changed */
      set_timespec(abstime, 1);
      continue;
    }

    /* Lock mutex and fill list with pointers to all open tables */
    NDB_SHARE *share;
    pthread_mutex_lock(&ndbcluster_mutex);
    uint i, open_count, record_count= ndbcluster_open_tables.records;
    if (share_list_size < record_count)
    {
      NDB_SHARE ** new_share_list= new NDB_SHARE * [record_count];
      if (!new_share_list)
      {
        sql_print_warning("ndb util thread: malloc failure, "
                          "query cache not maintained properly");
        pthread_mutex_unlock(&ndbcluster_mutex);
        goto next;                               // At least do not crash
      }
      delete [] share_list;
      share_list_size= record_count;
      share_list= new_share_list;
    }
    for (i= 0, open_count= 0; i < record_count; i++)
    {
      share= (NDB_SHARE *)hash_element(&ndbcluster_open_tables, i);
#ifdef HAVE_NDB_BINLOG
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 0)
        continue; // injector thread is the only user, skip statistics
      share->util_lock= current_thd; // Mark that util thread has lock
#endif /* HAVE_NDB_BINLOG */
      /* ndb_share reference temporary, free below */
      share->use_count++; /* Make sure the table can't be closed */
      DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                               share->key, share->use_count));
      DBUG_PRINT("ndb_util_thread",
                 ("Found open table[%d]: %s, use_count: %d",
                  i, share->table_name, share->use_count));

      /* Store pointer to table */
      share_list[open_count++]= share;
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    /* Iterate through the open files list */
    for (i= 0; i < open_count; i++)
    {
      share= share_list[i];
#ifdef HAVE_NDB_BINLOG
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 1)
      {
        /*
          Util thread and injector thread is the only user, skip statistics
	*/
        /* ndb_share reference temporary free */
        DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                 share->key, share->use_count));
        free_share(&share);
        continue;
      }
#endif /* HAVE_NDB_BINLOG */
      DBUG_PRINT("ndb_util_thread",
                 ("Fetching commit count for: %s", share->key));

      struct Ndb_statistics stat;
      uint lock;
      pthread_mutex_lock(&share->mutex);
      lock= share->commit_count_lock;
      pthread_mutex_unlock(&share->mutex);
      {
        /* Contact NDB to get commit count for table */
        Ndb* ndb= thd_ndb->ndb;
        if (ndb->setDatabaseName(share->db))
        {
          goto loop_next;
        }
        Ndb_table_guard ndbtab_g(ndb->getDictionary(), share->table_name);
        if (ndbtab_g.get_table() &&
            ndb_get_table_statistics(NULL, FALSE, ndb,
                                     ndbtab_g.get_table(), &stat) == 0)
        {
#ifndef DBUG_OFF
          char buff[22], buff2[22];
#endif
          DBUG_PRINT("info",
                     ("Table: %s  commit_count: %s  rows: %s",
                      share->key,
                      llstr(stat.commit_count, buff),
                      llstr(stat.row_count, buff2)));
        }
        else
        {
          DBUG_PRINT("ndb_util_thread",
                     ("Error: Could not get commit count for table %s",
                      share->key));
          stat.commit_count= 0;
        }
      }
  loop_next:
      pthread_mutex_lock(&share->mutex);
      if (share->commit_count_lock == lock)
        share->commit_count= stat.commit_count;
      pthread_mutex_unlock(&share->mutex);

      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
    }
next:
    /* Calculate new time to wake up */
    int secs= 0;
    int msecs= ndb_cache_check_time;

    struct timeval tick_time;
    gettimeofday(&tick_time, 0);
    abstime.tv_sec=  tick_time.tv_sec;
    abstime.tv_nsec= tick_time.tv_usec * 1000;

    if (msecs >= 1000){
      secs=  msecs / 1000;
      msecs= msecs % 1000;
    }

    abstime.tv_sec+=  secs;
    abstime.tv_nsec+= msecs * 1000000;
    if (abstime.tv_nsec >= 1000000000) {
      abstime.tv_sec+=  1;
      abstime.tv_nsec-= 1000000000;
    }
  }

  pthread_mutex_lock(&LOCK_ndb_util_thread);

ndb_util_thread_end:
  net_end(&thd->net);
ndb_util_thread_fail:
  if (share_list)
    delete [] share_list;
  thd->cleanup();
  delete thd;
  
  /* signal termination */
  ndb_util_thread_running= 0;
  pthread_cond_signal(&COND_ndb_util_ready);
  pthread_mutex_unlock(&LOCK_ndb_util_thread);
  DBUG_PRINT("exit", ("ndb_util_thread"));
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(NULL);
}

/*
  Condition pushdown
*/
/*
  Push a condition to ndbcluster storage engine for evaluation 
  during table   and index scans. The conditions will be stored on a stack
  for possibly storing several conditions. The stack can be popped
  by calling cond_pop, handler::extra(HA_EXTRA_RESET) (handler::reset())
  will clear the stack.
  The current implementation supports arbitrary AND/OR nested conditions
  with comparisons between columns and constants (including constant
  expressions and function calls) and the following comparison operators:
  =, !=, >, >=, <, <=, "is null", and "is not null".
  
  RETURN
    NULL The condition was supported and will be evaluated for each 
    row found during the scan
    cond The condition was not supported and all rows will be returned from
         the scan for evaluation (and thus not saved on stack)
*/
const 
COND* 
ha_ndbcluster::cond_push(const COND *cond) 
{ 
  DBUG_ENTER("cond_push");
  if (!m_cond) 
    m_cond= new ha_ndbcluster_cond;
  if (!m_cond)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(NULL);
  }
  DBUG_EXECUTE("where",print_where((COND *)cond, m_tabname););
  DBUG_RETURN(m_cond->cond_push(cond, table, (NDBTAB *)m_table));
}

/*
  Pop the top condition from the condition stack of the handler instance.
*/
void 
ha_ndbcluster::cond_pop() 
{ 
  if (m_cond)
    m_cond->cond_pop();
}


/*
  get table space info for SHOW CREATE TABLE
*/
char* ha_ndbcluster::get_tablespace_name(THD *thd, char* name, uint name_len)
{
  Ndb *ndb= check_ndb_in_thd(thd);
  NDBDICT *ndbdict= ndb->getDictionary();
  NdbError ndberr;
  Uint32 id;
  ndb->setDatabaseName(m_dbname);
  const NDBTAB *ndbtab= m_table;
  DBUG_ASSERT(ndbtab != NULL);
  if (!ndbtab->getTablespace(&id))
  {
    return 0;
  }
  {
    NdbDictionary::Tablespace ts= ndbdict->getTablespace(id);
    ndberr= ndbdict->getNdbError();
    if(ndberr.classification != NdbError::NoError)
      goto err;
    DBUG_PRINT("info", ("Found tablespace '%s'", ts.getName()));
    if (name)
    {
      strxnmov(name, name_len, ts.getName(), NullS);
      return name;
    }
    else
      return (my_strdup(ts.getName(), MYF(0)));
  }
err:
  if (ndberr.status == NdbError::TemporaryError)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_TEMPORARY_ERRMSG, ER(ER_GET_TEMPORARY_ERRMSG),
			ndberr.code, ndberr.message, "NDB");
  else
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
			ndberr.code, ndberr.message, "NDB");
  return 0;
}

/*
  Implements the SHOW NDB STATUS command.
*/
bool
ndbcluster_show_status(handlerton *hton, THD* thd, stat_print_fn *stat_print,
                       enum ha_stat_type stat_type)
{
  char buf[IO_SIZE];
  uint buflen;
  DBUG_ENTER("ndbcluster_show_status");
  
  if (stat_type != HA_ENGINE_STATUS)
  {
    DBUG_RETURN(FALSE);
  }

  Ndb* ndb= check_ndb_in_thd(thd);
  struct st_ndb_status ns;
  if (ndb)
    update_status_variables(&ns, get_thd_ndb(thd)->connection);
  else
    update_status_variables(&ns, g_ndb_cluster_connection);

  buflen=
    my_snprintf(buf, sizeof(buf),
                "cluster_node_id=%ld, "
                "connected_host=%s, "
                "connected_port=%ld, "
                "number_of_data_nodes=%ld, "
                "number_of_ready_data_nodes=%ld, "
                "connect_count=%ld",
                ns.cluster_node_id,
                ns.connected_host,
                ns.connected_port,
                ns.number_of_data_nodes,
                ns.number_of_ready_data_nodes,
                ns.connect_count);
  if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                 STRING_WITH_LEN("connection"), buf, buflen))
    DBUG_RETURN(TRUE);

  if (ndb)
  {
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      buflen=
        my_snprintf(buf, sizeof(buf),
                  "created=%u, free=%u, sizeof=%u",
                  tmp.m_created, tmp.m_free, tmp.m_sizeof);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     tmp.m_name, strlen(tmp.m_name), buf, buflen))
        DBUG_RETURN(TRUE);
    }
  }
#ifdef HAVE_NDB_BINLOG
  ndbcluster_show_status_binlog(thd, stat_print, stat_type);
#endif

  DBUG_RETURN(FALSE);
}


/*
  Create a table in NDB Cluster
 */
static uint get_no_fragments(ulonglong max_rows)
{
#if MYSQL_VERSION_ID >= 50000
  uint acc_row_size= 25 + /*safety margin*/ 2;
#else
  uint acc_row_size= pk_length*4;
  /* add acc overhead */
  if (pk_length <= 8)  /* main page will set the limit */
    acc_row_size+= 25 + /*safety margin*/ 2;
  else                /* overflow page will set the limit */
    acc_row_size+= 4 + /*safety margin*/ 4;
#endif
  ulonglong acc_fragment_size= 512*1024*1024;
#if MYSQL_VERSION_ID >= 50100
  return (max_rows*acc_row_size)/acc_fragment_size+1;
#else
  return ((max_rows*acc_row_size)/acc_fragment_size+1
	  +1/*correct rounding*/)/2;
#endif
}


/*
  Routine to adjust default number of partitions to always be a multiple
  of number of nodes and never more than 4 times the number of nodes.

*/
static bool adjusted_frag_count(uint no_fragments, uint no_nodes,
                                uint &reported_frags)
{
  uint i= 0;
  reported_frags= no_nodes;
  while (reported_frags < no_fragments && ++i < 4 &&
         (reported_frags + no_nodes) < MAX_PARTITIONS) 
    reported_frags+= no_nodes;
  return (reported_frags < no_fragments);
}

int ha_ndbcluster::get_default_no_partitions(HA_CREATE_INFO *create_info)
{
  ha_rows max_rows, min_rows;
  if (create_info)
  {
    max_rows= create_info->max_rows;
    min_rows= create_info->min_rows;
  }
  else
  {
    max_rows= table_share->max_rows;
    min_rows= table_share->min_rows;
  }
  uint reported_frags;
  uint no_fragments=
    get_no_fragments(max_rows >= min_rows ? max_rows : min_rows);
  uint no_nodes= g_ndb_cluster_connection->no_db_nodes();
  if (adjusted_frag_count(no_fragments, no_nodes, reported_frags))
  {
    push_warning(current_thd,
                 MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
    "Ndb might have problems storing the max amount of rows specified");
  }
  return (int)reported_frags;
}


/*
  Set-up auto-partitioning for NDB Cluster

  SYNOPSIS
    set_auto_partitions()
    part_info                  Partition info struct to set-up
 
  RETURN VALUE
    NONE

  DESCRIPTION
    Set-up auto partitioning scheme for tables that didn't define any
    partitioning. We'll use PARTITION BY KEY() in this case which
    translates into partition by primary key if a primary key exists
    and partition by hidden key otherwise.
*/

void ha_ndbcluster::set_auto_partitions(partition_info *part_info)
{
  DBUG_ENTER("ha_ndbcluster::set_auto_partitions");
  part_info->list_of_part_fields= TRUE;
  part_info->part_type= HASH_PARTITION;
  switch (opt_ndb_distribution_id)
  {
  case ND_KEYHASH:
    part_info->linear_hash_ind= FALSE;
    break;
  case ND_LINHASH:
    part_info->linear_hash_ind= TRUE;
    break;
  }
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::set_range_data(void *tab_ref, partition_info *part_info)
{
  NDBTAB *tab= (NDBTAB*)tab_ref;
  int32 *range_data= (int32*)my_malloc(part_info->no_parts*sizeof(int32),
                                       MYF(0));
  uint i;
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_range_data");

  if (!range_data)
  {
    mem_alloc_error(part_info->no_parts*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (i= 0; i < part_info->no_parts; i++)
  {
    longlong range_val= part_info->range_int_array[i];
    if (unsigned_flag)
      range_val-= 0x8000000000000000ULL;
    if (range_val < INT_MIN32 || range_val >= INT_MAX32)
    {
      if ((i != part_info->no_parts - 1) ||
          (range_val != LONGLONG_MAX))
      {
        my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
        error= 1;
        goto error;
      }
      range_val= INT_MAX32;
    }
    range_data[i]= (int32)range_val;
  }
  tab->setRangeListData(range_data, sizeof(int32)*part_info->no_parts);
error:
  my_free((char*)range_data, MYF(0));
  DBUG_RETURN(error);
}

int ha_ndbcluster::set_list_data(void *tab_ref, partition_info *part_info)
{
  NDBTAB *tab= (NDBTAB*)tab_ref;
  int32 *list_data= (int32*)my_malloc(part_info->no_list_values * 2
                                      * sizeof(int32), MYF(0));
  uint32 *part_id, i;
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_list_data");

  if (!list_data)
  {
    mem_alloc_error(part_info->no_list_values*2*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (i= 0; i < part_info->no_list_values; i++)
  {
    LIST_PART_ENTRY *list_entry= &part_info->list_array[i];
    longlong list_val= list_entry->list_value;
    if (unsigned_flag)
      list_val-= 0x8000000000000000ULL;
    if (list_val < INT_MIN32 || list_val > INT_MAX32)
    {
      my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
      error= 1;
      goto error;
    }
    list_data[2*i]= (int32)list_val;
    part_id= (uint32*)&list_data[2*i+1];
    *part_id= list_entry->partition_id;
  }
  tab->setRangeListData(list_data, 2*sizeof(int32)*part_info->no_list_values);
error:
  my_free((char*)list_data, MYF(0));
  DBUG_RETURN(error);
}

/*
  User defined partitioning set-up. We need to check how many fragments the
  user wants defined and which node groups to put those into. Later we also
  want to attach those partitions to a tablespace.

  All the functionality of the partition function, partition limits and so
  forth are entirely handled by the MySQL Server. There is one exception to
  this rule for PARTITION BY KEY where NDB handles the hash function and
  this type can thus be handled transparently also by NDB API program.
  For RANGE, HASH and LIST and subpartitioning the NDB API programs must
  implement the function to map to a partition.
*/

uint ha_ndbcluster::set_up_partition_info(partition_info *part_info,
                                          TABLE *table,
                                          void *tab_par)
{
  uint16 frag_data[MAX_PARTITIONS];
  char *ts_names[MAX_PARTITIONS];
  ulong fd_index= 0, i, j;
  NDBTAB *tab= (NDBTAB*)tab_par;
  NDBTAB::FragmentType ftype= NDBTAB::UserDefined;
  partition_element *part_elem;
  bool first= TRUE;
  uint tot_ts_name_len;
  List_iterator<partition_element> part_it(part_info->partitions);
  int error;
  DBUG_ENTER("ha_ndbcluster::set_up_partition_info");

  if (part_info->part_type == HASH_PARTITION &&
      part_info->list_of_part_fields == TRUE)
  {
    Field **fields= part_info->part_field_array;

    if (part_info->linear_hash_ind)
      ftype= NDBTAB::DistrKeyLin;
    else
      ftype= NDBTAB::DistrKeyHash;

    for (i= 0; i < part_info->part_field_list.elements; i++)
    {
      NDBCOL *col= tab->getColumn(fields[i]->field_index);
      DBUG_PRINT("info",("setting dist key on %s", col->getName()));
      col->setPartitionKey(TRUE);
    }
  }
  else 
  {
    if (!current_thd->variables.new_mode)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER(ER_ILLEGAL_HA_CREATE_OPTION),
                          ndbcluster_hton_name,
                          "LIST, RANGE and HASH partition disabled by default,"
                          " use --new option to enable");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
   /*
      Create a shadow field for those tables that have user defined
      partitioning. This field stores the value of the partition
      function such that NDB can handle reorganisations of the data
      even when the MySQL Server isn't available to assist with
      calculation of the partition function value.
    */
    NDBCOL col;
    DBUG_PRINT("info", ("Generating partition func value field"));
    col.setName("$PART_FUNC_VALUE");
    col.setType(NdbDictionary::Column::Int);
    col.setLength(1);
    col.setNullable(FALSE);
    col.setPrimaryKey(FALSE);
    col.setAutoIncrement(FALSE);
    tab->addColumn(col);
    if (part_info->part_type == RANGE_PARTITION)
    {
      if ((error= set_range_data((void*)tab, part_info)))
      {
        DBUG_RETURN(error);
      }
    }
    else if (part_info->part_type == LIST_PARTITION)
    {
      if ((error= set_list_data((void*)tab, part_info)))
      {
        DBUG_RETURN(error);
      }
    }
  }
  tab->setFragmentType(ftype);
  i= 0;
  tot_ts_name_len= 0;
  do
  {
    uint ng;
    part_elem= part_it++;
    if (!part_info->is_sub_partitioned())
    {
      ng= part_elem->nodegroup_id;
      if (first && ng == UNDEF_NODEGROUP)
        ng= 0;
      ts_names[fd_index]= part_elem->tablespace_name;
      frag_data[fd_index++]= ng;
    }
    else
    {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      j= 0;
      do
      {
        part_elem= sub_it++;
        ng= part_elem->nodegroup_id;
        if (first && ng == UNDEF_NODEGROUP)
          ng= 0;
        ts_names[fd_index]= part_elem->tablespace_name;
        frag_data[fd_index++]= ng;
      } while (++j < part_info->no_subparts);
    }
    first= FALSE;
  } while (++i < part_info->no_parts);
  tab->setDefaultNoPartitionsFlag(part_info->use_default_no_partitions);
  tab->setLinearFlag(part_info->linear_hash_ind);
  {
    ha_rows max_rows= table_share->max_rows;
    ha_rows min_rows= table_share->min_rows;
    if (max_rows < min_rows)
      max_rows= min_rows;
    if (max_rows != (ha_rows)0) /* default setting, don't set fragmentation */
    {
      tab->setMaxRows(max_rows);
      tab->setMinRows(min_rows);
    }
  }
  tab->setTablespaceNames(ts_names, fd_index*sizeof(char*));
  tab->setFragmentCount(fd_index);
  tab->setFragmentData(&frag_data, fd_index*2);
  DBUG_RETURN(0);
}


bool ha_ndbcluster::check_if_incompatible_data(HA_CREATE_INFO *create_info,
					       uint table_changes)
{
  DBUG_ENTER("ha_ndbcluster::check_if_incompatible_data");
  uint i;
  const NDBTAB *tab= (const NDBTAB *) m_table;

  if (current_thd->variables.ndb_use_copying_alter_table)
  {
    DBUG_PRINT("info", ("On-line alter table disabled"));
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  }

  int pk= 0;
  int ai= 0;

  if (create_info->tablespace)
    create_info->storage_media = HA_SM_DISK;
  else
    create_info->storage_media = HA_SM_MEMORY;

  for (i= 0; i < table->s->fields; i++) 
  {
    Field *field= table->field[i];
    const NDBCOL *col= tab->getColumn(i);
    if (col->getStorageType() == NDB_STORAGETYPE_MEMORY && create_info->storage_media != HA_SM_MEMORY ||
        col->getStorageType() == NDB_STORAGETYPE_DISK && create_info->storage_media != HA_SM_DISK)
    {
      DBUG_PRINT("info", ("Column storage media is changed"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
    
    if (field->flags & FIELD_IS_RENAMED)
    {
      DBUG_PRINT("info", ("Field has been renamed, copy table"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
    if ((field->flags & FIELD_IN_ADD_INDEX) &&
        col->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
    {
      DBUG_PRINT("info", ("add/drop index not supported for disk stored column"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
    
    if (field->flags & PRI_KEY_FLAG)
      pk=1;
    if (field->flags & FIELD_IN_ADD_INDEX)
      ai=1;
  }

  char tablespace_name[FN_LEN]; 
  if (get_tablespace_name(current_thd, tablespace_name, FN_LEN))
  {
    if (create_info->tablespace) 
    {
      if (strcmp(create_info->tablespace, tablespace_name))
      {
        DBUG_PRINT("info", ("storage media is changed, old tablespace=%s, new tablespace=%s",
          tablespace_name, create_info->tablespace));
        DBUG_RETURN(COMPATIBLE_DATA_NO);
      }
    }
    else
    {
      DBUG_PRINT("info", ("storage media is changed, old is DISK and tablespace=%s, new is MEM",
        tablespace_name));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
  }
  else
  {
    if (create_info->storage_media != HA_SM_MEMORY)
    {
      DBUG_PRINT("info", ("storage media is changed, old is MEM, new is DISK and tablespace=%s",
        create_info->tablespace));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
  }

  if (table_changes != IS_EQUAL_YES)
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  
  /**
   * Changing from/to primary key
   *
   * This is _not_ correct, but check_if_incompatible_data-interface
   *   doesnt give more info, so I guess that we can't do any
   *   online add index if not using primary key
   *
   *   This as mysql will handle a unique not null index as primary 
   *     even wo/ user specifiying it... :-(
   *   
   */
  if ((table_share->primary_key == MAX_KEY && pk) ||
      (table_share->primary_key != MAX_KEY && !pk) ||
      (table_share->primary_key == MAX_KEY && !pk && ai))
  {
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  }
  
  /* Check that auto_increment value was not changed */
  if ((create_info->used_fields & HA_CREATE_USED_AUTO) &&
      create_info->auto_increment_value != 0)
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  
  /* Check that row format didn't change */
  if ((create_info->used_fields & HA_CREATE_USED_AUTO) &&
      get_row_type() != create_info->row_type)
    DBUG_RETURN(COMPATIBLE_DATA_NO);

  DBUG_RETURN(COMPATIBLE_DATA_YES);
}

bool set_up_tablespace(st_alter_tablespace *alter_info,
                       NdbDictionary::Tablespace *ndb_ts)
{
  ndb_ts->setName(alter_info->tablespace_name);
  ndb_ts->setExtentSize(alter_info->extent_size);
  ndb_ts->setDefaultLogfileGroup(alter_info->logfile_group_name);
  return FALSE;
}

bool set_up_datafile(st_alter_tablespace *alter_info,
                     NdbDictionary::Datafile *ndb_df)
{
  if (alter_info->max_size > 0)
  {
    my_error(ER_TABLESPACE_AUTO_EXTEND_ERROR, MYF(0));
    return TRUE;
  }
  ndb_df->setPath(alter_info->data_file_name);
  ndb_df->setSize(alter_info->initial_size);
  ndb_df->setTablespace(alter_info->tablespace_name);
  return FALSE;
}

bool set_up_logfile_group(st_alter_tablespace *alter_info,
                          NdbDictionary::LogfileGroup *ndb_lg)
{
  ndb_lg->setName(alter_info->logfile_group_name);
  ndb_lg->setUndoBufferSize(alter_info->undo_buffer_size);
  return FALSE;
}

bool set_up_undofile(st_alter_tablespace *alter_info,
                     NdbDictionary::Undofile *ndb_uf)
{
  ndb_uf->setPath(alter_info->undo_file_name);
  ndb_uf->setSize(alter_info->initial_size);
  ndb_uf->setLogfileGroup(alter_info->logfile_group_name);
  return FALSE;
}

int ndbcluster_alter_tablespace(handlerton *hton,
                                THD* thd, st_alter_tablespace *alter_info)
{
  int is_tablespace= 0;
  NdbError err;
  NDBDICT *dict;
  int error;
  const char *errmsg;
  Ndb *ndb;
  DBUG_ENTER("ha_ndbcluster::alter_tablespace");
  LINT_INIT(errmsg);

  ndb= check_ndb_in_thd(thd);
  if (ndb == NULL)
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  dict= ndb->getDictionary();

  switch (alter_info->ts_cmd_type){
  case (CREATE_TABLESPACE):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
    
    NdbDictionary::Tablespace ndb_ts;
    NdbDictionary::Datafile ndb_df;
    NdbDictionary::ObjectId objid;
    if (set_up_tablespace(alter_info, &ndb_ts))
    {
      DBUG_RETURN(1);
    }
    if (set_up_datafile(alter_info, &ndb_df))
    {
      DBUG_RETURN(1);
    }
    errmsg= "TABLESPACE";
    if (dict->createTablespace(ndb_ts, &objid))
    {
      DBUG_PRINT("error", ("createTablespace returned %d", error));
      goto ndberror;
    }
    DBUG_PRINT("alter_info", ("Successfully created Tablespace"));
    errmsg= "DATAFILE";
    if (dict->createDatafile(ndb_df))
    {
      err= dict->getNdbError();
      NdbDictionary::Tablespace tmp= dict->getTablespace(ndb_ts.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropTablespace(tmp);
      }
      
      DBUG_PRINT("error", ("createDatafile returned %d", error));
      goto ndberror2;
    }
    is_tablespace= 1;
    break;
  }
  case (ALTER_TABLESPACE):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
    if (alter_info->ts_alter_tablespace_type == ALTER_TABLESPACE_ADD_FILE)
    {
      NdbDictionary::Datafile ndb_df;
      if (set_up_datafile(alter_info, &ndb_df))
      {
	DBUG_RETURN(1);
      }
      errmsg= " CREATE DATAFILE";
      if (dict->createDatafile(ndb_df))
      {
	goto ndberror;
      }
    }
    else if(alter_info->ts_alter_tablespace_type == ALTER_TABLESPACE_DROP_FILE)
    {
      NdbDictionary::Tablespace ts= dict->getTablespace(alter_info->tablespace_name);
      NdbDictionary::Datafile df= dict->getDatafile(0, alter_info->data_file_name);
      NdbDictionary::ObjectId objid;
      df.getTablespaceId(&objid);
      if (ts.getObjectId() == objid.getObjectId() && 
	  strcmp(df.getPath(), alter_info->data_file_name) == 0)
      {
	errmsg= " DROP DATAFILE";
	if (dict->dropDatafile(df))
	{
	  goto ndberror;
	}
      }
      else
      {
	DBUG_PRINT("error", ("No such datafile"));
	my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), " NO SUCH FILE");
	DBUG_RETURN(1);
      }
    }
    else
    {
      DBUG_PRINT("error", ("Unsupported alter tablespace: %d", 
			   alter_info->ts_alter_tablespace_type));
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    is_tablespace= 1;
    break;
  }
  case (CREATE_LOGFILE_GROUP):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
    NdbDictionary::LogfileGroup ndb_lg;
    NdbDictionary::Undofile ndb_uf;
    NdbDictionary::ObjectId objid;
    if (alter_info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    if (set_up_logfile_group(alter_info, &ndb_lg))
    {
      DBUG_RETURN(1);
    }
    errmsg= "LOGFILE GROUP";
    if (dict->createLogfileGroup(ndb_lg, &objid))
    {
      goto ndberror;
    }
    DBUG_PRINT("alter_info", ("Successfully created Logfile Group"));
    if (set_up_undofile(alter_info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "UNDOFILE";
    if (dict->createUndofile(ndb_uf))
    {
      err= dict->getNdbError();
      NdbDictionary::LogfileGroup tmp= dict->getLogfileGroup(ndb_lg.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropLogfileGroup(tmp);
      }
      goto ndberror2;
    }
    break;
  }
  case (ALTER_LOGFILE_GROUP):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
    if (alter_info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    NdbDictionary::Undofile ndb_uf;
    if (set_up_undofile(alter_info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "CREATE UNDOFILE";
    if (dict->createUndofile(ndb_uf))
    {
      goto ndberror;
    }
    break;
  }
  case (DROP_TABLESPACE):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "TABLESPACE";
    if (dict->dropTablespace(dict->getTablespace(alter_info->tablespace_name)))
    {
      goto ndberror;
    }
    is_tablespace= 1;
    break;
  }
  case (DROP_LOGFILE_GROUP):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "LOGFILE GROUP";
    if (dict->dropLogfileGroup(dict->getLogfileGroup(alter_info->logfile_group_name)))
    {
      goto ndberror;
    }
    break;
  }
  case (CHANGE_FILE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  case (ALTER_ACCESS_MODE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  default:
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  }
#ifdef HAVE_NDB_BINLOG
  if (is_tablespace)
    ndbcluster_log_schema_op(thd, 0,
                             thd->query, thd->query_length,
                             "", alter_info->tablespace_name,
                             0, 0,
                             SOT_TABLESPACE, 0, 0, 0);
  else
    ndbcluster_log_schema_op(thd, 0,
                             thd->query, thd->query_length,
                             "", alter_info->logfile_group_name,
                             0, 0,
                             SOT_LOGFILE_GROUP, 0, 0, 0);
#endif
  DBUG_RETURN(FALSE);

ndberror:
  err= dict->getNdbError();
ndberror2:
  ERR_PRINT(err);
  ndb_to_mysql_error(&err);
  
  my_error(error, MYF(0), errmsg);
  DBUG_RETURN(1);
}


bool ha_ndbcluster::get_no_parts(const char *name, uint *no_parts)
{
  Ndb *ndb;
  NDBDICT *dict;
  int err;
  DBUG_ENTER("ha_ndbcluster::get_no_parts");
  LINT_INIT(err);

  set_dbname(name);
  set_tabname(name);
  for (;;)
  {
    if (check_ndb_connection())
    {
      err= HA_ERR_NO_CONNECTION;
      break;
    }
    ndb= get_ndb();
    ndb->setDatabaseName(m_dbname);
    Ndb_table_guard ndbtab_g(dict= ndb->getDictionary(), m_tabname);
    if (!ndbtab_g.get_table())
      ERR_BREAK(dict->getNdbError(), err);
    *no_parts= ndbtab_g.get_table()->getFragmentCount();
    DBUG_RETURN(FALSE);
  }

  print_error(err, MYF(0));
  DBUG_RETURN(TRUE);
}

static int ndbcluster_fill_files_table(handlerton *hton, 
                                       THD *thd, 
                                       TABLE_LIST *tables,
                                       COND *cond)
{
  TABLE* table= tables->table;
  Ndb *ndb= check_ndb_in_thd(thd);
  NdbDictionary::Dictionary* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List dflist;
  NdbError ndberr;
  uint i;
  DBUG_ENTER("ndbcluster_fill_files_table");

  dict->listObjects(dflist, NdbDictionary::Object::Datafile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < dflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt = dflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    uint id;
    
    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_node(iter)))
    {
      init_fill_schema_files_row(table);
      NdbDictionary::Datafile df= dict->getDatafile(id, elt.name);
      ndberr= dict->getNdbError();
      if(ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }
      NdbDictionary::Tablespace ts= dict->getTablespace(df.getTablespace());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      table->field[IS_FILES_FILE_NAME]->set_notnull();
      table->field[IS_FILES_FILE_NAME]->store(elt.name, strlen(elt.name),
                                              system_charset_info);
      table->field[IS_FILES_FILE_TYPE]->set_notnull();
      table->field[IS_FILES_FILE_TYPE]->store("DATAFILE",8,
                                              system_charset_info);
      table->field[IS_FILES_TABLESPACE_NAME]->set_notnull();
      table->field[IS_FILES_TABLESPACE_NAME]->store(df.getTablespace(),
                                                    strlen(df.getTablespace()),
                                                    system_charset_info);
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->
        store(ts.getDefaultLogfileGroup(),
              strlen(ts.getDefaultLogfileGroup()),
              system_charset_info);
      table->field[IS_FILES_ENGINE]->set_notnull();
      table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                           ndbcluster_hton_name_length,
                                           system_charset_info);

      table->field[IS_FILES_FREE_EXTENTS]->set_notnull();
      table->field[IS_FILES_FREE_EXTENTS]->store(df.getFree()
                                                 / ts.getExtentSize());
      table->field[IS_FILES_TOTAL_EXTENTS]->set_notnull();
      table->field[IS_FILES_TOTAL_EXTENTS]->store(df.getSize()
                                                  / ts.getExtentSize());
      table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
      table->field[IS_FILES_EXTENT_SIZE]->store(ts.getExtentSize());
      table->field[IS_FILES_INITIAL_SIZE]->set_notnull();
      table->field[IS_FILES_INITIAL_SIZE]->store(df.getSize());
      table->field[IS_FILES_MAXIMUM_SIZE]->set_notnull();
      table->field[IS_FILES_MAXIMUM_SIZE]->store(df.getSize());
      table->field[IS_FILES_VERSION]->set_notnull();
      table->field[IS_FILES_VERSION]->store(df.getObjectVersion());

      table->field[IS_FILES_ROW_FORMAT]->set_notnull();
      table->field[IS_FILES_ROW_FORMAT]->store("FIXED", 5, system_charset_info);

      char extra[30];
      int len= my_snprintf(extra, sizeof(extra), "CLUSTER_NODE=%u", id);
      table->field[IS_FILES_EXTRA]->set_notnull();
      table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }

  NdbDictionary::Dictionary::List uflist;
  dict->listObjects(uflist, NdbDictionary::Object::Undofile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < uflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt= uflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    unsigned id;

    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_node(iter)))
    {
      NdbDictionary::Undofile uf= dict->getUndofile(id, elt.name);
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }
      NdbDictionary::LogfileGroup lfg=
        dict->getLogfileGroup(uf.getLogfileGroup());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      init_fill_schema_files_row(table);
      table->field[IS_FILES_FILE_NAME]->set_notnull();
      table->field[IS_FILES_FILE_NAME]->store(elt.name, strlen(elt.name),
                                              system_charset_info);
      table->field[IS_FILES_FILE_TYPE]->set_notnull();
      table->field[IS_FILES_FILE_TYPE]->store("UNDO LOG", 8,
                                              system_charset_info);
      NdbDictionary::ObjectId objid;
      uf.getLogfileGroupId(&objid);
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->store(uf.getLogfileGroup(),
                                                  strlen(uf.getLogfileGroup()),
                                                       system_charset_info);
      table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->store(objid.getObjectId());
      table->field[IS_FILES_ENGINE]->set_notnull();
      table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                           ndbcluster_hton_name_length,
                                           system_charset_info);

      table->field[IS_FILES_TOTAL_EXTENTS]->set_notnull();
      table->field[IS_FILES_TOTAL_EXTENTS]->store(uf.getSize()/4);
      table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
      table->field[IS_FILES_EXTENT_SIZE]->store(4);

      table->field[IS_FILES_INITIAL_SIZE]->set_notnull();
      table->field[IS_FILES_INITIAL_SIZE]->store(uf.getSize());
      table->field[IS_FILES_MAXIMUM_SIZE]->set_notnull();
      table->field[IS_FILES_MAXIMUM_SIZE]->store(uf.getSize());

      table->field[IS_FILES_VERSION]->set_notnull();
      table->field[IS_FILES_VERSION]->store(uf.getObjectVersion());

      char extra[100];
      int len= my_snprintf(extra,sizeof(extra),"CLUSTER_NODE=%u;UNDO_BUFFER_SIZE=%lu",
                           id, (ulong) lfg.getUndoBufferSize());
      table->field[IS_FILES_EXTRA]->set_notnull();
      table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }

  // now for LFGs
  NdbDictionary::Dictionary::List lfglist;
  dict->listObjects(lfglist, NdbDictionary::Object::LogfileGroup);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < lfglist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt= lfglist.elements[i];

    NdbDictionary::LogfileGroup lfg= dict->getLogfileGroup(elt.name);
    ndberr= dict->getNdbError();
    if (ndberr.classification != NdbError::NoError)
    {
      if (ndberr.classification == NdbError::SchemaError)
        continue;
      ERR_RETURN(ndberr);
    }

    init_fill_schema_files_row(table);
    table->field[IS_FILES_FILE_TYPE]->set_notnull();
    table->field[IS_FILES_FILE_TYPE]->store("UNDO LOG", 8,
                                            system_charset_info);

    table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
    table->field[IS_FILES_LOGFILE_GROUP_NAME]->store(elt.name,
                                                     strlen(elt.name),
                                                     system_charset_info);
    table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->set_notnull();
    table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->store(lfg.getObjectId());
    table->field[IS_FILES_ENGINE]->set_notnull();
    table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                         ndbcluster_hton_name_length,
                                         system_charset_info);

    table->field[IS_FILES_FREE_EXTENTS]->set_notnull();
    table->field[IS_FILES_FREE_EXTENTS]->store(lfg.getUndoFreeWords());
    table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
    table->field[IS_FILES_EXTENT_SIZE]->store(4);

    table->field[IS_FILES_VERSION]->set_notnull();
    table->field[IS_FILES_VERSION]->store(lfg.getObjectVersion());

    char extra[100];
    int len= my_snprintf(extra,sizeof(extra),
                         "UNDO_BUFFER_SIZE=%lu",
                         (ulong) lfg.getUndoBufferSize());
    table->field[IS_FILES_EXTRA]->set_notnull();
    table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
    schema_table_store_record(thd, table);
  }
  DBUG_RETURN(0);
}

SHOW_VAR ndb_status_variables_export[]= {
  {"Ndb",                      (char*) &ndb_status_variables,   SHOW_ARRAY},
  {NullS, NullS, SHOW_LONG}
};

struct st_mysql_storage_engine ndbcluster_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(ndbcluster)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &ndbcluster_storage_engine,
  ndbcluster_hton_name,
  "MySQL AB",
  "Clustered, fault-tolerant tables",
  PLUGIN_LICENSE_GPL,
  ndbcluster_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  ndb_status_variables_export,/* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;

#endif
