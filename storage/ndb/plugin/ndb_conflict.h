/*
   Copyright (c) 2012, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_CONFLICT_H
#define NDB_CONFLICT_H

#include <unordered_set>
#include "my_bitmap.h"
#include "mysql/plugin.h"   // SHOW_VAR
#include "mysql_com.h"      // NAME_CHAR_LEN
#include "sql/sql_const.h"  // MAX_REF_PARTS
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/NdbTransaction.hpp"
#include "storage/ndb/plugin/ndb_conflict_trans.h"

enum enum_conflict_fn_type {
  CFT_NDB_UNDEF = 0,
  CFT_NDB_MAX,
  CFT_NDB_OLD,
  CFT_NDB_MAX_DEL_WIN,
  CFT_NDB_MAX_INS,
  CFT_NDB_MAX_DEL_WIN_INS,
  CFT_NDB_EPOCH,
  CFT_NDB_EPOCH_TRANS,
  CFT_NDB_EPOCH2,
  CFT_NDB_EPOCH2_TRANS,
  CFT_NUMBER_OF_CFTS /* End marker */
};

/**
 * Definitions used when setting the conflict flags
 * member of the 'extra row info' on a Binlog row
 * event
 */
enum enum_binlog_extra_info_conflict_flags {
  NDB_ERIF_CFT_REFLECT_OP = 0x1,
  NDB_ERIF_CFT_REFRESH_OP = 0x2,
  NDB_ERIF_CFT_READ_OP = 0x4
};

static const uint MAX_CONFLICT_ARGS = 8;

enum enum_conflict_fn_arg_type {
  CFAT_END,
  CFAT_COLUMN_NAME,
  CFAT_EXTRA_GCI_BITS
};

struct st_conflict_fn_arg {
  enum_conflict_fn_arg_type type;
  union {
    char resolveColNameBuff[NAME_CHAR_LEN + 1];  // CFAT_COLUMN_NAME
    uint32 extraGciBits;                         // CFAT_EXTRA_GCI_BITS
  };
};

struct st_conflict_fn_arg_def {
  enum enum_conflict_fn_arg_type arg_type;
  bool optional;
};

/* What type of operation was issued */
enum enum_conflicting_op_type {                  /* NdbApi          */
                                WRITE_ROW = 1,   /* insert (!write) */
                                UPDATE_ROW = 2,  /* update          */
                                DELETE_ROW = 3,  /* delete          */
                                REFRESH_ROW = 4, /* refresh         */
                                READ_ROW = 5     /* read tracking   */
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
typedef int (*prepare_detect_func)(struct NDB_CONFLICT_FN_SHARE *cfn_share,
                                   enum_conflicting_op_type op_type,
                                   const NdbRecord *data_record,
                                   const uchar *old_data, const uchar *new_data,
                                   /* Before image columns bitmap */
                                   const MY_BITMAP *bi_cols,
                                   /* After image columns bitmap */
                                   const MY_BITMAP *ai_cols,
                                   class NdbInterpretedCode *code);

/**
 * enum_conflict_fn_flags
 *
 * These are 'features' of a particular conflict resolution algorithm, not
 * controlled on a per-table basis.
 * TODO : Encapsulate all these per-algorithm details inside the algorithm
 */
enum enum_conflict_fn_flags {
  CF_TRANSACTIONAL = 0x1,    /* Conflicts are handled per transaction */
  CF_REFLECT_SEC_OPS = 0x2,  /* Secondary operations are reflected back */
  CF_USE_ROLE_VAR = 0x4,     /* Functionality controlled by role variable */
  CF_DEL_DEL_CFT = 0x8,      /* Delete finding no row is a conflict */
  CF_USE_INTERP_WRITE = 0x10 /* Use interpreted writeTuple() when configured */
};

struct st_conflict_fn_def {
  const char *name;
  enum_conflict_fn_type type;
  const st_conflict_fn_arg_def *arg_defs;
  prepare_detect_func prep_func;
  uint8 flags; /* enum_conflict_fn_flags */
};

/* What sort of conflict was found */
enum enum_conflict_cause {
  ROW_ALREADY_EXISTS = 1, /* On insert */
  ROW_DOES_NOT_EXIST = 2, /* On Update, Delete */
  ROW_IN_CONFLICT = 3,    /* On Update, Delete */
  TRANS_IN_CONFLICT = 4   /* Any of above, or implied by transaction */
};

/* NdbOperation custom data which points out handler and record. */
struct Ndb_exceptions_data {
  struct NDB_SHARE *share;
  const NdbRecord *key_rec;
  const NdbRecord *data_rec;
  const uchar *old_row;
  const uchar *new_row;
  my_bitmap_map *bitmap_buf; /* Buffer for write_set */
  MY_BITMAP *write_set;
  enum_conflicting_op_type op_type;
  bool reflected_operation;
  Uint64 trans_id;
};

enum enum_conflict_fn_table_flags { CFF_NONE = 0, CFF_REFRESH_ROWS = 1 };

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
class ExceptionsTableWriter {
  enum COLUMN_VERSION { DEFAULT = 0, OLD = 1, NEW = 2 };

