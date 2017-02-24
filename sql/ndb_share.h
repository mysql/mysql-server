/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_SHARE_H
#define NDB_SHARE_H

#include <stdio.h>           // FILE, stderr
#include <string>

#include "my_alloc.h"        // MEM_ROOT
#include "my_bitmap.h"       // MY_BITMAP
#include "mysql/psi/mysql_thread.h"
#include "storage/ndb/include/ndbapi/Ndb.hpp" // Ndb::TupleIdRange
#include "thr_lock.h"        // THR_LOCK

enum NDB_SHARE_STATE {
  NSS_INITIAL= 0,
  NSS_DROPPED
};


enum Ndb_binlog_type
{
  NBT_DEFAULT                   = 0
  ,NBT_NO_LOGGING               = 1
  ,NBT_UPDATED_ONLY             = 2
  ,NBT_FULL                     = 3
  ,NBT_USE_UPDATE               = 4
  ,NBT_UPDATED_ONLY_USE_UPDATE  = 6
  ,NBT_FULL_USE_UPDATE          = 7
  ,NBT_UPDATED_ONLY_MINIMAL     = 8
  ,NBT_UPDATED_FULL_MINIMAL     = 9
};


/*
  Stats that can be retrieved from ndb
*/
struct Ndb_statistics {
  Uint64 row_count;
  ulong row_size;
  Uint64 fragment_memory;
  Uint64 fragment_extent_space; 
  Uint64 fragment_extent_free_space;
};


struct NDB_SHARE {
  MY_BITMAP stored_columns;
  NDB_SHARE_STATE state;
  THR_LOCK lock;
  mysql_mutex_t mutex;
  struct NDB_SHARE_KEY* key;
  uint use_count;
  char *db;
  char *table_name;
  Ndb::TupleIdRange tuple_id_range;
  struct Ndb_statistics stat;
  struct Ndb_index_stat* index_stat_list;
  uint32 flags;
  struct NDB_CONFLICT_FN_SHARE *m_cfn_share;
  class Ndb_event_data *event_data; // Place holder before NdbEventOperation is created
  class NdbEventOperation *op;
  class NdbEventOperation *new_op;

  // Raw pointer for passing table definition from schema dist client to
  // participant in the same node to avoid that paritcipant have to access
  // the DD to open the table definition.
  const void* inplace_alter_new_table_def;

  static NDB_SHARE* create(const char* key,
                         struct TABLE* table);
  static void destroy(NDB_SHARE* share);

  class Ndb_event_data* get_event_data_ptr() const;
  void set_binlog_flags_for_table(struct TABLE *);
  void print(const char* where, FILE* file = stderr) const;

  /*
    Returns true if this share need to subscribe to
    events from the table.
  */
  bool need_events(bool default_on) const;

  // Functions for working with the opaque NDB_SHARE_KEY
  static struct NDB_SHARE_KEY* create_key(const char *new_key);
  static void free_key(struct NDB_SHARE_KEY*);

  static std::string key_get_key(struct NDB_SHARE_KEY*);
  static char* key_get_db_name(struct NDB_SHARE_KEY*);
  static char* key_get_table_name(struct NDB_SHARE_KEY*);

  size_t key_length() const;
  const char* key_string() const;

  const char* share_state_string() const;
};


inline
NDB_SHARE_STATE
get_ndb_share_state(NDB_SHARE *share)
{
  NDB_SHARE_STATE state;
  mysql_mutex_lock(&share->mutex);
  state= share->state;
  mysql_mutex_unlock(&share->mutex);
  return state;
}


inline
void
set_ndb_share_state(NDB_SHARE *share, NDB_SHARE_STATE state)
{
  mysql_mutex_lock(&share->mutex);
  share->state= state;
  mysql_mutex_unlock(&share->mutex);
}


/* NDB_SHARE.flags */
#define NSF_HIDDEN_PK   1u /* table has hidden primary key */
#define NSF_BLOB_FLAG   2u /* table has blob attributes */
#define NSF_NO_BINLOG   4u /* table should not be binlogged */
#define NSF_BINLOG_FULL 8u /* table should be binlogged with full rows */
#define NSF_BINLOG_USE_UPDATE 16u  /* table update should be binlogged using
                                     update log event */
