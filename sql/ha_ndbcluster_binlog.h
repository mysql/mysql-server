/*
   Copyright (C) 2000-2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Typedefs for long names
typedef NdbDictionary::Object NDBOBJ;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index  NDBINDEX;
typedef NdbDictionary::Dictionary  NDBDICT;
typedef NdbDictionary::Event  NDBEVENT;

#define IS_TMP_PREFIX(A) (is_prefix(A, tmp_file_prefix))

#define INJECTOR_EVENT_LEN 200

#define NDB_INVALID_SCHEMA_OBJECT 241

extern handlerton *ndbcluster_hton;

class Ndb_event_data
{
public:
  Ndb_event_data(NDB_SHARE *the_share) : 
    table_share((TABLE_SHARE *)0), table((TABLE *)0), share(the_share)
  {
    ndb_value[0]= 0;
    ndb_value[1]= 0;
  }
  ~Ndb_event_data()
  {
    if (table)
      closefrm(table, 1);
    free_root(&mem_root, MYF(0));
    table_share= 0;
    table= 0;
    share= 0;
    /*
       ndbvalue[] allocated with my_multi_malloc
       so only first pointer should be freed  
    */
    my_free(ndb_value[0], MYF(MY_WME|MY_ALLOW_ZERO_PTR));
  }
  MEM_ROOT mem_root;
  TABLE_SHARE *table_share;
  TABLE *table;
  NDB_SHARE *share;
  NdbValue *ndb_value[2];
};

/*
  The numbers below must not change as they
  are passed between mysql servers, and if changed
  would break compatablility.  Add new numbers to
  the end.
*/
enum SCHEMA_OP_TYPE
{
  SOT_DROP_TABLE= 0,
  SOT_CREATE_TABLE= 1,
  SOT_RENAME_TABLE_NEW= 2,
  SOT_ALTER_TABLE_COMMIT= 3,
  SOT_DROP_DB= 4,
  SOT_CREATE_DB= 5,
  SOT_ALTER_DB= 6,
  SOT_CLEAR_SLOCK= 7,
  SOT_TABLESPACE= 8,
  SOT_LOGFILE_GROUP= 9,
  SOT_RENAME_TABLE= 10,
  SOT_TRUNCATE_TABLE= 11,
  SOT_RENAME_TABLE_PREPARE= 12,
  SOT_ONLINE_ALTER_TABLE_PREPARE= 13,
  SOT_ONLINE_ALTER_TABLE_COMMIT= 14
};

const uint max_ndb_nodes= 256; /* multiple of 32 */

static const char *ha_ndb_ext=".ndb";

#ifdef HAVE_NDB_BINLOG
#define NDB_EXCEPTIONS_TABLE_SUFFIX "$EX"
#define NDB_EXCEPTIONS_TABLE_SUFFIX_LOWER "$ex"

const uint error_conflict_fn_old_violation= 9998;
const uint error_conflict_fn_max_violation= 9999;
#endif /* HAVE_NDB_BINLOG */


class Mutex_guard
{
public:
  Mutex_guard(pthread_mutex_t &mutex) : m_mutex(mutex)
  {
    pthread_mutex_lock(&m_mutex);
  };
  ~Mutex_guard()
  {
    pthread_mutex_unlock(&m_mutex);
  };
private:
  pthread_mutex_t &m_mutex;
};

