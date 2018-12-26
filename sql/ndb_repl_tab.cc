/*
   Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_repl_tab.h"

#include <stdio.h>

#include "mf_wcomp.h"
#include "sql/ha_ndbcluster_tables.h"
#include "sql/mysqld.h"                // system_charset_info
#include "sql/ndb_share.h"
#include "sql/ndb_sleep.h"
#include "sql/ndb_table_guard.h"

Ndb_rep_tab_key::Ndb_rep_tab_key(const char* _db,
                                 const char* _table_name,
                                 uint _server_id)
{
  uint db_len= (uint) strlen(_db);
  uint tabname_len = (uint) strlen(_table_name);
  assert(DB_MAXLEN < 256); /* Fits in Varchar */
  assert(db_len <= DB_MAXLEN);
  assert(tabname_len <= TABNAME_MAXLEN);

  memcpy(&db[1], _db, db_len);
  db[ 0 ]= db_len;

  memcpy(&table_name[1], _table_name, tabname_len);
  table_name[ 0 ]= tabname_len;

  server_id= _server_id;

  null_terminate_strings();
}

void Ndb_rep_tab_key::null_terminate_strings()
{
  assert((uint) db[0] <= DB_MAXLEN);
  assert((uint) table_name[0] <= TABNAME_MAXLEN);
  db[ db[0] + 1] = '\0';
  table_name[ table_name[0] + 1] = '\0';
}

int
Ndb_rep_tab_key::attempt_match(const char* keyptr,
                               const uint keylen,
                               const char* candidateptr,
                               const uint candidatelen,
                               const int exactmatchvalue)
{
  if (my_strnncoll(system_charset_info,
                   (const uchar*) keyptr,
                   keylen,
                   (const uchar*) candidateptr,
                   candidatelen) == 0)
  {
    /* Exact match */
    return exactmatchvalue;
  }
  else if (my_wildcmp(system_charset_info,
                      keyptr,
                      keyptr + keylen,
                      candidateptr,
                      candidateptr + candidatelen,
                      '\\', wild_one, wild_many) == 0)
  {
    /* Wild match */
    return 0;
  }

  /* No match */
  return -1;
};

int
Ndb_rep_tab_key::get_match_quality(const Ndb_rep_tab_key* key,
                                   const Ndb_rep_tab_key* candidate_row)
{
  /* 0= No match
     1= Loosest match
     8= Best match

     Actual mapping is :
     db    table    serverid  Quality
     W     W        W         1
     W     W        =         2
     W     =        W         3
     W     =        =         4
     =     W        W         5
     =     W        =         6
     =     =        W         7
     =     =        =         8
  */
  int quality = MIN_MATCH_VAL;

  int rc;
  if ((rc = attempt_match(&key->db[1],
                          key->db[0],
                          &candidate_row->db[1],
                          candidate_row->db[0],
                          EXACT_MATCH_DB)) == -1)
  {
    /* No match, drop out now */
    return 0;
  }
  quality+= rc;

  if ((rc = attempt_match(&key->table_name[1],
                          key->table_name[0],
                          &candidate_row->table_name[1],
                          candidate_row->table_name[0],
                          EXACT_MATCH_TABLE_NAME)) == -1)
  {
    /* No match, drop out now */
    return 0;
  }
  quality+= rc;

  if (candidate_row->server_id == key->server_id)
  {
    /* Exact match */
    quality += EXACT_MATCH_SERVER_ID;
  }
  else if (candidate_row->server_id != 0)
  {
    /* No match */
    return 0;
  }

  return quality;
};

Ndb_rep_tab_row::Ndb_rep_tab_row()
  : binlog_type(0), cfs_is_null(true)
{
  memset(conflict_fn_spec, 0, sizeof(conflict_fn_spec));
}
const char* Ndb_rep_tab_reader::ndb_rep_db= NDB_REP_DB;
const char* Ndb_rep_tab_reader::ndb_replication_table = "ndb_replication";
const char* Ndb_rep_tab_reader::nrt_db= "db";
const char* Ndb_rep_tab_reader::nrt_table_name= "table_name";
const char* Ndb_rep_tab_reader::nrt_server_id= "server_id";
const char* Ndb_rep_tab_reader::nrt_binlog_type= "binlog_type";
const char* Ndb_rep_tab_reader::nrt_conflict_fn= "conflict_fn";

