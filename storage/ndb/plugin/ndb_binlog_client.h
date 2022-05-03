/*
   Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_BINLOG_CLIENT_H
#define NDB_BINLOG_CLIENT_H

#include <string>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

class Ndb_event_data;
class NdbEventOperation;
struct NDB_SHARE;
struct st_conflict_fn_def;
struct st_conflict_fn_arg;

namespace dd {
class Table;
}

class Ndb_binlog_client {
  class THD *m_thd;
  const char *m_dbname;
  const char *m_tabname;

  /**
   * @brief log_warning, push the message as warning for user threads and
   *                     write the message to log file for other threads
   */
  void log_warning(uint code, const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 3, 4)));

  /**
   * @brief event_name_for_table, generate name for the event for this table
   *
   * @param db             database of table
   * @param table_name     name of table
   * @param full           create name for event with all columns
   * @return the returned event name
   */
  static std::string event_name_for_table(const char *db,
                                          const char *table_name, bool full);

 public:
  Ndb_binlog_client(class THD *, const char *dbname, const char *tabname);
  ~Ndb_binlog_client();

  int read_and_apply_replication_info(Ndb *ndb, NDB_SHARE *share,
                                      const NdbDictionary::Table *ndbtab,
                                      uint server_id);
  int apply_replication_info(Ndb *ndb, NDB_SHARE *share,
                             const NdbDictionary::Table *ndbtab,
                             const st_conflict_fn_def *conflict_fn,
                             const st_conflict_fn_arg *args, uint num_args,
                             uint32 binlog_flags);
  bool read_replication_info(Ndb *ndb, const char *db, const char *table_name,
                             uint server_id, uint32 *binlog_flags,
                             const st_conflict_fn_def **conflict_fn,
                             struct st_conflict_fn_arg *args, uint *num_args);

  /**
   * @brief table_should_have_event, decide if a NdbEvent should be created
   * for the current table. Normally a NdbEvent is created for the table
   * unless the table will never been binlogged(like the distributed
   * privilege tables).
   *
   * NOTE! Even if the MySQL Server who creates the  event will not use
   * it, there might be several other MySQL Server(s) who will need the
   * event. Even if they could of course create the event while opening
   * the table, that's an unnecessary chance for race conditions and overload
   * to occur.
   *
   * @return true if table should have a NdbEvent
   */
  bool table_should_have_event(NDB_SHARE *share,
                               const NdbDictionary::Table *ndbtab) const;

  /**
   * @brief table_should_have_event_op, decide if a NdbEventOperation
   * should be created for the current table. Only table which need to
   * be binlogged would create such a event operation. The exception
   * is the ndb_schema table who subscribes to events for schema distribution.
   * @return  true if table should have a NdbEventOperation
   */
  bool table_should_have_event_op(const NDB_SHARE *share) const;

  /**
   * @brief event_exists_for_table, check if event already exists for this
   *        table
   *
   * @param ndb    Ndb pointer
   * @param share  NDB_SHARE pointer
   *
   * @return true if event already exists
   */
  bool event_exists_for_table(Ndb *ndb, const NDB_SHARE *share) const;

  int create_event(Ndb *ndb, const NdbDictionary::Table *ndbtab,
                   const NDB_SHARE *share);

 private:
  NdbEventOperation *create_event_op_in_NDB(Ndb *ndb,
                                            const NdbDictionary::Table *ndbtab,
                                            const std::string &event_name,
                                            const Ndb_event_data *event_data);

 public:
  int create_event_op(NDB_SHARE *share, const dd::Table *table_def,
                      const NdbDictionary::Table *ndbtab,
                      bool replace_op = false);

  /**
   * @brief drop_events_for_table, drop all binlog events for the table
   *        from NDB
   *
   * NOTE! There might be 2 different events created for binlogging the table
   * and it's not possible to know which ones have been create as that depends
   * on the settings of the MySQL Server who need them. Drop all.
   *
   * @param thd            thread context
   * @param ndb            Ndb pointer
   * @param dbname         database of table
   * @param table_name     name of table
   */
  static void drop_events_for_table(THD *thd, Ndb *ndb, const char *dbname,
                                    const char *table_name);
};

#endif