class Ndb_table_guard
{
public:
  Ndb_table_guard(NDBDICT *dict)
    : m_dict(dict), m_ndbtab(NULL), m_invalidate(0)
  {}
  Ndb_table_guard(NDBDICT *dict, const char *tabname)
    : m_dict(dict), m_ndbtab(NULL), m_invalidate(0)
  {
    DBUG_ENTER("Ndb_table_guard");
    init(tabname);
    DBUG_VOID_RETURN;
  }
  ~Ndb_table_guard()
  {
    DBUG_ENTER("~Ndb_table_guard");
    reinit();
    DBUG_VOID_RETURN;
  }
  void init(const char *tabname)
  {
    DBUG_ENTER("Ndb_table_guard::init");
    /* must call reinit() if already initialized */
    DBUG_ASSERT(m_ndbtab == NULL);
    m_ndbtab= m_dict->getTableGlobal(tabname);
    m_invalidate= 0;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_VOID_RETURN;
  }
  void reinit(const char *tabname= 0)
  {
    DBUG_ENTER("Ndb_table_guard::reinit");
    if (m_ndbtab)
    {
      DBUG_PRINT("info", ("m_ndbtab: %p  m_invalidate: %d",
                          m_ndbtab, m_invalidate));
      m_dict->removeTableGlobal(*m_ndbtab, m_invalidate);
      m_ndbtab= NULL;
      m_invalidate= 0;
    }
    if (tabname)
      init(tabname);
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_VOID_RETURN;
  }
  const NDBTAB *get_table() { return m_ndbtab; }
  void invalidate() { m_invalidate= 1; }
  const NDBTAB *release()
  {
    DBUG_ENTER("Ndb_table_guard::release");
    const NDBTAB *tmp= m_ndbtab;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    m_ndbtab = 0;
    DBUG_RETURN(tmp);
  }
private:
  NDBDICT *m_dict;
  const NDBTAB *m_ndbtab;
  int m_invalidate;
};

class Thd_proc_info_guard
{
public:
  Thd_proc_info_guard(THD *thd)
   : m_thd(thd), m_proc_info(thd->proc_info) {}
  ~Thd_proc_info_guard() { m_thd->proc_info= m_proc_info; }
private:
  THD *m_thd;
  const char *m_proc_info;
};

extern Ndb_cluster_connection* g_ndb_cluster_connection;

void ndbcluster_global_schema_lock_init();
void ndbcluster_global_schema_lock_deinit();

extern unsigned char g_node_id_map[max_ndb_nodes];
extern pthread_mutex_t LOCK_ndb_util_thread;
extern pthread_cond_t COND_ndb_util_thread;
extern pthread_mutex_t ndbcluster_mutex;
extern HASH ndbcluster_open_tables;

/*
  Initialize the binlog part of the ndb handlerton
*/
void ndbcluster_binlog_init_handlerton();
/*
  Initialize the binlog part of the NDB_SHARE
*/
int ndbcluster_binlog_init_share(THD *thd, NDB_SHARE *share, TABLE *table);
int ndbcluster_create_binlog_setup(THD *thd, Ndb *ndb, const char *key,
                                   uint key_len,
                                   const char *db,
                                   const char *table_name,
                                   my_bool share_may_exist);
int ndbcluster_create_event(THD *thd, Ndb *ndb, const NDBTAB *table,
                            const char *event_name, NDB_SHARE *share,
                            int push_warning= 0);
int ndbcluster_create_event_ops(THD *thd,
                                NDB_SHARE *share,
                                const NDBTAB *ndbtab,
                                const char *event_name);
int ndbcluster_log_schema_op(THD *thd,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             enum SCHEMA_OP_TYPE type,
                             const char *new_db,
                             const char *new_table_name);
int ndbcluster_drop_event(THD *thd, Ndb *ndb, NDB_SHARE *share,
                          const char *type_str,
                          const char * dbname, const char * tabname);
int ndbcluster_handle_alter_table(THD *thd, NDB_SHARE *share,
                                  const char *type_str);
int ndbcluster_handle_drop_table(THD *thd, Ndb *ndb, NDB_SHARE *share,
                                 const char *type_str,
                                 const char * db, const char * tabname);
void ndb_rep_event_name(String *event_name,
                        const char *db, const char *tbl, my_bool full);
#ifdef HAVE_NDB_BINLOG
int
ndbcluster_read_binlog_replication(THD *thd, Ndb *ndb,
                                   NDB_SHARE *share,
                                   const NDBTAB *ndbtab,
                                   uint server_id,
                                   TABLE *table,
                                   bool do_set_binlog_flags);
