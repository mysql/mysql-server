/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include <my_alloc.h>        // MEM_ROOT
#include <thr_lock.h>        // THR_LOCK
#include <my_bitmap.h>       // MY_BITMAP
#include <mysql_com.h>       // NAME_CHAR_LEN
#include <sql_const.h>       // MAX_REF_PARTS

#include <ndbapi/Ndb.hpp>    // Ndb::TupleIdRange

enum NDB_SHARE_STATE {
  NSS_INITIAL= 0,
  NSS_DROPPED,
  NSS_ALTERED 
};


enum enum_conflict_fn_type
{
  CFT_NDB_UNDEF = 0
  ,CFT_NDB_MAX
  ,CFT_NDB_OLD
  ,CFT_NDB_MAX_DEL_WIN
  ,CFT_NDB_EPOCH
  ,CFT_NDB_EPOCH_TRANS
  ,CFT_NUMBER_OF_CFTS /* End marker */
};

#ifdef HAVE_NDB_BINLOG
static const Uint32 MAX_CONFLICT_ARGS= 8;

enum enum_conflict_fn_arg_type
{
  CFAT_END
  ,CFAT_COLUMN_NAME
  ,CFAT_EXTRA_GCI_BITS
};

struct st_conflict_fn_arg
{
  enum_conflict_fn_arg_type type;
  union
  {
    char resolveColNameBuff[ NAME_CHAR_LEN + 1 ]; // CFAT_COLUMN_NAME
    uint32 extraGciBits; // CFAT_EXTRA_GCI_BITS
  };
};

struct st_conflict_fn_arg_def
{
  enum enum_conflict_fn_arg_type arg_type;
  bool optional;
};

/* What type of operation was issued */
enum enum_conflicting_op_type
{                /* NdbApi          */
  WRITE_ROW,     /* insert (!write) */
  UPDATE_ROW,    /* update          */
  DELETE_ROW,    /* delete          */
  REFRESH_ROW    /* refresh         */
};

/*
  prepare_detect_func

  Type of function used to prepare for conflict detection on
  an NdbApi operation
*/
typedef int (* prepare_detect_func) (struct st_ndbcluster_conflict_fn_share* cfn_share,
                                     enum_conflicting_op_type op_type,
                                     const NdbRecord* data_record,
                                     const uchar* old_data,
                                     const uchar* new_data,
                                     const MY_BITMAP* write_set,
                                     class NdbInterpretedCode* code);

enum enum_conflict_fn_flags
{
  CF_TRANSACTIONAL = 1
};

struct st_conflict_fn_def
{
  const char *name;
  enum_conflict_fn_type type;
  const st_conflict_fn_arg_def* arg_defs;
  prepare_detect_func prep_func;
  uint8 flags; /* enum_conflict_fn_flags */
};

/* What sort of conflict was found */
enum enum_conflict_cause
{
  ROW_ALREADY_EXISTS,   /* On insert */
  ROW_DOES_NOT_EXIST,   /* On Update, Delete */
  ROW_IN_CONFLICT,      /* On Update, Delete */
  TRANS_IN_CONFLICT     /* Any of above, or implied by transaction */
};

/* NdbOperation custom data which points out handler and record. */
struct Ndb_exceptions_data {
  struct NDB_SHARE* share;
  const NdbRecord* key_rec;
  const uchar* row;
  enum_conflicting_op_type op_type;
  Uint64 trans_id;
};

enum enum_conflict_fn_table_flags
{
  CFF_NONE         = 0,
  CFF_REFRESH_ROWS = 1
};

/*
   Maximum supported key parts (16)
   (Ndb supports 32, but MySQL has a lower limit)
*/
static const int NDB_MAX_KEY_PARTS = MAX_REF_PARTS;

