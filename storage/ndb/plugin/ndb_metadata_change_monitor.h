/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_METADATA_CHANGE_MONITOR_H
#define NDB_METADATA_CHANGE_MONITOR_H

#include <string>
#include <vector>

#include "storage/ndb/plugin/ndb_component.h"

// Forward declarations
class THD;
class Thd_ndb;
struct NdbError;
struct mysql_mutex_t;
struct mysql_cond_t;
struct SHOW_VAR;

class Ndb_metadata_change_monitor : public Ndb_component {
  mysql_mutex_t m_wait_mutex;  // protects m_wait_cond
  mysql_cond_t m_wait_cond;
  static mysql_mutex_t m_sync_done_mutex;  // protects m_sync_done_cond
  static mysql_cond_t m_sync_done_cond;
  bool m_mark_sync_complete;

 public:
  Ndb_metadata_change_monitor();
  Ndb_metadata_change_monitor(const Ndb_metadata_change_monitor &) = delete;
  virtual ~Ndb_metadata_change_monitor() override;

  /*
    @brief Signal that the check interval has been changed by the user

    @param new_check_interval  New check interval value specified

    @return void
  */
  void set_check_interval(unsigned long new_check_interval);

  /*
    @brief Signal that the ndb_metadata_sync option has been set

    @return void
  */
  void signal_metadata_sync_enabled();

  /*
    @brief Inform the thread that the all metadata changes detected have been
           synchronized by the binlog thread. Signal is sent only when the
           ndb_metadata_sync option has been set

    @return void
  */
  static void sync_done();

 private:
  virtual int do_init() override;
  virtual void do_run() override;
  virtual int do_deinit() override;
  // Wakeup for stop
  virtual void do_wakeup() override;

  /*
    @brief Log error returned by the NDB sub-system

    @param ndb_error  NdbError object containing error code and message

    @return void
  */
  void log_NDB_error(const NdbError &ndb_error) const;

  /*
    @brief Detect any differences between the logfile groups stored in DD and
           those in NDB Dictionary

    @param  thd      Thread handle
    @param  thd_ndb  Handle for ndbcluster specific thread data

    @return true if changes detected, false if not
  */
  bool detect_logfile_group_changes(THD *thd, const Thd_ndb *thd_ndb) const;

  /*
    @brief Detect any differences between the tablespaces stored in DD and
           those in NDB Dictionary

    @param  thd      Thread handle
    @param  thd_ndb  Handle for ndbcluster specific thread data

    @return true if changes detected, false if not
  */
  bool detect_tablespace_changes(THD *thd, const Thd_ndb *thd_ndb) const;

  /*
    @brief Detect schemata which are used in NDB Dictionary but do not exist
           in DD. Unlike other objects, only this particular scenario is of
           interest since schemata may contain tables of other storage engines.
           Thus, the auto sync mechanism shall only create the schema in DD in
           the above scenario and never remove a schema object from the DD

    @param  thd_ndb          Handle for ndbcluster specific thread data
    @param  dd_schema_names  Schema names retrieved from the DD

    @return true if changes are successfully detected, false if not
  */
  bool detect_schema_changes(const Thd_ndb *thd_ndb,
                             std::vector<std::string> *dd_schema_names) const;

  /*
    @brief Detect any differences between the schemata and tables stored in DD
           and those in NDB Dictionary

    @param  thd      Thread handle
    @param  thd_ndb  Handle for ndbcluster specific thread data

    @return true if changes detected, false if not
  */
  bool detect_schema_and_table_changes(THD *thd, const Thd_ndb *thd_ndb);

  /*
    @brief Detect any differences between the tables belonging to a particular
           schema stored in DD and those in NDB Dictionary

    @param  thd          Thread handle
    @param  thd_ndb      Handle for ndbcluster specific thread data
    @param  schema_name  Name of the schema

    @return true if changes detected, false if not
  */
  bool detect_table_changes_in_schema(THD *thd, const Thd_ndb *thd_ndb,
                                      const std::string &schema_name) const;
};

/*
  Called as part of SHOW STATUS or performance_schema queries. Returns
  information about the number of NDB metadata objects detected
*/
int show_ndb_metadata_check(THD *, SHOW_VAR *var, char *);

#endif
