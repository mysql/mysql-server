/*
   Copyright (c) 2012, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <sys/types.h>

#include "my_bitmap.h"
#include "my_inttypes.h"
#include "mysql_com.h"      // NAME_CHAR_LEN
#include "sql/sql_const.h"  // MAX_REF_PARTS
#include "storage/ndb/include/ndb_types.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/ndbapi_limits.h"

class Ndb;
class NdbTransaction;
class NdbRecord;
struct CHARSET_INFO;
struct NdbError;

struct CHARSET_INFO;

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
                                   class NdbInterpretedCode *code,
                                   Uint64 max_rep_epoch);

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
  ROW_DOES_NOT_EXIST = 1, /* On Update, Delete */
  ROW_ALREADY_EXISTS = 2, /* On insert */
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

#endif
