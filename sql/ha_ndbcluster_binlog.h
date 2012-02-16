#ifndef HA_NDBCLUSTER_BINLOG_INCLUDED
#define HA_NDBCLUSTER_BINLOG_INCLUDED

/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_class.h"                      /* THD */

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

/* server id's with high bit set is reservered */
#define NDB_ANYVALUE_FOR_NOLOGGING 0xFFFFFFFF
#define NDB_ANYVALUE_RESERVED      0x80000000

extern handlerton *ndbcluster_hton;

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
  SOT_ALTER_TABLE= 3,
  SOT_DROP_DB= 4,
  SOT_CREATE_DB= 5,
  SOT_ALTER_DB= 6,
  SOT_CLEAR_SLOCK= 7,
  SOT_TABLESPACE= 8,
  SOT_LOGFILE_GROUP= 9,
  SOT_RENAME_TABLE= 10,
  SOT_TRUNCATE_TABLE= 11
};

const uint max_ndb_nodes= 64; /* multiple of 32 */

static const char *ha_ndb_ext=".ndb";
static const char share_prefix[]= "./";

class Ndb_table_guard
{
public:
  Ndb_table_guard(NDBDICT *dict, const char *tabname)
    : m_dict(dict)
  {
    DBUG_ENTER("Ndb_table_guard");
    m_ndbtab= m_dict->getTableGlobal(tabname);
    m_invalidate= 0;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_VOID_RETURN;
  }
  ~Ndb_table_guard()
  {
    DBUG_ENTER("~Ndb_table_guard");
    if (m_ndbtab)
    {
      DBUG_PRINT("info", ("m_ndbtab: %p  m_invalidate: %d",
                          m_ndbtab, m_invalidate));
      m_dict->removeTableGlobal(*m_ndbtab, m_invalidate);
    }
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
  const NDBTAB *m_ndbtab;
  NDBDICT *m_dict;
  int m_invalidate;
};

#ifdef HAVE_NDB_BINLOG

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_injector_mutex, key_ndb_schema_share_mutex,
                     key_ndb_schema_object_mutex;
extern PSI_cond_key key_injector_cond;
extern PSI_thread_key key_thread_ndb_binlog;
#endif /* HAVE_PSI_INTERFACE */

extern pthread_t ndb_binlog_thread;
extern mysql_mutex_t injector_mutex;
extern mysql_cond_t  injector_cond;

extern unsigned char g_node_id_map[max_ndb_nodes];
extern pthread_t ndb_util_thread;
extern mysql_mutex_t LOCK_ndb_util_thread;
extern mysql_cond_t COND_ndb_util_thread;
extern int ndbcluster_util_inited;
extern mysql_mutex_t ndbcluster_mutex;
extern HASH ndbcluster_open_tables;
extern Ndb_cluster_connection* g_ndb_cluster_connection;
extern long ndb_number_of_storage_nodes;

/*
  Initialize the binlog part of the ndb handlerton
*/
void ndbcluster_binlog_init_handlerton();
/*
  Initialize the binlog part of the NDB_SHARE
*/
int ndbcluster_binlog_init_share(NDB_SHARE *share, TABLE *table);

bool ndbcluster_check_if_local_table(const char *dbname, const char *tabname);
bool ndbcluster_check_if_local_tables_in_db(THD *thd, const char *dbname);

int ndbcluster_create_binlog_setup(Ndb *ndb, const char *key,
                                   uint key_len,
                                   const char *db,
                                   const char *table_name,
                                   my_bool share_may_exist);
int ndbcluster_create_event(Ndb *ndb, const NDBTAB *table,
                            const char *event_name, NDB_SHARE *share,
                            int push_warning= 0);
int ndbcluster_create_event_ops(NDB_SHARE *share,
                                const NDBTAB *ndbtab,
                                const char *event_name);
int ndbcluster_log_schema_op(THD *thd, NDB_SHARE *share,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             enum SCHEMA_OP_TYPE type,
                             const char *new_db,
                             const char *new_table_name);
int ndbcluster_handle_drop_table(Ndb *ndb, const char *event_name,
                                 NDB_SHARE *share,
                                 const char *type_str);
void ndb_rep_event_name(String *event_name,
                        const char *db, const char *tbl);
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
#endif /* HAVE_NDB_BINLOG */

void ndb_unpack_record(TABLE *table, NdbValue *value,
                       MY_BITMAP *defined, uchar *buf);
char *ndb_pack_varchar(const NDBCOL *col, char *buf,
                       const char *str, int sz);

NDB_SHARE *ndbcluster_get_share(const char *key,
                                TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock);
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share);
void ndbcluster_free_share(NDB_SHARE **share, bool have_lock);
void ndbcluster_real_free_share(NDB_SHARE **share);
int handle_trailing_share(NDB_SHARE *share);
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

inline
Thd_ndb *
get_thd_ndb(THD *thd)
{ return (Thd_ndb *) thd_get_ha_data(thd, ndbcluster_hton); }

inline
void
set_thd_ndb(THD *thd, Thd_ndb *thd_ndb)
{ thd_set_ha_data(thd, ndbcluster_hton, thd_ndb); }

Ndb* check_ndb_in_thd(THD* thd);

#endif /* HA_NDBCLUSTER_BINLOG_INCLUDED */