 public:
  ExceptionsTableWriter()
      : m_pk_cols(0),
        m_cols(0),
        m_xcols(0),
        m_ex_tab(nullptr),
        m_count(0),
        m_extended(false),
        m_op_type_pos(0),
        m_conflict_cause_pos(0),
        m_orig_transid_pos(0) {}

  ~ExceptionsTableWriter() {}

  /**
     hasTable

     Returns true if there is an Exceptions table
  */
  bool hasTable() const { return m_ex_tab != nullptr; }

  /**
    init

    Initialise ExceptionsTableWriter with main and exceptions
    tables.

    May set a warning message on success or error.
  */
  int init(const NdbDictionary::Table *mainTable,
           const NdbDictionary::Table *exceptionsTable, char *msg_buf,
           uint msg_buf_len, const char **msg);

  /**
     free

     Release reference to exceptions table
  */
  void mem_free(Ndb *ndb);

  /**
     writeRow

     Write a row to the Exceptions Table for the given
     key
  */
  int writeRow(NdbTransaction *trans, const NdbRecord *keyRecord,
               const NdbRecord *dataRecord, uint32 server_id,
               uint32 master_server_id, uint64 master_epoch,
               const uchar *oldRowPtr, const uchar *newRowPtr,
               enum_conflicting_op_type op_type,
               enum_conflict_cause conflict_cause, uint64 orig_transid,
               const MY_BITMAP *write_set, NdbError &err);

 private:
  /* Help methods for checking exception table definition */
  bool check_mandatory_columns(const NdbDictionary::Table *exceptionsTable);
  bool check_pk_columns(const NdbDictionary::Table *mainTable,
                        const NdbDictionary::Table *exceptionsTable, int &k);
  bool check_optional_columns(const NdbDictionary::Table *mainTable,
                              const NdbDictionary::Table *exceptionsTable,
                              char *msg_buf, uint msg_buf_len, const char **msg,
                              int &k, char *error_details,
                              uint error_details_len);

  /* info about original table */
  uint8 m_pk_cols;
  uint16 m_cols;
  /* Specifies if a column in the original table is nullable */
  bool m_col_nullable[NDB_MAX_ATTRIBUTES_IN_TABLE];

  /* info about exceptions table */
  uint16 m_xcols;
  const NdbDictionary::Table *m_ex_tab;
  uint32 m_count;
  /*
    Extension tables can be extended with optional fields
    NDB@OPT_TYPE
   */
  bool m_extended;
  uint32 m_op_type_pos;
  uint32 m_conflict_cause_pos;
  uint32 m_orig_transid_pos;

  /*
    Mapping of where the referenced primary key fields are
    in the original table. Doesn't have to include all fields.
  */
  uint16 m_key_attrids[NDB_MAX_KEY_PARTS];
  /* Mapping of pk columns in original table to conflict table */
  int m_key_data_pos[NDB_MAX_KEY_PARTS];
  /* Mapping of non-pk columns in original table to conflict table */
  int m_data_pos[NDB_MAX_ATTRIBUTES_IN_TABLE];
  /* Specifies what version of a column is reference (before- or after-image) */
  COLUMN_VERSION m_column_version[NDB_MAX_ATTRIBUTES_IN_TABLE];

  /*
    has_prefix_ci

    Return true if a column has a specific prefix.
  */
  bool has_prefix_ci(const char *col_name, const char *prefix,
                     CHARSET_INFO *cs);

  /*
    has_suffix_ci

    Return true if a column has a specific suffix
    and sets the column_real_name to the column name
    without the suffix.
  */
  bool has_suffix_ci(const char *col_name, const char *suffix, CHARSET_INFO *cs,
                     char *col_name_real);

  /*
    find_column_name_ci

    Search for column_name in table and
    return true if found. Also return what
    position column was found in pos and possible
    position in the primary key in key_pos.
   */
  bool find_column_name_ci(CHARSET_INFO *cs, const char *col_name,
                           const NdbDictionary::Table *table, int *pos,
                           int *no_key_cols);
};

struct NDB_CONFLICT_FN_SHARE {
  const st_conflict_fn_def *m_conflict_fn;

  /* info about original table */
  uint16 m_resolve_column;
  uint8 m_resolve_size;
  uint8 m_flags;

  ExceptionsTableWriter m_ex_tab_writer;
};

/**
 * enum_slave_conflict_role
 *
 * These are the roles the Slave can play
 * in asymmetric conflict algorithms
 */

enum enum_slave_conflict_role {
  SCR_NONE = 0,
  SCR_SECONDARY = 1,
  SCR_PRIMARY = 2,
  SCR_PASS = 3
};

enum enum_slave_trans_conflict_apply_state {
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

enum enum_slave_conflict_flags {
  /* Conflict detection Ops defined */
  SCS_OPS_DEFINED = 1,
  /* Conflict detected on table with transactional resolution */
  SCS_TRANS_CONFLICT_DETECTED_THIS_PASS = 2
};

/*
  State associated with the Slave thread
  (From the Ndb handler's point of view)
*/
struct st_ndb_slave_state {
  /* Counter values for current slave transaction */
  Uint32 current_violation_count[CFT_NUMBER_OF_CFTS];

