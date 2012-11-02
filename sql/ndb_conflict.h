/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_CONFLICT_H
#define NDB_CONFLICT_H

#include "ndb_conflict_trans.h"
#include <ndbapi/NdbDictionary.hpp>
#include <ndbapi/NdbTransaction.hpp>

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
  Room for 10 instruction words, two labels (@ 2words/label)
  + 2 extra words for the case of resolve_size == 8
*/
#define MAX_CONFLICT_INTERPRETED_PROG_SIZE 16

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

/**
   ExceptionsTableWriter

   Helper class for inserting entries into an exceptions
   table
*/
class ExceptionsTableWriter
{
public:
  ExceptionsTableWriter()
    :m_pk_cols(0), m_ex_tab(NULL), m_count(0)
  {};

  ~ExceptionsTableWriter()
  {};

  /**
     hasTable

     Returns true if there is an Exceptions table
  */
  bool hasTable() const
  {
    return m_ex_tab != NULL;
  };

  /**
    init

    Initialise ExceptionsTableWriter with main and exceptions
    tables.

    May set a warning message on success or error.
  */
  int init(const NdbDictionary::Table* mainTable,
           const NdbDictionary::Table* exceptionsTable,
           char* msg_buf,
           uint msg_buf_len,
           const char** msg);

  /**
     free

     Release reference to exceptions table
  */
  void free(Ndb* ndb);

  /**
     writeRow

     Write a row to the Exceptions Table for the given
     key
  */
  int writeRow(NdbTransaction* trans,
               const NdbRecord* keyRecord,
               uint32 server_id,
               uint32 master_server_id,
               uint64 master_epoch,
               const uchar* rowPtr,
               NdbError& err);

private:
  /* info about original table */
  uint8 m_pk_cols;
  uint16 m_key_attrids[ NDB_MAX_KEY_PARTS ];

  const NdbDictionary::Table *m_ex_tab;
  uint32 m_count;
};

typedef struct st_ndbcluster_conflict_fn_share {
  const st_conflict_fn_def* m_conflict_fn;

  /* info about original table */
  uint16 m_resolve_column;
  uint8 m_resolve_size;
  uint8 m_flags;

  ExceptionsTableWriter m_ex_tab_writer;
} NDB_CONFLICT_FN_SHARE;


/* HAVE_NDB_BINLOG */
#endif

enum enum_slave_trans_conflict_apply_state
{
  /* Normal with optional row-level conflict detection */
  SAS_NORMAL,

  /*
    SAS_TRACK_TRANS_DEPENDENCIES
    Track inter-transaction dependencies
  */
  SAS_TRACK_TRANS_DEPENDENCIES,

  /*
    SAS_APPLY_TRANS_DEPENDENCIES
    Apply only non conflicting transactions
  */
  SAS_APPLY_TRANS_DEPENDENCIES
};

enum enum_slave_conflict_flags
{
  /* Conflict detection Ops defined */
  SCS_OPS_DEFINED = 1,
  /* Conflict detected on table with transactional resolution */
  SCS_TRANS_CONFLICT_DETECTED_THIS_PASS = 2
};

/*
  State associated with the Slave thread
  (From the Ndb handler's point of view)
*/
struct st_ndb_slave_state
{
  /* Counter values for current slave transaction */
  Uint32 current_violation_count[CFT_NUMBER_OF_CFTS];
  Uint64 current_master_server_epoch;
  Uint64 current_max_rep_epoch;
  uint8 conflict_flags; /* enum_slave_conflict_flags */
    /* Transactional conflict detection */
  Uint32 retry_trans_count;
  Uint32 current_trans_row_conflict_count;
  Uint32 current_trans_row_reject_count;
  Uint32 current_trans_in_conflict_count;

  /* Cumulative counter values */
  Uint64 total_violation_count[CFT_NUMBER_OF_CFTS];
  Uint64 max_rep_epoch;
  Uint32 sql_run_id;
  /* Transactional conflict detection */
  Uint64 trans_row_conflict_count;
  Uint64 trans_row_reject_count;
  Uint64 trans_detect_iter_count;
  Uint64 trans_in_conflict_count;
  Uint64 trans_conflict_commit_count;

  static const Uint32 MAX_RETRY_TRANS_COUNT = 100;

  /*
    Slave Apply State

    State of Binlog application from Ndb point of view.
  */
  enum_slave_trans_conflict_apply_state trans_conflict_apply_state;

  MEM_ROOT conflict_mem_root;
  class DependencyTracker* trans_dependency_tracker;

  /* Methods */
  void atStartSlave();
  int  atPrepareConflictDetection(const NdbDictionary::Table* table,
                                  const NdbRecord* key_rec,
                                  const uchar* row_data,
                                  Uint64 transaction_id,
                                  bool& handle_conflict_now);
  int  atTransConflictDetected(Uint64 transaction_id);
  int  atConflictPreCommit(bool& retry_slave_trans);

  void atBeginTransConflictHandling();
  void atEndTransConflictHandling();

  void atTransactionCommit();
  void atTransactionAbort();
  void atResetSlave();

  void atApplyStatusWrite(Uint32 master_server_id,
                          Uint32 row_server_id,
                          Uint64 row_epoch,
                          bool is_row_server_id_local);

  void resetPerAttemptCounters();

  st_ndb_slave_state();
};


/* NDB_CONFLICT_H */
#endif