#define NSF_BINLOG_MINIMAL_UPDATE 32u  /* table update should be binlogged using
                              minimal format: before(PK):after(changed cols) */
inline void set_binlog_logging(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_logging"));
  share->flags&= ~NSF_NO_BINLOG;
}
inline void set_binlog_nologging(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_nologging"));
  share->flags|= NSF_NO_BINLOG;
}
inline bool get_binlog_nologging(NDB_SHARE *share)
{ return (share->flags & NSF_NO_BINLOG) != 0; }
inline void set_binlog_updated_only(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_updated_only"));
  share->flags&= ~NSF_BINLOG_FULL;
}
inline void set_binlog_full(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_full"));
  share->flags|= NSF_BINLOG_FULL;
}
inline bool get_binlog_full(NDB_SHARE *share)
{ return (share->flags & NSF_BINLOG_FULL) != 0; }
inline void set_binlog_use_write(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_use_write"));
  share->flags&= ~NSF_BINLOG_USE_UPDATE;
}
inline void set_binlog_use_update(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_use_update"));
  share->flags|= NSF_BINLOG_USE_UPDATE;
}
inline bool get_binlog_use_update(NDB_SHARE *share)
{ return (share->flags & NSF_BINLOG_USE_UPDATE) != 0; }

static inline void set_binlog_update_minimal(NDB_SHARE *share)
{
  DBUG_PRINT("info", ("set_binlog_update_minimal"));
  share->flags|= NSF_BINLOG_MINIMAL_UPDATE;
}

static inline bool get_binlog_update_minimal(const NDB_SHARE *share)
{
  return (share->flags & NSF_BINLOG_MINIMAL_UPDATE) != 0;
}

NDB_SHARE *ndbcluster_get_share(const char *key,
                                struct TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock);
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share);
void ndbcluster_free_share(NDB_SHARE **share, bool have_lock);
void ndbcluster_real_free_share(NDB_SHARE **share);
int ndbcluster_rename_share(THD *thd,
                            NDB_SHARE *share,
                            struct NDB_SHARE_KEY* new_key);
void ndbcluster_mark_share_dropped(NDB_SHARE** share);
inline NDB_SHARE *get_share(const char *key,
                            struct TABLE *table,
                            bool create_if_not_exists= true,
                            bool have_lock= false)
{
  return ndbcluster_get_share(key, table, create_if_not_exists, have_lock);
}

inline NDB_SHARE *get_share(NDB_SHARE *share)
{
  return ndbcluster_get_share(share);
}

inline void free_share(NDB_SHARE **share, bool have_lock= false)
{
  ndbcluster_free_share(share, have_lock);
}

/**
   @brief Utility class for working with a temporary
          NDB_SHARE* references RAII style

          The class will automatically "get" a NDB_SHARE*
          reference and release it when going out of scope.
 */
class Ndb_share_temp_ref {
  NDB_SHARE* m_share;

  Ndb_share_temp_ref(const Ndb_share_temp_ref&); // prevent
  Ndb_share_temp_ref& operator=(const Ndb_share_temp_ref&); // prevent
public:
  Ndb_share_temp_ref(const char* key)
  {
    m_share= get_share(key, NULL, false);
     // Should always exist
    assert(m_share);
     // already existed + this temp ref
    assert(m_share->use_count >= 2);

    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             m_share->key_string(), m_share->use_count));
  }

  ~Ndb_share_temp_ref()
  {
    /* release the temporary reference */
    assert(m_share);
    // at least  this temp ref
    assert(m_share->use_count > 0);

    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             m_share->key_string(), m_share->use_count));

    free_share(&m_share);
  }

  // Return the NDB_SHARE* by type conversion operator
  operator NDB_SHARE*() const
  {
    assert(m_share);
    return m_share;
  }

  // Return the NDB_SHARE* when using pointer operator
  const NDB_SHARE* operator->() const
  {
    assert(m_share);
    return m_share;
  }
};


#define dbug_print_share(t, s)                  \
  DBUG_LOCK_FILE;                               \
  DBUG_EXECUTE("info",                          \
               (s)->print((t), DBUG_FILE););    \
  DBUG_UNLOCK_FILE;

#endif
