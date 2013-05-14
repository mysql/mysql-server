/* Copyright (c) 2000-2003, 2006, 2007 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

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

const uint error_conflict_fn_violation= 9999;
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
                             const char *new_table_name,
                             int have_lock_open);
int ndbcluster_drop_event(THD *thd, Ndb *ndb, NDB_SHARE *share,
                          const char *type_str,
                          const char * dbname, const char * tabname);
int ndbcluster_handle_drop_table(THD *thd, Ndb *ndb, NDB_SHARE *share,
                                 const char *type_str,
                                 const char * db, const char * tabname);
void ndb_rep_event_name(String *event_name,
                        const char *db, const char *tbl, my_bool full);
#ifdef HAVE_NDB_BINLOG
int
ndbcluster_get_binlog_replication_info(THD *thd, Ndb *ndb,
                                       const char* db,
                                       const char* table_name,
                                       uint server_id,
                                       Uint32* binlog_flags,
                                       const st_conflict_fn_def** conflict_fn,
                                       st_conflict_fn_arg* args,
                                       Uint32* num_args);
int
ndbcluster_apply_binlog_replication_info(THD *thd,
                                         NDB_SHARE *share,
                                         const NDBTAB* ndbtab,
                                         const st_conflict_fn_def* conflict_fn,
                                         const st_conflict_fn_arg* args,
                                         Uint32 num_args,
                                         bool do_set_binlog_flags,
                                         Uint32 binlog_flags);
int
ndbcluster_read_binlog_replication(THD *thd, Ndb *ndb,
                                   NDB_SHARE *share,
                                   const NDBTAB *ndbtab,
                                   uint server_id,
                                   bool do_set_binlog_flags);
#endif
int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name);
int ndbcluster_binlog_start();


/*
  Setup function for the ndb binlog component. The function should be
  called on startup until it succeeds(to allow initial setup) and with
  regular intervals afterwards to reconnect after a lost cluster
  connection
*/
bool ndb_binlog_setup(THD *thd);

/*
  Will return true when the ndb binlog component is properly setup
  and ready to receive events from the cluster. As long as function
  returns false, all tables in this MySQL Server are opened in read only
  mode to avoid writes before the binlog is ready to record them.
 */
bool ndb_binlog_is_read_only(void);

extern NDB_SHARE *ndb_apply_status_share;
extern NDB_SHARE *ndb_schema_share;

extern my_bool ndb_binlog_running;

bool
ndbcluster_show_status_binlog(THD* thd, stat_print_fn *stat_print,
                              enum ha_stat_type stat_type);

/*
  prototypes for ndb handler utility function also needed by
  the ndb binlog code
*/
int cmp_frm(const NDBTAB *ndbtab, const void *pack_data,
            size_t pack_length);
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
int handle_trailing_share(THD *thd, NDB_SHARE *share, int have_lock_open= 1);
int ndbcluster_prepare_rename_share(NDB_SHARE *share, const char *new_key);
int ndbcluster_rename_share(THD *thd, NDB_SHARE *share, int have_lock_open= 1);
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

/*
   Helper for reading/writing Binlog extra row info
   in Ndb format.
   It contains an internal buffer, which can be passed
   in the thd variable when writing binlog entries if
   the object stays in scope around the write.
*/
class Ndb_binlog_extra_row_info
{
public:
  static const Uint32 FLAGS_SIZE = sizeof(Uint16);
  static const Uint32 TRANSID_SIZE = sizeof(Uint64);
  static const Uint32 MaxLen =
    EXTRA_ROW_INFO_HDR_BYTES +
    FLAGS_SIZE +
    TRANSID_SIZE;

  static const Uint64 InvalidTransactionId = ~Uint64(0);

  enum Flags
  {
    NDB_ERIF_TRANSID = 0x1
  };

  Ndb_binlog_extra_row_info();

  int loadFromBuffer(const uchar* extra_row_info_ptr);

  Uint16 getFlags() const
  {
    return flags;
  }
  void setFlags(Uint16 _flags);
  Uint64 getTransactionId() const
  { return transactionId; };
  void setTransactionId(Uint64 _transactionId);
  uchar* getBuffPtr()
  { return buff; };
  uchar* generateBuffer();
private:
  uchar buff[MaxLen];
  Uint16 flags;
  Uint64 transactionId;
};