typedef struct st_ndbcluster_conflict_fn_share {
  const st_conflict_fn_def* m_conflict_fn;

  /* info about original table */
  uint8 m_pk_cols;
  uint16 m_resolve_column;
  uint8 m_resolve_size;
  uint8 m_flags;
  uint16 m_key_attrids[ NDB_MAX_KEY_PARTS ];

  const NdbDictionary::Table *m_ex_tab;
  uint32 m_count;
} NDB_CONFLICT_FN_SHARE;
#endif


/*
  Stats that can be retrieved from ndb
*/
struct Ndb_statistics {
  Uint64 row_count;
  Uint64 commit_count;
  ulong row_size;
  Uint64 fragment_memory;
  Uint64 fragment_extent_space; 
  Uint64 fragment_extent_free_space;
};


struct NDB_SHARE {
  NDB_SHARE_STATE state;
  MEM_ROOT mem_root;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *key;
  uint key_length;
  char *new_key;
  uint use_count;
  uint commit_count_lock;
  ulonglong commit_count;
  char *db;
  char *table_name;
  Ndb::TupleIdRange tuple_id_range;
  struct Ndb_statistics stat;
  struct Ndb_index_stat* index_stat_list;
  bool util_thread; // if opened by util thread
  uint32 flags;
#ifdef HAVE_NDB_BINLOG
  NDB_CONFLICT_FN_SHARE *m_cfn_share;
#endif
  class Ndb_event_data *event_data; // Place holder before NdbEventOperation is created
  class NdbEventOperation *op;
  char *old_names; // for rename table
  MY_BITMAP *subscriber_bitmap;
  class NdbEventOperation *new_op;

  static NDB_SHARE* create(const char* key, size_t key_length,
                         struct TABLE* table, const char* db_name,
                         const char* table_name);
  static void destroy(NDB_SHARE* share);

  class Ndb_event_data* get_event_data_ptr() const;

  void print(const char* where, FILE* file = stderr) const;

  /*
    Returns true if this share need to subscribe to
    events from the table.
  */
  bool need_events(bool default_on) const;
};


inline
NDB_SHARE_STATE
get_ndb_share_state(NDB_SHARE *share)
{
  NDB_SHARE_STATE state;
  pthread_mutex_lock(&share->mutex);
  state= share->state;
  pthread_mutex_unlock(&share->mutex);
  return state;
}


inline
void
set_ndb_share_state(NDB_SHARE *share, NDB_SHARE_STATE state)
{
  pthread_mutex_lock(&share->mutex);
  share->state= state;
  pthread_mutex_unlock(&share->mutex);
}


/* NDB_SHARE.flags */
#define NSF_HIDDEN_PK   1u /* table has hidden primary key */
#define NSF_BLOB_FLAG   2u /* table has blob attributes */
#define NSF_NO_BINLOG   4u /* table should not be binlogged */
#define NSF_BINLOG_FULL 8u /* table should be binlogged with full rows */
#define NSF_BINLOG_USE_UPDATE 16u  /* table update should be binlogged using
                                     update log event */
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
inline my_bool get_binlog_nologging(NDB_SHARE *share)
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
inline my_bool get_binlog_full(NDB_SHARE *share)
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
inline my_bool get_binlog_use_update(NDB_SHARE *share)
{ return (share->flags & NSF_BINLOG_USE_UPDATE) != 0; }


NDB_SHARE *ndbcluster_get_share(const char *key,
                                struct TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock);
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share);
void ndbcluster_free_share(NDB_SHARE **share, bool have_lock);
void ndbcluster_real_free_share(NDB_SHARE **share);
int handle_trailing_share(THD *thd, NDB_SHARE *share);
int ndbcluster_prepare_rename_share(NDB_SHARE *share, const char *new_key);
int ndbcluster_rename_share(THD *thd, NDB_SHARE *share);
int ndbcluster_undo_rename_share(THD *thd, NDB_SHARE *share);
void ndbcluster_mark_share_dropped(NDB_SHARE*);
inline NDB_SHARE *get_share(const char *key,
                            struct TABLE *table,
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

#endif
