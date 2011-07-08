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
  const char *ptr;
  uint32 len;
  union
  {
    uint32 fieldno;      // CFAT_COLUMN_NAME
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
  DELETE_ROW     /* delete          */
};

/*
  prepare_detect_func

  Type of function used to prepare for conflict detection on
  an NdbApi operation
*/
typedef int (* prepare_detect_func) (struct NDB_CONFLICT_FN_SHARE* cfn_share,
                                     enum_conflicting_op_type op_type,
                                     const uchar* old_data,
                                     const uchar* new_data,
                                     const MY_BITMAP* write_set,
                                     class NdbInterpretedCode* code);

struct st_conflict_fn_def
{
  const char *name;
  enum_conflict_fn_type type;
  const st_conflict_fn_arg_def* arg_defs;
  prepare_detect_func prep_func;
};

/* What sort of conflict was found */
enum enum_conflict_cause
{
  ROW_ALREADY_EXISTS,
  ROW_DOES_NOT_EXIST,
  ROW_IN_CONFLICT
};

/* NdbOperation custom data which points out handler and record. */
struct Ndb_exceptions_data {
  struct NDB_SHARE* share;
  const NdbRecord* key_rec;
  const uchar* row;
  enum_conflicting_op_type op_type;
};

enum enum_conflict_fn_flags
{
  CFF_NONE = 0,
  CFF_REFRESH_ROWS = 1
};

struct NDB_CONFLICT_FN_SHARE {
  const st_conflict_fn_def* m_conflict_fn;

  /* info about original table */
  uint8 m_pk_cols;
  uint8 m_resolve_column;
  uint8 m_resolve_size;
  uint8 m_flags;
  uint16 m_offset[16];
  uint16 m_resolve_offset;

  const NdbDictionary::Table *m_ex_tab;
  uint32 m_count;
};
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
  uint32 connect_count;
  uint32 flags;
#ifdef HAVE_NDB_BINLOG
  NDB_CONFLICT_FN_SHARE *m_cfn_share;
#endif
  class Ndb_event_data *event_data; // Place holder before NdbEventOperation is created
  class NdbEventOperation *op;
  char *old_names; // for rename table
  MY_BITMAP *subscriber_bitmap;
  class NdbEventOperation *new_op;
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

#endif
