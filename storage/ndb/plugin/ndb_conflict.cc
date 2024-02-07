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

#include "storage/ndb/plugin/ndb_conflict.h"

#include <inttypes.h>

#include "my_dbug.h"
#include "mysql/strings/m_ctype.h"
#include "nulls.h"
#include "sql/mysqld.h"  // lower_case_table_names
#include "sql/mysqld_cs.h"
#include "storage/ndb/include/ndbapi/Ndb.hpp"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/NdbError.hpp"
#include "storage/ndb/include/ndbapi/NdbInterpretedCode.hpp"
#include "storage/ndb/include/ndbapi/NdbOperation.hpp"
#include "storage/ndb/include/ndbapi/NdbTransaction.hpp"
#include "storage/ndb/plugin/ndb_binlog_extra_row_info.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "strxnmov.h"

extern ulong opt_ndb_slave_conflict_role;

typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Column NDBCOL;

#define NDB_EXCEPTIONS_TABLE_SUFFIX "$EX"
#define NDB_EXCEPTIONS_TABLE_SUFFIX_LOWER "$ex"

#define NDB_EXCEPTIONS_TABLE_COLUMN_PREFIX "NDB$"
#define NDB_EXCEPTIONS_TABLE_OP_TYPE "NDB$OP_TYPE"
#define NDB_EXCEPTIONS_TABLE_CONFLICT_CAUSE "NDB$CFT_CAUSE"
#define NDB_EXCEPTIONS_TABLE_ORIG_TRANSID "NDB$ORIG_TRANSID"
#define NDB_EXCEPTIONS_TABLE_COLUMN_OLD_SUFFIX "$OLD"
#define NDB_EXCEPTIONS_TABLE_COLUMN_NEW_SUFFIX "$NEW"

/*
  Return true if a column has a specific prefix.
*/
bool ExceptionsTableWriter::has_prefix_ci(const char *col_name,
                                          const char *prefix,
                                          CHARSET_INFO *cs) {
  uint col_len = strlen(col_name);
  uint prefix_len = strlen(prefix);
  if (col_len < prefix_len) return false;
  char col_name_prefix[FN_HEADLEN];
  strncpy(col_name_prefix, col_name, prefix_len);
  col_name_prefix[prefix_len] = '\0';
  return (my_strcasecmp(cs, col_name_prefix, prefix) == 0);
}

/*
  Return true if a column has a specific suffix
  and sets the column_real_name to the column name
  without the suffix.
*/
bool ExceptionsTableWriter::has_suffix_ci(const char *col_name,
                                          const char *suffix, CHARSET_INFO *cs,
                                          char *col_name_real) {
  uint col_len = strlen(col_name);
  uint suffix_len = strlen(suffix);
  const char *col_name_endp = col_name + col_len;
  strcpy(col_name_real, col_name);
  if (col_len > suffix_len &&
      my_strcasecmp(cs, col_name_endp - suffix_len, suffix) == 0) {
    col_name_real[col_len - suffix_len] = '\0';
    return true;
  }
  return false;
}

/*
  Search for column_name in table and
  return true if found. Also return what
  position column was found in pos and possible
  position in the primary key in key_pos.
 */
bool ExceptionsTableWriter::find_column_name_ci(
    CHARSET_INFO *cs, const char *col_name, const NdbDictionary::Table *table,
    int *pos, int *key_pos) {
  int ncol = table->getNoOfColumns();
  for (int m = 0; m < ncol; m++) {
    const NdbDictionary::Column *col = table->getColumn(m);
    const char *tcol_name = col->getName();
    if (col->getPrimaryKey()) (*key_pos)++;
    if (my_strcasecmp(cs, col_name, tcol_name) == 0) {
      *pos = m;
      return true;
    }
  }
  return false;
}

bool ExceptionsTableWriter::check_mandatory_columns(
    const NdbDictionary::Table *exceptionsTable) {
  DBUG_TRACE;
  if (/* server id */
      exceptionsTable->getColumn(0)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(0)->getPrimaryKey() &&
      /* master_server_id */
      exceptionsTable->getColumn(1)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(1)->getPrimaryKey() &&
      /* master_epoch */
      exceptionsTable->getColumn(2)->getType() == NDBCOL::Bigunsigned &&
      exceptionsTable->getColumn(2)->getPrimaryKey() &&
      /* count */
      exceptionsTable->getColumn(3)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(3)->getPrimaryKey())
    return true;
  else
    return false;
}

bool ExceptionsTableWriter::check_pk_columns(
    const NdbDictionary::Table *mainTable,
    const NdbDictionary::Table *exceptionsTable, int &k) {
  DBUG_TRACE;
  const int fixed_cols = 4;
  int ncol = mainTable->getNoOfColumns();
  int nkey = mainTable->getNoOfPrimaryKeys();
  /* Check columns that are part of the primary key */
  for (int i = k = 0; i < ncol && k < nkey; i++) {
    const NdbDictionary::Column *col = mainTable->getColumn(i);
    if (col->getPrimaryKey()) {
      const NdbDictionary::Column *ex_col =
          exceptionsTable->getColumn(fixed_cols + k);
      if (!(ex_col != nullptr && col->getType() == ex_col->getType() &&
            col->getLength() == ex_col->getLength() &&
            col->getNullable() == ex_col->getNullable())) {
        /*
           Primary key type of the original table doesn't match
           the primary key column of the exception table.
           Assume that the table format has been extended and
           check more below.
        */
        DBUG_PRINT(
            "info",
            ("Primary key column columns don't match, assume extended table"));
        m_extended = true;
        break;
      }
      /*
        Store mapping of Exception table key# to
        orig table attrid
      */
      DBUG_PRINT("info", ("%u: Setting m_key_attrids[%i]= %i", __LINE__, k, i));
      m_key_attrids[k] = i;
      k++;
    }
  }
  return true;
}

