/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "sql/ndb_component.h"

// Forward declarations
class THD;
class Thd_ndb;
struct NdbError;
struct mysql_mutex_t;
struct mysql_cond_t;
struct SHOW_VAR;

class Ndb_metadata_change_monitor : public Ndb_component {
  mysql_mutex_t m_wait_mutex;
  mysql_cond_t m_wait_cond;

 public:
  Ndb_metadata_change_monitor();
  Ndb_metadata_change_monitor(const Ndb_metadata_change_monitor &) = delete;
  virtual ~Ndb_metadata_change_monitor();

  /*
    @brief Signal that the check interval has been changed by the user

    @param new_check_interval  New check interval value specified

    @return void
  */
  void set_check_interval(unsigned long new_check_interval);

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
  bool detect_logfile_group_changes(THD *thd, const Thd_ndb *thd_ndb);

  /*
    @brief Detect any differences between the tablespaces stored in DD and
           those in NDB Dictionary

    @param  thd      Thread handle
    @param  thd_ndb  Handle for ndbcluster specific thread data

    @return true if changes detected, false if not
  */
  bool detect_tablespace_changes(THD *thd, const Thd_ndb *thd_ndb);

  /*
    @brief Detect any differences between the tables stored in DD and those in
           NDB Dictionary

    @param  thd      Thread handle
    @param  thd_ndb  Handle for ndbcluster specific thread data

    @return true if changes detected, false if not
  */
  bool detect_table_changes(THD *thd, const Thd_ndb *thd_ndb);

  /*
    @brief Detect any differences between the tables belonging to a particular
           schema stored in DD and those in NDB Dictionary

    @param  thd          Thread handle
    @param  thd_ndb      Handle for ndbcluster specific thread data
    @param  schema_name  Name of the schema

    @return true if changes detected, false if not
  */
  bool detect_changes_in_schema(THD *thd, const Thd_ndb *thd_ndb,
                                const std::string &schema_name);
};

/*
  Called as part of SHOW STATUS or performance_schema queries. Returns
  information about the number of NDB metadata objects detected
*/
int show_ndb_metadata_check(THD *, SHOW_VAR *var, char *);

#endif