#endif
int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name);
int ndbcluster_binlog_start();
pthread_handler_t ndb_binlog_thread_func(void *arg);

/*
  table mysql.ndb_apply_status
*/
int ndbcluster_setup_binlog_table_shares(THD *thd);
extern NDB_SHARE *ndb_apply_status_share;
extern NDB_SHARE *ndb_schema_share;

extern THD *injector_thd;
extern my_bool ndb_binlog_running;
extern my_bool ndb_binlog_tables_inited;
extern my_bool ndb_binlog_is_ready;

bool
ndbcluster_show_status_binlog(THD* thd, stat_print_fn *stat_print,
                              enum ha_stat_type stat_type);

/*
  prototypes for ndb handler utility function also needed by
  the ndb binlog code
*/
int cmp_frm(const NDBTAB *ndbtab, const void *pack_data,
            uint pack_length);
int ndbcluster_find_all_files(THD *thd);

char *ndb_pack_varchar(const NDBCOL *col, char *buf,
                       const char *str, int sz);

NDB_SHARE *ndbcluster_get_share(const char *key,
                                TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock);
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share);
void ndbcluster_free_share(NDB_SHARE **share, bool have_lock);
void ndbcluster_real_free_share(NDB_SHARE **share);
int handle_trailing_share(THD *thd, NDB_SHARE *share);
int ndbcluster_prepare_rename_share(NDB_SHARE *share, const char *new_key);
int ndbcluster_rename_share(THD *thd, NDB_SHARE *share);
int ndbcluster_undo_rename_share(THD *thd, NDB_SHARE *share);
inline NDB_SHARE *get_share(const char *key,
                            TABLE *table,
                            bool create_if_not_exists= TRUE,
                            bool have_lock= FALSE)
{
  return ndbcluster_get_share(key, table, create_if_not_exists, have_lock);
}

inline NDB_SHARE *get_share(NDB_SHARE *share)
{
  return ndbcluster_get_share(share);
}

inline void free_share(NDB_SHARE **share, bool have_lock= FALSE)
{
  ndbcluster_free_share(share, have_lock);
}

void set_binlog_flags(NDB_SHARE *share);

/*
  Helper functions
*/
bool
ndbcluster_check_if_local_table(const char *dbname, const char *tabname);
bool
ndbcluster_check_if_local_tables_in_db(THD *thd, const char *dbname);

inline
Thd_ndb *
get_thd_ndb(THD *thd)
{ return (Thd_ndb *) thd_get_ha_data(thd, ndbcluster_hton); }

inline
void
set_thd_ndb(THD *thd, Thd_ndb *thd_ndb)
{ thd_set_ha_data(thd, ndbcluster_hton, thd_ndb); }

Ndb* check_ndb_in_thd(THD* thd, bool validate_ndb= false);

bool ndbcluster_has_global_schema_lock(Thd_ndb *thd_ndb);
int ndbcluster_no_global_schema_lock_abort(THD *thd, const char *msg);

class Ndbcluster_global_schema_lock_guard
{
public:
  Ndbcluster_global_schema_lock_guard(THD *thd);
  ~Ndbcluster_global_schema_lock_guard();
  int lock();
private:
  THD *m_thd;
  int m_lock;
};


bool ndbcluster_anyvalue_is_reserved(Uint32 anyValue);
bool ndbcluster_anyvalue_is_nologging(Uint32 anyValue);
void ndbcluster_anyvalue_set_nologging(Uint32& anyValue);
bool ndbcluster_anyvalue_is_serverid_in_range(Uint32 serverId);
void ndbcluster_anyvalue_set_normal(Uint32& anyValue);
Uint32 ndbcluster_anyvalue_get_serverid(Uint32 anyValue);
void ndbcluster_anyvalue_set_serverid(Uint32& anyValue, Uint32 serverId);

#ifndef DBUG_OFF
void dbug_ndbcluster_anyvalue_set_userbits(Uint32& anyValue);
#endif