Ndb_rep_tab_reader::Ndb_rep_tab_reader()
  : binlog_flags(NBT_DEFAULT),
    conflict_fn_spec(NULL),
    warning_msg(NULL)
{
}

int Ndb_rep_tab_reader::check_schema(const NdbDictionary::Table* reptab,
                                     const char** error_str)
{
  DBUG_ENTER("check_schema");
  *error_str= NULL;

  const NdbDictionary::Column
    *col_db, *col_table_name, *col_server_id, *col_binlog_type, *col_conflict_fn;
  if (reptab->getNoOfPrimaryKeys() != 3)
  {
    *error_str= "Wrong number of primary key parts, expected 3";
    DBUG_RETURN(-2);
  }
  col_db= reptab->getColumn(*error_str= nrt_db);
  if (col_db == NULL ||
      !col_db->getPrimaryKey() ||
      col_db->getType() != NdbDictionary::Column::Varbinary)
    DBUG_RETURN(-1);
  col_table_name= reptab->getColumn(*error_str= nrt_table_name);
  if (col_table_name == NULL ||
      !col_table_name->getPrimaryKey() ||
      col_table_name->getType() != NdbDictionary::Column::Varbinary)
    DBUG_RETURN(-1);
  col_server_id= reptab->getColumn(*error_str= nrt_server_id);
  if (col_server_id == NULL ||
      !col_server_id->getPrimaryKey() ||
      col_server_id->getType() != NdbDictionary::Column::Unsigned)
    DBUG_RETURN(-1);
  col_binlog_type= reptab->getColumn(*error_str= nrt_binlog_type);
  if (col_binlog_type == NULL ||
      col_binlog_type->getPrimaryKey() ||
      col_binlog_type->getType() != NdbDictionary::Column::Unsigned)
    DBUG_RETURN(-1);
  col_conflict_fn= reptab->getColumn(*error_str= nrt_conflict_fn);
  if (col_conflict_fn != NULL)
  {
    if ((col_conflict_fn->getPrimaryKey()) ||
        (col_conflict_fn->getType() != NdbDictionary::Column::Varbinary))
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

int
Ndb_rep_tab_reader::scan_candidates(Ndb* ndb,
                                    const NdbDictionary::Table* reptab,
                                    const char* db,
                                    const char* table_name,
                                    uint server_id,
                                    Ndb_rep_tab_row& best_match)
{
  uint retries= 100;
  int retry_sleep= 30; /* 30 milliseconds, transaction */
  int best_match_quality= 0;
  NdbError ok;
  NdbError ndberror;

  /* Loop to enable temporary error retries */
  while(true)
  {
    ndberror = ok; /* reset */
    NdbTransaction *trans= ndb->startTransaction();
    if (trans == NULL)
    {
      ndberror= ndb->getNdbError();

      if (ndberror.status == NdbError::TemporaryError)
      {
        if (retries--)
        {
          ndb_retry_sleep(retry_sleep);
          continue;
        }
      }
      break;
    }
    NdbRecAttr* ra_binlog_type= NULL;
    NdbRecAttr* ra_conflict_fn_spec= NULL;
    Ndb_rep_tab_row row;
    bool have_conflict_fn_col = (reptab->getColumn(nrt_conflict_fn) != NULL);

    /* Define scan op on ndb_replication */
    NdbScanOperation* scanOp = trans->getNdbScanOperation(reptab);
    if (scanOp == NULL) { ndberror= trans->getNdbError(); break; }

    if ((scanOp->readTuples(NdbScanOperation::LM_CommittedRead) != 0) ||
        (scanOp->getValue(nrt_db, (char*) row.key.db) == NULL) ||
        (scanOp->getValue(nrt_table_name, (char*) row.key.table_name) == NULL) ||
        (scanOp->getValue(nrt_server_id, (char*) &row.key.server_id) == NULL) ||
        ((ra_binlog_type = scanOp->getValue(nrt_binlog_type, (char*) &row.binlog_type)) == NULL) ||
        (have_conflict_fn_col &&
         ((ra_conflict_fn_spec=
           scanOp->getValue(nrt_conflict_fn, (char*) row.conflict_fn_spec)) == NULL)))
    {
      ndberror= scanOp->getNdbError();
      break;
    }

    if (trans->execute(NdbTransaction::NoCommit,
                       NdbOperation::AO_IgnoreError))
    {
      ndberror= trans->getNdbError();
      ndb->closeTransaction(trans);

      if (ndberror.status == NdbError::TemporaryError)
      {
        if (retries--)
        {
          ndb_retry_sleep(retry_sleep);
          continue;
        }
      }
      break;
    }

    /* Scroll through results, looking for best match */
    DBUG_PRINT("info", ("Searching ndb_replication for %s.%s %u",
                        db, table_name, server_id));

    bool ambiguous_match = false;
    Ndb_rep_tab_key searchkey(db, table_name, server_id);
    int scan_rc;
    while ((scan_rc= scanOp->nextResult(true)) == 0)
    {
      if (ra_binlog_type->isNULL() == 1)
      {
        row.binlog_type= NBT_DEFAULT;
      }
      if (ra_conflict_fn_spec)
      {
        row.set_conflict_fn_spec_null(ra_conflict_fn_spec->isNULL() == 1);
      }

      /* Compare row to searchkey to get quality of match */
      int match_quality= Ndb_rep_tab_key::get_match_quality(&searchkey,
                                                            &row.key);
#ifndef DBUG_OFF
      {
        row.null_terminate_strings();

        DBUG_PRINT("info", ("Candidate : %s.%s %u : %u %s"
                            " Match quality : %u.",
                            row.key.get_db(),
                            row.key.get_table_name(),
                            row.key.server_id,
                            row.binlog_type,
                            row.get_conflict_fn_spec(),
                            match_quality));
      }
#endif

      if (match_quality > 0)
      {
        if (match_quality == best_match_quality)
        {
          ambiguous_match = true;
          /* Ambiguous matches...*/
          snprintf(warning_msg_buffer, sizeof(warning_msg_buffer),
                      "Ambiguous matches in %s.%s for %s.%s (%u)."
                      "Candidates : %s.%s (%u), %s.%s (%u).",
                      ndb_rep_db, ndb_replication_table,
                      db, table_name, server_id,
                      &best_match.key.db[1],
                      &best_match.key.table_name[1],
                      best_match.key.server_id,
                      &row.key.db[1],
                      &row.key.table_name[1],
                      row.key.server_id);
          DBUG_PRINT("info", ("%s", warning_msg_buffer));
        }
        if (match_quality > best_match_quality)
        {
          /* New best match */
          best_match= row;
          best_match_quality = match_quality;
          ambiguous_match = false;

          if (best_match_quality == Ndb_rep_tab_key::EXACT_MATCH_QUALITY)
          {
            /* We're done */
            break;
          }
        }
      } /* if (match_quality > 0) */
    } /* while ((scan_rc= scanOp->nextResult(true)) */

    if (scan_rc < 0)
    {
      ndberror= scanOp->getNdbError();
      if (ndberror.status == NdbError::TemporaryError)
      {
        if (retries--)
        {
          ndb->closeTransaction(trans);
          ndb_retry_sleep(retry_sleep);
          continue;
        }
      }
    }

    ndb->closeTransaction(trans);

    if (ambiguous_match)
    {
      warning_msg= warning_msg_buffer;
      best_match_quality = -1;
    }

    break;
  } /* while(true) */

  if (ndberror.code != 0)
  {
    snprintf(warning_msg_buffer, sizeof(warning_msg_buffer),
                "Unable to retrieve %s.%s, logging and "
                "conflict resolution may not function "
                "as intended (ndberror %u)",
                ndb_rep_db, ndb_replication_table,
                ndberror.code);
    warning_msg= warning_msg_buffer;
    best_match_quality = -1;
  }

  return best_match_quality;
}

int
Ndb_rep_tab_reader::lookup(Ndb* ndb,
                           /* Keys */
                           const char* db,
                           const char* table_name,
                           uint server_id)
{
  DBUG_ENTER("lookup");
  int error= 0;
  NdbError ndberror;
  const char *error_str= "<none>";

  /* Set results to defaults */
  binlog_flags= NBT_DEFAULT;
  conflict_fn_spec= NULL;
  warning_msg= NULL;

  ndb->setDatabaseName(ndb_rep_db);
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, ndb_replication_table);
  const NdbDictionary::Table *reptab= ndbtab_g.get_table();

  do
  {
    if (reptab == NULL)
    {
      if (dict->getNdbError().classification == NdbError::SchemaError ||
          dict->getNdbError().code == 4009)
      {
        DBUG_PRINT("info", ("No %s.%s table", ndb_rep_db, ndb_replication_table));
        DBUG_RETURN(0);
      }
      else
      {
        error= 0;
        ndberror= dict->getNdbError();
        break;
      }
    }

    if ((error = check_schema(reptab, &error_str)) != 0)
    {
      DBUG_PRINT("info", ("check_schema failed : %u, error_str : %s",
                          error, error_str));
      break;
    }

    Ndb_rep_tab_row best_match_row;

    int best_match_quality = scan_candidates(ndb,
                                             reptab,
                                             db,
                                             table_name,
                                             server_id,
                                             best_match_row);

    DBUG_PRINT("info", ("Best match at quality : %u", best_match_quality));

    if (best_match_quality == -1)
    {
      /* Problem in matching, message already set */
      assert(warning_msg != NULL);
      error= -3;
      break;
    }
    if (best_match_quality == 0)
    {
      /* No match : Use defaults */
    }
    else
    {
      /* Have a matching row, copy out values */
      /* Ensure VARCHARs are usable as strings */
      best_match_row.null_terminate_strings();

      binlog_flags= (enum Ndb_binlog_type) best_match_row.binlog_type;

      if (best_match_row.cfs_is_null)
      {
        DBUG_PRINT("info", ("Conflict FN SPEC is Null"));
        /* No conflict fn spec */
        conflict_fn_spec= NULL;
      }
      else
      {
        const char* conflict_fn = best_match_row.get_conflict_fn_spec();
        uint len= (uint) strlen(conflict_fn);
        if ((len + 1) > sizeof(conflict_fn_buffer))
        {
          error= -2;
          error_str= "Conflict function specification too long.";
          break;
        }
        memcpy(conflict_fn_buffer, conflict_fn, len);
        conflict_fn_buffer[len] = '\0';
        conflict_fn_spec = conflict_fn_buffer;
      }
    }
  } while(0);

  /* Error handling */
  if (error == 0)
  {
    if (ndberror.code != 0)
    {
      snprintf(warning_msg_buffer, sizeof(warning_msg_buffer),
                  "Unable to retrieve %s.%s, logging and "
                  "conflict resolution may not function "
                  "as intended (ndberror %u)",
                  ndb_rep_db, ndb_replication_table,
                  ndberror.code);
      warning_msg= warning_msg_buffer;
      error= -4;
    }
  }
  else
  {
    switch (error)
    {
    case -1:
      snprintf(warning_msg_buffer, sizeof(warning_msg_buffer),
                  "Missing or wrong type for column '%s'", error_str);
      break;
    case -2:
      snprintf(warning_msg_buffer, sizeof(warning_msg_buffer), "%s", error_str);
      break;
    case -3:
      /* Message already set */
      break;
    default:
      abort();
    }
    warning_msg= warning_msg_buffer;
    error= 0; /* No real error, just use defaults */
  }

  DBUG_PRINT("info", ("Rc : %d Retrieved Binlog flags : %u and function spec : %s",
                      error, binlog_flags, (conflict_fn_spec != NULL ?conflict_fn_spec:
                                             "NULL")));

  DBUG_RETURN(error);
}

Uint32
Ndb_rep_tab_reader::get_binlog_flags() const
{
  return binlog_flags;
}

const char*
Ndb_rep_tab_reader::get_conflict_fn_spec() const
{
  return conflict_fn_spec;
}

const char*
Ndb_rep_tab_reader::get_warning_message() const
{
  return warning_msg;
}
