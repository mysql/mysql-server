/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_REPL_TAB_H
#define NDB_REPL_TAB_H

#include <assert.h>

#include "my_inttypes.h"
#include "mysql_com.h" /* NAME_CHAR_LEN */
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

/*
  Ndb_rep_tab_key

  This class represents the key columns of the
  mysql.ndb_replication system table
  It is used when reading values from that table
*/
class Ndb_rep_tab_key {
 public:
  static const uint DB_MAXLEN = NAME_CHAR_LEN - 1;
  static const uint TABNAME_MAXLEN = NAME_CHAR_LEN - 1;

  /* Char arrays in varchar format with 1 length byte and
   * trailing 0
   */
  char db[DB_MAXLEN + 2];
  char table_name[TABNAME_MAXLEN + 2];
  uint server_id;

  Ndb_rep_tab_key() {
    db[0] = 0;
    table_name[0] = 0;
    server_id = 0;
  }

  /* Constructor from normal null terminated strings */
  Ndb_rep_tab_key(const char *_db, const char *_table_name, uint _server_id);

  /* Add null terminators to VARCHAR format string values */
  void null_terminate_strings();

  const char *get_db() const { return &db[1]; }

  const char *get_table_name() const { return &table_name[1]; }

  static const int MIN_MATCH_VAL = 1;
  static const int EXACT_MATCH_DB = 4;
  static const int EXACT_MATCH_TABLE_NAME = 2;
  static const int EXACT_MATCH_SERVER_ID = 1;

  static const int EXACT_MATCH_QUALITY = MIN_MATCH_VAL + EXACT_MATCH_DB +
                                         EXACT_MATCH_TABLE_NAME +
                                         EXACT_MATCH_SERVER_ID;

  /*
    This static method attempts an exact, then a wild
    match between the passed key (with optional wild
    characters), and the passed candidate row
    returns :
     1  : Exact match
     0  : Wild match
     -1 : No match
  */
  static int attempt_match(const char *keyptr, const uint keylen,
                           const char *candidateptr, const uint candidatelen,
                           const int exactmatchvalue);

  /* This static method compares a fixed key value with
   * a possibly wildcard containing candidate_row.
   * If there is no match, 0 is returned.
   * >0 means there is a match, with larger numbers
   * indicating a better match quality.
   * An exact match returns EXACT_MATCH_QUALITY
   */
  static int get_match_quality(const Ndb_rep_tab_key *key,
                               const Ndb_rep_tab_key *candidate_row);
};

/*
  Ndb_rep_tab_row

  This class represents a row in the mysql.ndb_replication table
*/
class Ndb_rep_tab_row {
 public:
  static const uint MAX_CONFLICT_FN_SPEC_LEN = 255;
  static const uint CONFLICT_FN_SPEC_BUF_LEN =
      MAX_CONFLICT_FN_SPEC_LEN + 1; /* Trailing '\0' */

  Ndb_rep_tab_key key;
  uint binlog_type;
  bool cfs_is_null;
  /* Buffer has space for leading length byte */
  char conflict_fn_spec[CONFLICT_FN_SPEC_BUF_LEN + 1];

  Ndb_rep_tab_row();

  void null_terminate_strings() {
    key.null_terminate_strings();
    uint speclen = 0;
    speclen = conflict_fn_spec[0];

    assert(speclen <= MAX_CONFLICT_FN_SPEC_LEN);
    conflict_fn_spec[1 + speclen] = '\0';
  }

  const char *get_conflict_fn_spec() { return &conflict_fn_spec[1]; }

  void set_conflict_fn_spec_null(bool null) {
    if (null) {
      cfs_is_null = true;
      conflict_fn_spec[0] = 0;
      conflict_fn_spec[1] = 0;
    } else {
      cfs_is_null = false;
    }
  }
};

/**
   Ndb_rep_tab_reader

   A helper class for accessing the mysql.ndb_replication
   table
*/
class Ndb_rep_tab_reader {
 private:
  static const char *ndb_rep_db;
  static const char *ndb_replication_table;
  static const char *nrt_db;
  static const char *nrt_table_name;
  static const char *nrt_server_id;
  static const char *nrt_binlog_type;
  static const char *nrt_conflict_fn;

  Uint32 binlog_flags;
  char conflict_fn_buffer[Ndb_rep_tab_row::CONFLICT_FN_SPEC_BUF_LEN];
  char warning_msg_buffer[FN_REFLEN];

  const char *conflict_fn_spec;
  const char *warning_msg;

  /**
     check_schema

     Checks that the schema of the mysql.ndb_replication table
     is acceptable.
     Returns
     0 if ok
     -1 if a column has an error.  Col name in error_str
     -2 if there's a more general error.  Error description in
        error_str
  */
  static int check_schema(const NdbDictionary::Table *reptab,
                          const char **error_str);

  /**
     scan_candidates

     Scans the ndb_replication table for rows matching the
     passed db, table_name, server_id triple.
     Returns the quality of the match made.

     -1 = Error in processing, see msg
     0 = No match, use defaults.
     >0 = Use data in best_match

     if msg is set on return it contains a warning.
     Warnings may be produces in non error scenarios
  */
  int scan_candidates(Ndb *ndb, const NdbDictionary::Table *reptab,
                      const char *db, const char *table_name, uint server_id,
                      Ndb_rep_tab_row &best_match);

 public:
  Ndb_rep_tab_reader();
  ~Ndb_rep_tab_reader() {}

  /**
     lookup

     lookup scans the mysql.ndb_replication table for
     the best matching entry for the supplied db,
     table_name, server_id triple.
     A buffer for the conflict_fn spec, and for any
     error or warning messages must be supplied.
     The passed binlog_flags, conflict_fn_spec and
     message may be updated as a result

     Returns :
       0  : Success.
       <0 : Error.
  */
  int lookup(Ndb *ndb,
             /* Keys */
             const char *db, const char *table_name, uint server_id);

  /* Following only valid after a call to lookup() */
  Uint32 get_binlog_flags() const;
  const char *get_conflict_fn_spec() const;
  const char *get_warning_message() const;
};

#endif