  /**
   * Number of delete-delete conflicts detected
   * (delete op is applied, and row does not exist)
   */
  Uint32 current_delete_delete_count;

  /**
   * Number of reflected operations received that have been
   * prepared (defined) to be executed.
   */
  Uint32 current_reflect_op_prepare_count;

  /**
   * Number of reflected operations that were not applied as
   * they hit some error during execution
   */
  Uint32 current_reflect_op_discard_count;

  /**
   * Number of refresh operations that have been prepared
   */
  Uint32 current_refresh_op_count;

  // Tracks server_id's from any source, both immediate and downstream
  std::unordered_set<Uint32> source_server_ids;

  /* Track the current epoch from the immediate master,
   * and whether we've committed it
   */
  Uint64 current_master_server_epoch;
  bool current_master_server_epoch_committed;

  Uint64 current_max_rep_epoch;
  uint8 conflict_flags; /* enum_slave_conflict_flags */
  /* Transactional conflict detection */
  Uint32 retry_trans_count;
  Uint32 current_trans_row_conflict_count;
  Uint32 current_trans_row_reject_count;
  Uint32 current_trans_in_conflict_count;

  /* Last conflict epoch */
  Uint64 last_conflicted_epoch;

  /* Last stable epoch */
  Uint64 last_stable_epoch;

  /* Cumulative counter values */
  Uint64 total_violation_count[CFT_NUMBER_OF_CFTS];
  Uint64 total_delete_delete_count;
  Uint64 total_reflect_op_prepare_count;
  Uint64 total_reflect_op_discard_count;
  Uint64 total_refresh_op_count;
  Uint64 max_rep_epoch;
  /* Mark if slave has been started/restarted */
  bool applier_sql_thread_start;
  /* Transactional conflict detection */
  Uint64 trans_row_conflict_count;
  Uint64 trans_row_reject_count;
  Uint64 trans_detect_iter_count;
  Uint64 trans_in_conflict_count;
  Uint64 trans_conflict_commit_count;

  static constexpr Uint32 MAX_RETRY_TRANS_COUNT = 100;

  /*
    Slave Apply State

    State of Binlog application from Ndb point of view.
  */
  enum_slave_trans_conflict_apply_state trans_conflict_apply_state;

  MEM_ROOT conflict_mem_root;
  class DependencyTracker *trans_dependency_tracker;

  /* Methods */
  void atStartSlave();
  int atPrepareConflictDetection(const NdbDictionary::Table *table,
                                 const NdbRecord *key_rec,
                                 const uchar *row_data, Uint64 transaction_id,
                                 bool &handle_conflict_now);
  int atTransConflictDetected(Uint64 transaction_id);
  int atConflictPreCommit(bool &retry_slave_trans);

  void atBeginTransConflictHandling();
  void atEndTransConflictHandling();

  void atTransactionCommit(Uint64 epoch);
  void atTransactionAbort();
  void atResetSlave();

  int atApplyStatusWrite(Uint32 master_server_id, Uint32 row_server_id,
                         Uint64 row_epoch, bool is_row_server_id_local);
  bool verifyNextEpoch(Uint64 next_epoch, Uint32 master_server_id) const;

  void resetPerAttemptCounters();

  void saveServerId(Uint32);
  bool seenServerId(Uint32) const;

  static bool checkSlaveConflictRoleChange(enum_slave_conflict_role old_role,
                                           enum_slave_conflict_role new_role,
                                           const char **failure_cause);

  st_ndb_slave_state();
  ~st_ndb_slave_state();
};

constexpr int ERROR_CONFLICT_FN_VIOLATION = 9999;

/**
 * Conflict function setup infrastructure
 */
int parse_conflict_fn_spec(const char *conflict_fn_spec,
                           const st_conflict_fn_def **conflict_fn,
                           st_conflict_fn_arg *args, Uint32 *max_args,
                           char *msg, uint msg_len);
int setup_conflict_fn(Ndb *ndb, NDB_CONFLICT_FN_SHARE **ppcfn_share,
                      const char *dbName, const char *tabName,
                      bool tableBinlogUseUpdate,
                      const NdbDictionary::Table *ndbtab, char *msg,
                      uint msg_len, const st_conflict_fn_def *conflict_fn,
                      const st_conflict_fn_arg *args, const Uint32 num_args);

void teardown_conflict_fn(Ndb *ndb, NDB_CONFLICT_FN_SHARE *cfn_share);

void slave_reset_conflict_fn(NDB_CONFLICT_FN_SHARE *cfn_share);

bool is_exceptions_table(const char *table_name);

/**
  show_ndb_status_conflict

  Called as part of SHOW STATUS or performance_schema
  queries. Returns info about ndb_conflict related status variables.
*/

int show_ndb_status_conflict(THD *thd, SHOW_VAR *var, char *buff);

#endif