bool ExceptionsTableWriter::check_optional_columns(
    const NdbDictionary::Table *mainTable,
    const NdbDictionary::Table *exceptionsTable, char *msg_buf,
    uint msg_buf_len, const char **msg, int &k, char *error_details,
    uint error_details_len) {
  DBUG_TRACE;
  /*
    Check optional columns.
    Check if table has been extended by looking for
    the NDB$ prefix. By looking at the columns in
    reverse order we can determine if table has been
    extended and then double check that the original
    mandatory columns also have the NDB$ prefix.
    If an incomplete primary key has been found or
    additional non-primary key attributes from the
    original table then table is also assumed to be
    extended.
  */
  const char *ex_tab_name = exceptionsTable->getName();
  const int fixed_cols = 4;
  bool ok = true;
  int xncol = exceptionsTable->getNoOfColumns();
  int i;
  for (i = xncol - 1; i >= 0; i--) {
    const NdbDictionary::Column *col = exceptionsTable->getColumn(i);
    const char *col_name = col->getName();
    /*
      We really need the CHARSET_INFO from when the table was
      created but NdbDictionary::Table doesn't save this. This
      means we cannot handle tables and exception tables defined
      with a charset different than the system charset.
    */
    CHARSET_INFO *cs = system_charset_info;
    bool has_prefix = false;

    if (has_prefix_ci(col_name, NDB_EXCEPTIONS_TABLE_COLUMN_PREFIX, cs)) {
      has_prefix = true;
      m_extended = true;
      DBUG_PRINT("info", ("Exceptions table %s is extended with column %s",
                          ex_tab_name, col_name));
    }
    /* Check that mandatory columns have NDB$ prefix */
    if (i < 4) {
      if (m_extended && !has_prefix) {
        snprintf(msg_buf, msg_buf_len,
                 "Exceptions table %s is extended, but mandatory column %s  "
                 "doesn't have the \'%s\' prefix",
                 ex_tab_name, col_name, NDB_EXCEPTIONS_TABLE_COLUMN_PREFIX);
        *msg = msg_buf;
        return false;
      }
    }
    k = i - fixed_cols;
    /* Check for extended columns */
    if (my_strcasecmp(cs, col_name, NDB_EXCEPTIONS_TABLE_OP_TYPE) == 0) {
      /* Check if ENUM or INT UNSIGNED */
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Char &&
          exceptionsTable->getColumn(i)->getType() != NDBCOL::Unsigned) {
        snprintf(error_details, error_details_len,
                 "Table %s has incorrect type %u for NDB$OP_TYPE",
                 exceptionsTable->getName(),
                 exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok = false;
        break;
      }
      m_extended = true;
      m_op_type_pos = i;
      continue;
    }
    if (my_strcasecmp(cs, col_name, NDB_EXCEPTIONS_TABLE_CONFLICT_CAUSE) == 0) {
      /* Check if ENUM or INT UNSIGNED */
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Char &&
          exceptionsTable->getColumn(i)->getType() != NDBCOL::Unsigned) {
        snprintf(error_details, error_details_len,
                 "Table %s has incorrect type %u for NDB$CFT_CAUSE",
                 exceptionsTable->getName(),
                 exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok = false;
        break;
      }
      m_extended = true;
      m_conflict_cause_pos = i;
      continue;
    }
    if (my_strcasecmp(cs, col_name, NDB_EXCEPTIONS_TABLE_ORIG_TRANSID) == 0) {
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Bigunsigned) {
        snprintf(error_details, error_details_len,
                 "Table %s has incorrect type %u for NDB$ORIG_TRANSID",
                 exceptionsTable->getName(),
                 exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok = false;
        break;
      }
      m_extended = true;
      m_orig_transid_pos = i;
      continue;
    }
    /*
      Check for any optional columns from the original table in the extended
      table. Compare column types of columns with names matching a column in
      the original table. If a non-primary key column is found we assume that
      the table is extended.
    */
    if (i >= fixed_cols) {
      int match = -1;
      int match_k = -1;
      COLUMN_VERSION column_version = DEFAULT;
      char col_name_real[FN_HEADLEN];
      /* Check for old or new column reference */
      if (has_suffix_ci(col_name, NDB_EXCEPTIONS_TABLE_COLUMN_OLD_SUFFIX, cs,
                        col_name_real)) {
        DBUG_PRINT("info", ("Found reference to old column %s", col_name));
        column_version = OLD;
      } else if (has_suffix_ci(col_name, NDB_EXCEPTIONS_TABLE_COLUMN_NEW_SUFFIX,
                               cs, col_name_real)) {
        DBUG_PRINT("info", ("Found reference to new column %s", col_name));
        column_version = NEW;
      }
      DBUG_PRINT("info", ("Checking for original column %s", col_name_real));
      /*
        We really need the CHARSET_INFO from when the table was
        created but NdbDictionary::Table doesn't save this. This
        means we cannot handle tables end exception tables defined
        with a charset different than the system charset.
      */
      CHARSET_INFO *mcs = system_charset_info;
      if (!find_column_name_ci(mcs, col_name_real, mainTable, &match,
                               &match_k)) {
        if (!strcmp(col_name, col_name_real)) {
          /*
            Column did have $OLD or $NEW suffix, but it didn't
            match. Check if that is the real name of the column.
          */
          match_k = -1;
          if (find_column_name_ci(mcs, col_name, mainTable, &match, &match_k)) {
            DBUG_PRINT("info",
                       ("Column %s in main table %s has an unfortunate name",
                        col_name, mainTable->getName()));
          }
        }
      }
      /*
        Check that old or new references are nullable
        or have a default value.
      */
      if (column_version != DEFAULT && match_k != -1) {
        if ((!col->getNullable()) && col->getDefaultValue() == nullptr) {
          snprintf(error_details, error_details_len,
                   "Old or new column reference %s in table %s is not nullable "
                   "and doesn't have a default value",
                   col->getName(), exceptionsTable->getName());
          DBUG_PRINT("info", ("%s", error_details));
          ok = false;
          break;
        }
      }

      if (match == -1) {
        /*
           Column do not have the same name, could be allowed
           if column is nullable or has a default value,
           continue checking, but give a warning to user
        */
        if ((!col->getNullable()) && col->getDefaultValue() == nullptr) {
          snprintf(error_details, error_details_len,
                   "Extra column %s in table %s is not nullable and doesn't "
                   "have a default value",
                   col->getName(), exceptionsTable->getName());
          DBUG_PRINT("info", ("%s", error_details));
          ok = false;
          break;
        }
        snprintf(error_details, error_details_len,
                 "Column %s in extension table %s not found in %s",
                 col->getName(), exceptionsTable->getName(),
                 mainTable->getName());
        DBUG_PRINT("info", ("%s", error_details));
        snprintf(msg_buf, msg_buf_len,
                 "exceptions table %s has suspicious "
                 "definition ((column %d): %s",
                 ex_tab_name, fixed_cols + k, error_details);
        continue;
      }
      /* We have a matching name */
      const NdbDictionary::Column *mcol = mainTable->getColumn(match);
      if (col->getType() == mcol->getType()) {
        DBUG_PRINT("info", ("Comparing column %s in exceptions table with "
                            "column %s in main table",
                            col->getName(), mcol->getName()));
        /* We have matching type */
        if (!mcol->getPrimaryKey()) {
          /*
            Matching non-key column found.
            Check that column is nullable
            or has a default value.
          */
          if (col->getNullable() || col->getDefaultValue() != nullptr) {
            DBUG_PRINT("info", ("Mapping column %s %s(%i) to %s(%i)",
                                col->getName(), mainTable->getName(), match,
                                exceptionsTable->getName(), i));
            /* Save position */
            m_data_pos[i] = match;
            m_column_version[i] = column_version;
          } else {
            snprintf(error_details, error_details_len,
                     "Data column %s in table %s is not nullable and doesn't "
                     "have a default value",
                     col->getName(), exceptionsTable->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok = false;
            break;
          }
        } else {
          /* Column is part of the primary key */
          if (column_version != DEFAULT) {
            snprintf(
                error_details, error_details_len,
                "Old or new values of primary key columns cannot be referenced "
                "since primary keys cannot be updated, column %s in table %s",
                col->getName(), exceptionsTable->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok = false;
            break;
          }
          if (col->getNullable() == mcol->getNullable()) {
            /*
              Columns are both nullable or not nullable.
              Save position.
            */
            if (m_key_data_pos[match_k] != -1) {
              snprintf(
                  error_details, error_details_len,
                  "Multiple references to the same key column %s in table %s",
                  col->getName(), exceptionsTable->getName());
              DBUG_PRINT("info", ("%s", error_details));
              ok = false;
              break;
            }
            DBUG_PRINT("info", ("Setting m_key_data_pos[%i]= %i", match_k, i));
            m_key_data_pos[match_k] = i;

            if (i == fixed_cols + match_k) {
              /* Found key column in correct position */
              if (!m_extended) continue;
            }
            /*
              Store mapping of Exception table key# to
              orig table attrid
            */
            DBUG_PRINT("info", ("%u: Setting m_key_attrids[%i]= %i", __LINE__,
                                match_k, match));
            m_key_attrids[match_k] = match;
            m_extended = true;
          } else if (column_version == DEFAULT) {
            /*
               Columns have same name and same type
               Column with this name is part of primary key,
               but both columns are not declared not null
            */
            snprintf(error_details, error_details_len,
                     "Pk column %s not declared not null in both tables",
                     col->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok = false;
            break;
          }
        }
      } else {
        /*
           Columns have same name, but not the same type
        */
        snprintf(error_details, error_details_len,
                 "Column %s has matching name to column %s for table %s, but "
                 "wrong type, %u versus %u",
                 col->getName(), mcol->getName(), mainTable->getName(),
                 col->getType(), mcol->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok = false;
        break;
      }
    }
  }

  return ok;
}

int ExceptionsTableWriter::init(const NdbDictionary::Table *mainTable,
                                const NdbDictionary::Table *exceptionsTable,
                                char *msg_buf, uint msg_buf_len,
                                const char **msg) {
  DBUG_TRACE;
  const char *ex_tab_name = exceptionsTable->getName();
  const int fixed_cols = 4;
  *msg = nullptr;
  *msg_buf = '\0';

  DBUG_PRINT("info",
             ("Checking definition of exceptions table %s", ex_tab_name));
  /*
    Check that the table have the correct number of columns
    and the mandatory columns.
   */

  bool ok = exceptionsTable->getNoOfColumns() >= fixed_cols &&
            exceptionsTable->getNoOfPrimaryKeys() == 4 &&
            check_mandatory_columns(exceptionsTable);

  if (ok) {
    char error_details[FN_REFLEN];
    uint error_details_len = sizeof(error_details);
    error_details[0] = '\0';
    int ncol = mainTable->getNoOfColumns();
    int nkey = mainTable->getNoOfPrimaryKeys();
    int xncol = exceptionsTable->getNoOfColumns();
    int i, k;
    /* Initialize position arrays */
    for (k = 0; k < nkey; k++) m_key_data_pos[k] = -1;
    for (i = 0; i < xncol; i++) m_data_pos[i] = -1;
    /* Initialize nullability information */
    for (i = 0; i < ncol; i++) {
      const NdbDictionary::Column *col = mainTable->getColumn(i);
      m_col_nullable[i] = col->getNullable();
    }

    /*
      Check that the primary key columns in the main table
      are referenced correctly.
      Then check if the table is extended with optional
      columns.
     */
    ok =
        check_pk_columns(mainTable, exceptionsTable, k) &&
        check_optional_columns(mainTable, exceptionsTable, msg_buf, msg_buf_len,
                               msg, k, error_details, error_details_len);
    if (ok) {
      m_ex_tab = exceptionsTable;
      m_pk_cols = nkey;
      m_cols = ncol;
      m_xcols = xncol;
      if (m_extended && strlen(msg_buf) > 0) *msg = msg_buf;
      return 0;
    } else
      snprintf(msg_buf, msg_buf_len,
               "exceptions table %s has wrong "
               "definition (column %d): %s",
               ex_tab_name, fixed_cols + k, error_details);
  } else
    snprintf(msg_buf, msg_buf_len,
             "exceptions table %s has wrong "
             "definition (initial %d columns)",
             ex_tab_name, fixed_cols);

  *msg = msg_buf;
  return -1;
}

void ExceptionsTableWriter::mem_free(Ndb *ndb) {
  if (m_ex_tab) {
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    dict->removeTableGlobal(*m_ex_tab, 0);
    m_ex_tab = nullptr;
  }
}

int ExceptionsTableWriter::writeRow(
    NdbTransaction *trans, const NdbRecord *keyRecord,
    const NdbRecord *dataRecord, uint32 server_id, uint32 master_server_id,
    uint64 master_epoch, const uchar *oldRowPtr, const uchar *newRowPtr,
    enum_conflicting_op_type op_type, enum_conflict_cause conflict_cause,
    uint64 orig_transid, const MY_BITMAP *write_set, NdbError &err) {
  DBUG_TRACE;
  DBUG_PRINT(
      "info",
      ("op_type(pos):%u(%u), conflict_cause(pos):%u(%u), orig_transid:%" PRIu64
       "(%u)",
       op_type, m_op_type_pos, conflict_cause, m_conflict_cause_pos,
       orig_transid, m_orig_transid_pos));
  assert(write_set != nullptr);
  assert(err.code == 0);
  const uchar *rowPtr = (op_type == DELETE_ROW) ? oldRowPtr : newRowPtr;

  do {
    /* Have exceptions table, add row to it */
    const NDBTAB *ex_tab = m_ex_tab;

    /* get insert op */
    NdbOperation *ex_op = trans->getNdbOperation(ex_tab);
    if (ex_op == nullptr) {
      err = trans->getNdbError();
      break;
    }
    if (ex_op->insertTuple() == -1) {
      err = ex_op->getNdbError();
      break;
    }
    {
      uint32 count = (uint32)++m_count;
      /* Set mandatory columns */
      if (ex_op->setValue((Uint32)0, (const char *)&(server_id)) ||
          ex_op->setValue((Uint32)1, (const char *)&(master_server_id)) ||
          ex_op->setValue((Uint32)2, (const char *)&(master_epoch)) ||
          ex_op->setValue((Uint32)3, (const char *)&(count))) {
        err = ex_op->getNdbError();
        break;
      }
      /* Set optional columns */
      if (m_extended) {
        if (m_op_type_pos) {
          if (m_ex_tab->getColumn(m_op_type_pos)->getType() == NDBCOL::Char) {
            /* Defined as ENUM */
            char op_type_val = (char)op_type;
            if (ex_op->setValue((Uint32)m_op_type_pos,
                                (const char *)&(op_type_val))) {
              err = ex_op->getNdbError();
              break;
            }
          } else {
            uint32 op_type_val = op_type;
            if (ex_op->setValue((Uint32)m_op_type_pos,
                                (const char *)&(op_type_val))) {
              err = ex_op->getNdbError();
              break;
            }
          }
        }
        if (m_conflict_cause_pos) {
          if (m_ex_tab->getColumn(m_conflict_cause_pos)->getType() ==
              NDBCOL::Char) {
            /* Defined as ENUM */
            char conflict_cause_val = (char)conflict_cause;
            if (ex_op->setValue((Uint32)m_conflict_cause_pos,
                                (const char *)&(conflict_cause_val))) {
              err = ex_op->getNdbError();
              break;
            }
          } else {
            uint32 conflict_cause_val = conflict_cause;
            if (ex_op->setValue((Uint32)m_conflict_cause_pos,
                                (const char *)&(conflict_cause_val))) {
              err = ex_op->getNdbError();
              break;
            }
          }
        }
        if (m_orig_transid_pos != 0) {
          const NdbDictionary::Column *col =
              m_ex_tab->getColumn(m_orig_transid_pos);
          if (orig_transid == Ndb_binlog_extra_row_info::InvalidTransactionId &&
              col->getNullable()) {
            if (ex_op->setValue((Uint32)m_orig_transid_pos, (char *)nullptr)) {
              err = ex_op->getNdbError();
              break;
            }
          } else {
            DBUG_PRINT("info", ("Setting orig_transid (%u) for table %s",
                                m_orig_transid_pos, ex_tab->getName()));
            uint64 orig_transid_val = orig_transid;
            if (ex_op->setValue((Uint32)m_orig_transid_pos,
                                (const char *)&(orig_transid_val))) {
              err = ex_op->getNdbError();
              break;
            }
          }
        }
      }
    }
    /* copy primary keys */
    {
      int nkey = m_pk_cols;
      int k;
      for (k = 0; k < nkey; k++) {
        assert(rowPtr != nullptr);
        if (m_key_data_pos[k] != -1) {
          const uchar *data = (const uchar *)NdbDictionary::getValuePtr(
              keyRecord, (const char *)rowPtr, m_key_attrids[k]);
          if (ex_op->setValue((Uint32)m_key_data_pos[k], (const char *)data) ==
              -1) {
            err = ex_op->getNdbError();
            break;
          }
        }
      }
    }
    /* Copy additional data */
    if (m_extended) {
      int xncol = m_xcols;
      int i;
      for (i = 0; i < xncol; i++) {
        const NdbDictionary::Column *col = m_ex_tab->getColumn(i);
        const uchar *default_value = (const uchar *)col->getDefaultValue();
        DBUG_PRINT("info", ("Checking column %s(%i)%s", col->getName(), i,
                            (default_value) ? ", has default value" : ""));
        assert(rowPtr != nullptr);
        if (m_data_pos[i] != -1) {
          const uchar *row_vPtr = nullptr;
          switch (m_column_version[i]) {
            case DEFAULT:
              row_vPtr = rowPtr;
              break;
            case OLD:
              if (op_type != WRITE_ROW) row_vPtr = oldRowPtr;
              break;
            case NEW:
              if (op_type != DELETE_ROW) row_vPtr = newRowPtr;
          }
          if (row_vPtr == nullptr ||
              (m_col_nullable[m_data_pos[i]] &&
               NdbDictionary::isNull(dataRecord, (const char *)row_vPtr,
                                     m_data_pos[i]))) {
            DBUG_PRINT("info", ("Column %s is set to NULL because it is NULL",
                                col->getName()));
            if (ex_op->setValue((Uint32)i, (char *)nullptr)) {
              err = ex_op->getNdbError();
              break;
            }
          } else if (write_set != nullptr &&
                     bitmap_is_set(write_set, m_data_pos[i])) {
            DBUG_PRINT("info", ("Column %s is set", col->getName()));
            const uchar *data = (const uchar *)NdbDictionary::getValuePtr(
                dataRecord, (const char *)row_vPtr, m_data_pos[i]);
            if (ex_op->setValue((Uint32)i, (const char *)data) == -1) {
              err = ex_op->getNdbError();
              break;
            }
          } else if (default_value != nullptr) {
            DBUG_PRINT(
                "info",
                ("Column %s is not set to NULL because it has a default value",
                 col->getName()));
            /*
             * Column has a default value
             * Since no value was set in write_set
             * we let the default value be set from
             * Ndb instead.
             */
          } else {
            DBUG_PRINT("info",
                       ("Column %s is set to NULL because it not in write_set",
                        col->getName()));
            if (ex_op->setValue((Uint32)i, (char *)nullptr)) {
              err = ex_op->getNdbError();
              break;
            }
          }
        }
      }
    }
  } while (0);

  if (err.code != 0) {
    if (err.classification == NdbError::SchemaError) {
      /*
       * Something up with Exceptions table schema, forget it.
       * No further exceptions will be recorded.
       * Caller will log this and slave will stop.
       */
      NdbDictionary::Dictionary *dict = trans->getNdb()->getDictionary();
      dict->removeTableGlobal(*m_ex_tab, false);
      m_ex_tab = nullptr;
      return 0;
    }
    return -1;
  }
  return 0;
}

/**
 * Conflict function interpreted programs
 */

/**
  CFT_NDB_OLD

  To perform conflict detection, an interpreted program is used to read
  the timestamp stored locally and compare to what was on the master.
  If timestamp is not equal, an error for this operation (9998) will be raised,
  and new row will not be applied. The error codes for the operations will
  be checked on return.  For this to work is is vital that the operation
  is run with ignore error option.

  As an independent feature, phase 2 also saves the
  conflicts into the table's exceptions table.
*/
static int row_conflict_fn_old(NDB_CONFLICT_FN_SHARE *cfn_share,
                               enum_conflicting_op_type,
                               const NdbRecord *data_record,
                               const uchar *old_data, const uchar *,
                               const MY_BITMAP *bi_cols, const MY_BITMAP *,
                               NdbInterpretedCode *code, Uint64) {
  DBUG_TRACE;
  uint32 resolve_column = cfn_share->m_resolve_column;
  uint32 resolve_size = cfn_share->m_resolve_size;
  const uchar *field_ptr = (const uchar *)NdbDictionary::getValuePtr(
      data_record, (const char *)old_data, cfn_share->m_resolve_column);

  assert((resolve_size == 4) || (resolve_size == 8));

  if (unlikely(!bitmap_is_set(bi_cols, resolve_column))) {
    ndb_log_info("Replica: missing data for %s timestamp column %u.",
                 cfn_share->m_conflict_fn->name, resolve_column);
    return 1;
  }

  const uint label_0 = 0;
  const Uint32 RegOldValue = 1, RegCurrentValue = 2;
  int r;

  DBUG_PRINT(
      "info",
      ("Adding interpreted filter, existing value must eq event old value"));
  /*
   * read old value from record
   */
  union {
    uint32 old_value_32;
    uint64 old_value_64;
  };
  {
    if (resolve_size == 4) {
      memcpy(&old_value_32, field_ptr, resolve_size);
      DBUG_PRINT("info", ("  old_value_32: %u", old_value_32));
    } else {
      memcpy(&old_value_64, field_ptr, resolve_size);
      DBUG_PRINT("info",
                 ("  old_value_64: %llu", (unsigned long long)old_value_64));
    }
  }

  /*
   * Load registers RegOldValue and RegCurrentValue
   */
  if (resolve_size == 4)
    r = code->load_const_u32(RegOldValue, old_value_32);
  else
    r = code->load_const_u64(RegOldValue, old_value_64);
  assert(r == 0);
  r = code->read_attr(RegCurrentValue, resolve_column);
  assert(r == 0);
  /*
   * if RegOldValue == RegCurrentValue goto label_0
   * else raise error for this row
   */
  r = code->branch_eq(RegOldValue, RegCurrentValue, label_0);
  assert(r == 0);
  r = code->interpret_exit_nok(ERROR_CONFLICT_FN_VIOLATION);
  assert(r == 0);
  r = code->def_label(label_0);
  assert(r == 0);
  r = code->interpret_exit_ok();
  assert(r == 0);
  r = code->finalise();
  assert(r == 0);
  return r;
}

static int row_conflict_fn_max_interpreted_program(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type,
    const NdbRecord *data_record, const uchar *, const uchar *new_data,
    const MY_BITMAP *, const MY_BITMAP *ai_cols, NdbInterpretedCode *code) {
  DBUG_TRACE;
  uint32 resolve_column = cfn_share->m_resolve_column;
  uint32 resolve_size = cfn_share->m_resolve_size;
  const uchar *field_ptr = (const uchar *)NdbDictionary::getValuePtr(
      data_record, (const char *)new_data, cfn_share->m_resolve_column);

  assert((resolve_size == 4) || (resolve_size == 8));

  if (unlikely(!bitmap_is_set(ai_cols, resolve_column))) {
    ndb_log_info("Replica: missing data for %s timestamp column %u.",
                 cfn_share->m_conflict_fn->name, resolve_column);
    return 1;
  }

  const uint label_0 = 0;
  const Uint32 RegNewValue = 1, RegCurrentValue = 2;
  int r;

  DBUG_PRINT(
      "info",
      ("Adding interpreted filter, existing value must be lt event new"));
  /*
   * read new value from record
   */
  union {
    uint32 new_value_32;
    uint64 new_value_64;
  };
  {
    if (resolve_size == 4) {
      memcpy(&new_value_32, field_ptr, resolve_size);
      DBUG_PRINT("info", ("  new_value_32: %u", new_value_32));
    } else {
      memcpy(&new_value_64, field_ptr, resolve_size);
      DBUG_PRINT("info",
                 ("  new_value_64: %llu", (unsigned long long)new_value_64));
    }
  }
  /*
   * Load registers RegNewValue and RegCurrentValue
   */
  if (resolve_size == 4)
    r = code->load_const_u32(RegNewValue, new_value_32);
  else
    r = code->load_const_u64(RegNewValue, new_value_64);
  assert(r == 0);
  r = code->read_attr(RegCurrentValue, resolve_column);
  assert(r == 0);
  /*
   * if RegNewValue > RegCurrentValue goto label_0
   * else raise error for this row
   */
  r = code->branch_gt(RegNewValue, RegCurrentValue, label_0);
  assert(r == 0);
  r = code->interpret_exit_nok(ERROR_CONFLICT_FN_VIOLATION);
  assert(r == 0);
  r = code->def_label(label_0);
  assert(r == 0);
  r = code->interpret_exit_ok();
  assert(r == 0);
  r = code->finalise();
  assert(r == 0);
  return r;
}

/**
  CFT_NDB_MAX

  To perform conflict resolution, an interpreted program is used to read
  the timestamp stored locally and compare to what is going to be applied.
  If timestamp is lower, an error for this operation (9999) will be raised,
  and new row will not be applied. The error codes for the operations will
  be checked on return.  For this to work is is vital that the operation
  is run with ignore error option.

  Note that for delete, this algorithm reverts to the OLD algorithm.
*/
static int row_conflict_fn_max(NDB_CONFLICT_FN_SHARE *cfn_share,
                               enum_conflicting_op_type op_type,
                               const NdbRecord *data_record,
                               const uchar *old_data, const uchar *new_data,
                               const MY_BITMAP *bi_cols,
                               const MY_BITMAP *ai_cols,
                               NdbInterpretedCode *code, Uint64 max_rep_epoch) {
  switch (op_type) {
    case WRITE_ROW:
      abort();
      return 1;
    case UPDATE_ROW:
      return row_conflict_fn_max_interpreted_program(
          cfn_share, op_type, data_record, old_data, new_data, bi_cols, ai_cols,
          code);
    case DELETE_ROW:
      /* Can't use max of new image, as there's no new image
       * for DELETE
       * Use OLD instead
       */
      return row_conflict_fn_old(cfn_share, op_type, data_record, old_data,
                                 new_data, bi_cols, ai_cols, code,
                                 max_rep_epoch);
    default:
      abort();
      return 1;
  }
}

/**
  CFT_NDB_MAX_DEL_WIN

  To perform conflict resolution, an interpreted program is used to read
  the timestamp stored locally and compare to what is going to be applied.
  If timestamp is lower, an error for this operation (9999) will be raised,
  and new row will not be applied. The error codes for the operations will
  be checked on return.  For this to work is is vital that the operation
  is run with ignore error option.

  In this variant, replicated DELETEs always succeed - no filter is added
  to them.
*/

static int row_conflict_fn_max_del_win(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type op_type,
    const NdbRecord *data_record, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *bi_cols, const MY_BITMAP *ai_cols,
    NdbInterpretedCode *code, Uint64) {
  switch (op_type) {
    case WRITE_ROW:
      abort();
      return 1;
    case UPDATE_ROW:
      return row_conflict_fn_max_interpreted_program(
          cfn_share, op_type, data_record, old_data, new_data, bi_cols, ai_cols,
          code);
    case DELETE_ROW:
      /* This variant always lets a received DELETE_ROW
       * succeed.
       */
      return 0;
    default:
      abort();
      return 1;
  }
}

/**
 *  CFT_NDB_MAX_INS:
 */

static int row_conflict_fn_max_ins(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type op_type,
    const NdbRecord *data_record, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *bi_cols, const MY_BITMAP *ai_cols,
    NdbInterpretedCode *code, Uint64 max_rep_epoch) {
  switch (op_type) {
    case WRITE_ROW:
    case UPDATE_ROW:
      return row_conflict_fn_max_interpreted_program(
          cfn_share, op_type, data_record, old_data, new_data, bi_cols, ai_cols,
          code);
    case DELETE_ROW:
      return row_conflict_fn_old(cfn_share, op_type, data_record, old_data,
                                 new_data, bi_cols, ai_cols, code,
                                 max_rep_epoch);
    default:
      abort();
      return 1;
  }
}

/**
 * CFT_NDB_MAX_DEL_WIN_INS:
 */

static int row_conflict_fn_max_del_win_ins(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type op_type,
    const NdbRecord *data_record, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *bi_cols, const MY_BITMAP *ai_cols,
    NdbInterpretedCode *code, Uint64) {
  switch (op_type) {
    case WRITE_ROW:
    case UPDATE_ROW:
      return row_conflict_fn_max_interpreted_program(
          cfn_share, op_type, data_record, old_data, new_data, bi_cols, ai_cols,
          code);
    case DELETE_ROW:
      return 0;
    default:
      abort();
      return 1;
  }
}

/**
  CFT_NDB_EPOCH

*/

static int row_conflict_fn_epoch(NDB_CONFLICT_FN_SHARE *,
                                 enum_conflicting_op_type op_type,
                                 const NdbRecord *, const uchar *,
                                 const uchar *, const MY_BITMAP *,
                                 const MY_BITMAP *, NdbInterpretedCode *code,
                                 Uint64 max_rep_epoch) {
  DBUG_TRACE;
  switch (op_type) {
    case WRITE_ROW:
      abort();
      return 1;
    case UPDATE_ROW:
    case DELETE_ROW:
    case READ_ROW: /* Read tracking */
    {
      const uint label_0 = 0;
      const Uint32 RegAuthor = 1, RegZero = 2, RegMaxRepEpoch = 1,
                   RegRowEpoch = 2;
      int r;

      r = code->load_const_u32(RegZero, 0);
      assert(r == 0);
      r = code->read_attr(RegAuthor, NdbDictionary::Column::ROW_AUTHOR);
      assert(r == 0);
      /* If last author was not local, assume no conflict */
      r = code->branch_ne(RegZero, RegAuthor, label_0);
      assert(r == 0);

      /*
       * Load registers RegMaxRepEpoch and RegRowEpoch
       */
      r = code->load_const_u64(RegMaxRepEpoch, max_rep_epoch);
      assert(r == 0);
      r = code->read_attr(RegRowEpoch, NdbDictionary::Column::ROW_GCI64);
      assert(r == 0);

      /*
       * if RegRowEpoch <= RegMaxRepEpoch goto label_0
       * else raise error for this row
       */
      r = code->branch_le(RegRowEpoch, RegMaxRepEpoch, label_0);
      assert(r == 0);
      r = code->interpret_exit_nok(ERROR_CONFLICT_FN_VIOLATION);
      assert(r == 0);
      r = code->def_label(label_0);
      assert(r == 0);
      r = code->interpret_exit_ok();
      assert(r == 0);
      r = code->finalise();
      assert(r == 0);
      return r;
    }
    default:
      abort();
      return 1;
  }
}

/**
 * CFT_NDB_EPOCH2
 */

static int row_conflict_fn_epoch2_primary(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type op_type,
    const NdbRecord *data_record, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *bi_cols, const MY_BITMAP *ai_cols,
    NdbInterpretedCode *code, Uint64 max_rep_epoch) {
  DBUG_TRACE;

  /* We use the normal NDB$EPOCH detection function */
  return row_conflict_fn_epoch(cfn_share, op_type, data_record, old_data,
                               new_data, bi_cols, ai_cols, code, max_rep_epoch);
}

static int row_conflict_fn_epoch2_secondary(NDB_CONFLICT_FN_SHARE *,
                                            enum_conflicting_op_type op_type,
                                            const NdbRecord *, const uchar *,
                                            const uchar *, const MY_BITMAP *,
                                            const MY_BITMAP *,
                                            NdbInterpretedCode *code, Uint64) {
  DBUG_TRACE;

  /* Only called for reflected update and delete operations
   * on the secondary.
   * These are returning operations which should only be
   * applied if the row in the database was last written
   * remotely (by the Primary)
   */

  switch (op_type) {
    case WRITE_ROW:
      abort();
      return 1;
    case UPDATE_ROW:
    case DELETE_ROW: {
      const uint label_0 = 0;
      const Uint32 RegAuthor = 1, RegZero = 2;
      int r;

      r = code->load_const_u32(RegZero, 0);
      assert(r == 0);
      r = code->read_attr(RegAuthor, NdbDictionary::Column::ROW_AUTHOR);
      assert(r == 0);
      r = code->branch_eq(RegZero, RegAuthor, label_0);
      assert(r == 0);
      /* Last author was not local, no conflict, apply */
      r = code->interpret_exit_ok();
      assert(r == 0);
      r = code->def_label(label_0);
      assert(r == 0);
      /* Last author was secondary-local, conflict, do not apply */
      r = code->interpret_exit_nok(ERROR_CONFLICT_FN_VIOLATION);
      assert(r == 0);

      r = code->finalise();
      assert(r == 0);
      return r;
    }
    default:
      abort();
      return 1;
  }
}

static int row_conflict_fn_epoch2(
    NDB_CONFLICT_FN_SHARE *cfn_share, enum_conflicting_op_type op_type,
    const NdbRecord *data_record, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *bi_cols, const MY_BITMAP *ai_cols,
    NdbInterpretedCode *code, Uint64 max_rep_epoch) {
  DBUG_TRACE;

  /**
   * NdbEpoch2 behaviour depends on the Slave conflict role variable
   *
   */
  switch (opt_ndb_slave_conflict_role) {
    case SCR_NONE:
      /* This is a problem */
      return 1;
    case SCR_PRIMARY:
      return row_conflict_fn_epoch2_primary(cfn_share, op_type, data_record,
                                            old_data, new_data, bi_cols,
                                            ai_cols, code, max_rep_epoch);
    case SCR_SECONDARY:
      return row_conflict_fn_epoch2_secondary(cfn_share, op_type, data_record,
                                              old_data, new_data, bi_cols,
                                              ai_cols, code, max_rep_epoch);
    case SCR_PASS:
      /* Do nothing */
      return 0;

    default:
      break;
  }

  abort();

  return 1;
}

/**
 * Conflict function setup infrastructure
 */

static const st_conflict_fn_arg_def resolve_col_args[] = {
    /* Arg type              Optional */
    {CFAT_COLUMN_NAME, false},
    {CFAT_END, false}};

static const st_conflict_fn_arg_def epoch_fn_args[] = {
    /* Arg type              Optional */
    {CFAT_EXTRA_GCI_BITS, true},
    {CFAT_END, false}};

static const st_conflict_fn_def conflict_fns[] = {
    {"NDB$MAX_INS", CFT_NDB_MAX_INS, &resolve_col_args[0],
     row_conflict_fn_max_ins, CF_USE_INTERP_WRITE},
    {"NDB$MAX_DEL_WIN_INS", CFT_NDB_MAX_DEL_WIN_INS, &resolve_col_args[0],
     row_conflict_fn_max_del_win_ins, CF_USE_INTERP_WRITE},
    {"NDB$MAX_DELETE_WIN", CFT_NDB_MAX_DEL_WIN, &resolve_col_args[0],
     row_conflict_fn_max_del_win, 0},
    {"NDB$MAX", CFT_NDB_MAX, &resolve_col_args[0], row_conflict_fn_max, 0},
    {"NDB$OLD", CFT_NDB_OLD, &resolve_col_args[0], row_conflict_fn_old, 0},
    {"NDB$EPOCH2_TRANS", CFT_NDB_EPOCH2_TRANS, &epoch_fn_args[0],
     row_conflict_fn_epoch2,
     CF_REFLECT_SEC_OPS | CF_USE_ROLE_VAR | CF_TRANSACTIONAL | CF_DEL_DEL_CFT},
    {"NDB$EPOCH2", CFT_NDB_EPOCH2, &epoch_fn_args[0], row_conflict_fn_epoch2,
     CF_REFLECT_SEC_OPS | CF_USE_ROLE_VAR},
    {"NDB$EPOCH_TRANS", CFT_NDB_EPOCH_TRANS, &epoch_fn_args[0],
     row_conflict_fn_epoch, CF_TRANSACTIONAL},
    {"NDB$EPOCH", CFT_NDB_EPOCH, &epoch_fn_args[0], row_conflict_fn_epoch, 0}};

static unsigned n_conflict_fns =
    sizeof(conflict_fns) / sizeof(struct st_conflict_fn_def);

int parse_conflict_fn_spec(const char *conflict_fn_spec,
                           const st_conflict_fn_def **conflict_fn,
                           st_conflict_fn_arg *args, Uint32 *max_args,
                           char *msg, uint msg_len) {
  DBUG_TRACE;

  Uint32 no_args = 0;
  const char *ptr = conflict_fn_spec;
  const char *error_str = "unknown conflict resolution function";
  /* remove whitespace */
  while (*ptr == ' ' && *ptr != '\0') ptr++;

  DBUG_PRINT("info", ("parsing %s", conflict_fn_spec));

  for (unsigned i = 0; i < n_conflict_fns; i++) {
    const st_conflict_fn_def &fn = conflict_fns[i];

    uint len = (uint)strlen(fn.name);
    if (strncmp(ptr, fn.name, len)) continue;

    DBUG_PRINT("info", ("found function %s", fn.name));

    /* skip function name */
    ptr += len;

    /* remove whitespace */
    while (*ptr == ' ' && *ptr != '\0') ptr++;

    /* next '(' */
    if (*ptr != '(') {
      error_str = "missing '('";
      DBUG_PRINT("info", ("parse error %s", error_str));
      break;
    }
    ptr++;

    /* find all arguments */
    for (;;) {
      if (no_args >= *max_args) {
        error_str = "too many arguments";
        DBUG_PRINT("info", ("parse error %s", error_str));
        break;
      }

      /* expected type */
      enum enum_conflict_fn_arg_type type =
          conflict_fns[i].arg_defs[no_args].arg_type;

      /* remove whitespace */
      while (*ptr == ' ' && *ptr != '\0') ptr++;

      if (type == CFAT_END) {
        args[no_args].type = type;
        error_str = nullptr;
        break;
      }

      /* arg */
      /* Todo : Should support comma as an arg separator? */
      const char *start_arg = ptr;
      while (*ptr != ')' && *ptr != ' ' && *ptr != '\0') ptr++;
      const char *end_arg = ptr;

      bool optional_arg = conflict_fns[i].arg_defs[no_args].optional;
      /* any arg given? */
      if (start_arg == end_arg) {
        if (!optional_arg) {
          error_str = "missing function argument";
          DBUG_PRINT("info", ("parse error %s", error_str));
          break;
        } else {
          /* Arg was optional, and not present
           * Must be at end of args, finish parsing
           */
          args[no_args].type = CFAT_END;
          error_str = nullptr;
          break;
        }
      }

      uint len = (uint)(end_arg - start_arg);
      args[no_args].type = type;

      DBUG_PRINT("info", ("found argument %s %u", start_arg, len));

      bool arg_processing_error = false;
      switch (type) {
        case CFAT_COLUMN_NAME: {
          /* Copy column name out into argument's buffer */
          char *dest = &args[no_args].resolveColNameBuff[0];

          memcpy(dest, start_arg,
                 (len < (uint)NAME_CHAR_LEN ? len : NAME_CHAR_LEN));
          dest[len] = '\0';
          break;
        }
        case CFAT_EXTRA_GCI_BITS: {
          /* Map string to number and check it's in range etc */
          char *end_of_arg = const_cast<char *>(end_arg);
          Uint32 bits = strtoul(start_arg, &end_of_arg, 0);
          DBUG_PRINT("info", ("Using %u as the number of extra bits", bits));

          if (bits > 31) {
            arg_processing_error = true;
            error_str = "Too many extra Gci bits";
            DBUG_PRINT("info", ("%s", error_str));
            break;
          }
          /* Num bits seems ok */
          args[no_args].extraGciBits = bits;
          break;
        }
        case CFAT_END:
          abort();
      }

      if (arg_processing_error) break;
      no_args++;
    }

    if (error_str) break;

    /* remove whitespace */
    while (*ptr == ' ' && *ptr != '\0') ptr++;

    /* next ')' */
    if (*ptr != ')') {
      error_str = "missing ')'";
      break;
    }
    ptr++;

    /* remove whitespace */
    while (*ptr == ' ' && *ptr != '\0') ptr++;

    /* garbage in the end? */
    if (*ptr != '\0') {
      error_str = "garbage in the end";
      break;
    }

    /* Update ptrs to conflict fn + # of args */
    *conflict_fn = &conflict_fns[i];
    *max_args = no_args;

    return 0;
  }
  /* parse error */
  snprintf(msg, msg_len, "%s, %s at '%s'", conflict_fn_spec, error_str, ptr);
  DBUG_PRINT("info", ("%s", msg));
  return -1;
}

static uint slave_check_resolve_col_type(const NDBTAB *ndbtab,
                                         uint field_index) {
  DBUG_TRACE;
  const NDBCOL *c = ndbtab->getColumn(field_index);
  uint sz = 0;
  switch (c->getType()) {
    case NDBCOL::Unsigned:
      sz = sizeof(Uint32);
      DBUG_PRINT("info", ("resolve column Uint32 %u", field_index));
      break;
    case NDBCOL::Bigunsigned:
      sz = sizeof(Uint64);
      DBUG_PRINT("info", ("resolve column Uint64 %u", field_index));
      break;
    default:
      DBUG_PRINT("info", ("resolve column %u has wrong type", field_index));
      break;
  }
  return sz;
}

static int slave_set_resolve_fn(Ndb *ndb, NDB_CONFLICT_FN_SHARE **ppcfn_share,
                                const char *dbName, const char *tabName,
                                const NDBTAB *ndbtab, uint field_index,
                                uint resolve_col_sz,
                                const st_conflict_fn_def *conflict_fn,
                                uint8 flags) {
  DBUG_TRACE;

  NDB_CONFLICT_FN_SHARE *cfn_share = *ppcfn_share;
  const char *ex_suffix = NDB_EXCEPTIONS_TABLE_SUFFIX;
  if (cfn_share == nullptr) {
    *ppcfn_share = cfn_share = (NDB_CONFLICT_FN_SHARE *)my_malloc(
        PSI_INSTRUMENT_ME, sizeof(NDB_CONFLICT_FN_SHARE),
        MYF(MY_WME | ME_FATALERROR));
    slave_reset_conflict_fn(cfn_share);
  }
  cfn_share->m_conflict_fn = conflict_fn;

  /* Calculate resolve col stuff (if relevant) */
  cfn_share->m_resolve_size = resolve_col_sz;
  cfn_share->m_resolve_column = field_index;
  cfn_share->m_flags = flags;

  /* Init Exceptions Table Writer */
  new (&cfn_share->m_ex_tab_writer) ExceptionsTableWriter();
  /* Check for '$EX' or '$ex' suffix in table name */
  for (int tries = 2; tries-- > 0;
       ex_suffix = (tries == 1)
                       ? (const char *)NDB_EXCEPTIONS_TABLE_SUFFIX_LOWER
                       : NullS) {
    /* get exceptions table */
    char ex_tab_name[FN_REFLEN];
    strxnmov(ex_tab_name, sizeof(ex_tab_name), tabName, ex_suffix, NullS);
    Ndb_table_guard ndbtab_g(ndb, dbName, ex_tab_name);
    const NDBTAB *ex_tab = ndbtab_g.get_table();
    if (ex_tab) {
      char msgBuf[FN_REFLEN];
      const char *msg = nullptr;
      if (cfn_share->m_ex_tab_writer.init(ndbtab, ex_tab, msgBuf,
                                          sizeof(msgBuf), &msg) == 0) {
        /* Ok */
        /* Hold our table reference outside the table_guard scope */
        ndbtab_g.release();

        /* Table looked suspicious, warn user */
        if (msg) ndb_log_warning("Replica: %s", msg);

        ndb_log_verbose(1, "Replica: Table %s.%s logging exceptions to %s.%s",
                        dbName, tabName, dbName, ex_tab_name);
      } else {
        ndb_log_warning("Replica: %s", msg);
      }
      break;
    } /* if (ex_tab) */
  }
  return 0;
}

bool is_exceptions_table(const char *table_name) {
  size_t len = strlen(table_name);
  size_t suffixlen = strlen(NDB_EXCEPTIONS_TABLE_SUFFIX);
  if (len > suffixlen &&
      (strcmp(table_name + len - suffixlen,
              lower_case_table_names ? NDB_EXCEPTIONS_TABLE_SUFFIX_LOWER
                                     : NDB_EXCEPTIONS_TABLE_SUFFIX) == 0)) {
    return true;
  }
  return false;
}

int setup_conflict_fn(Ndb *ndb, NDB_CONFLICT_FN_SHARE **ppcfn_share,
                      const char *dbName, const char *tabName,
                      bool tableBinlogUseUpdate,
                      const NdbDictionary::Table *ndbtab, char *msg,
                      uint msg_len, const st_conflict_fn_def *conflict_fn,
                      const st_conflict_fn_arg *args, const Uint32 num_args) {
  DBUG_TRACE;

  if (is_exceptions_table(tabName)) {
    snprintf(msg, msg_len,
             "Table %s.%s is exceptions table: not using conflict function %s",
             dbName, tabName, conflict_fn->name);
    DBUG_PRINT("info", ("%s", msg));
    return 0;
  }

  /* setup the function */
  switch (conflict_fn->type) {
    case CFT_NDB_MAX:
    case CFT_NDB_OLD:
    case CFT_NDB_MAX_DEL_WIN:
    case CFT_NDB_MAX_INS:
    case CFT_NDB_MAX_DEL_WIN_INS: {
      if (num_args != 1) {
        snprintf(msg, msg_len, "Incorrect arguments to conflict function");
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      /* Now try to find the column in the table */
      int colNum = -1;
      const char *resolveColName = args[0].resolveColNameBuff;
      int resolveColNameLen = (int)strlen(resolveColName);

      for (int j = 0; j < ndbtab->getNoOfColumns(); j++) {
        const char *colName = ndbtab->getColumn(j)->getName();

        if (strncmp(colName, resolveColName, resolveColNameLen) == 0 &&
            colName[resolveColNameLen] == '\0') {
          colNum = j;
          break;
        }
      }
      if (colNum == -1) {
        snprintf(msg, msg_len, "Could not find resolve column %s.",
                 resolveColName);
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      const uint resolve_col_sz = slave_check_resolve_col_type(ndbtab, colNum);
      if (resolve_col_sz == 0) {
        /* wrong data type */
        slave_reset_conflict_fn(*ppcfn_share);
        snprintf(msg, msg_len, "Column '%s' has wrong datatype",
                 resolveColName);
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      if (slave_set_resolve_fn(ndb, ppcfn_share, dbName, tabName, ndbtab,
                               colNum, resolve_col_sz, conflict_fn, CFF_NONE)) {
        snprintf(msg, msg_len,
                 "Unable to setup conflict resolution using column '%s'",
                 resolveColName);
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      /* Success, update message */
      snprintf(msg, msg_len,
               "Table %s.%s using conflict_fn %s on attribute %s.", dbName,
               tabName, conflict_fn->name, resolveColName);
      break;
    }
    case CFT_NDB_EPOCH2:
    case CFT_NDB_EPOCH2_TRANS: {
      /* Check how updates will be logged... */
      const bool log_update_as_write = (!tableBinlogUseUpdate);
      if (log_update_as_write) {
        snprintf(msg, msg_len,
                 "Table %s.%s configured to log updates as writes.  "
                 "Not suitable for %s.",
                 dbName, tabName, conflict_fn->name);
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }
    }
      /* Fall through - for the rest of the EPOCH* processing... */
      [[fallthrough]];
    case CFT_NDB_EPOCH:
    case CFT_NDB_EPOCH_TRANS: {
      if (num_args > 1) {
        snprintf(msg, msg_len, "Too many arguments to conflict function");
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      /* Check that table doesn't have Blobs as we don't support that */
      if (ndb_table_has_blobs(ndbtab)) {
        snprintf(msg, msg_len, "Table has Blob column(s), not suitable for %s.",
                 conflict_fn->name);
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      /* Check that table has required extra meta-columns */
      /* Todo : Could warn if extra gcibits is insufficient to
       * represent SavePeriod/EpochPeriod
       */
      if (ndbtab->getExtraRowGciBits() == 0)
        ndb_log_info("Replica: Table %s.%s : %s, low epoch resolution", dbName,
                     tabName, conflict_fn->name);

      if (ndbtab->getExtraRowAuthorBits() == 0) {
        snprintf(msg, msg_len, "No extra row author bits in table.");
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }

      if (slave_set_resolve_fn(ndb, ppcfn_share, dbName, tabName, ndbtab,
                               0,  // field_no
                               0,  // resolve_col_sz
                               conflict_fn, CFF_REFRESH_ROWS)) {
        snprintf(msg, msg_len, "unable to setup conflict resolution");
        DBUG_PRINT("info", ("%s", msg));
        return -1;
      }
      /* Success, update message */
      snprintf(msg, msg_len, "Table %s.%s using conflict_fn %s.", dbName,
               tabName, conflict_fn->name);

      break;
    }
    case CFT_NUMBER_OF_CFTS:
    case CFT_NDB_UNDEF:
      abort();
  }
  return 0;
}

void teardown_conflict_fn(Ndb *ndb, NDB_CONFLICT_FN_SHARE *cfn_share) {
  if (cfn_share && cfn_share->m_ex_tab_writer.hasTable() && ndb) {
    cfn_share->m_ex_tab_writer.mem_free(ndb);
  }

  // Release the NDB_CONFLICT_FN_SHARE which was allocated
  // in setup_conflict_fn()
  my_free(cfn_share);
}

void slave_reset_conflict_fn(NDB_CONFLICT_FN_SHARE *cfn_share) {
  if (cfn_share) {
    cfn_share->m_conflict_fn = nullptr;
    cfn_share->m_resolve_size = 0;
    cfn_share->m_resolve_column = 0;
    cfn_share->m_flags = 0;
  }
}
