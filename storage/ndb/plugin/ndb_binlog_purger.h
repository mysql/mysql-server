/*
   Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef NDB_BINLOG_PURGER_H
#define NDB_BINLOG_PURGER_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "storage/ndb/plugin/ndb_binlog_hooks.h"
#include "storage/ndb/plugin/ndb_component.h"

// Forward declarations
class THD;
struct SHOW_VAR;
class Ndb_local_connection;

class Ndb_binlog_purger : public Ndb_component {
  bool *m_binlogging_on;
  ulong *const m_log_purge_rate;

  THD *m_thd{nullptr};
  void create_thd(void *stackptr);

  // Name of one file to purge and the session which requested it
  struct PurgeRequest {
    std::string filename;
    void *session;
  };

  // List of purged files whose rows need to be removed
  std::vector<PurgeRequest> m_purge_files;
  // Mutex protecting the list of files to purge
  std::mutex m_purge_files_lock;
  // Condition used by purger to wait until there are new files to
  // purge, can be signaled when:
  // 1. new purged file is added
  // 2. stop is requested
  std::condition_variable m_purge_file_added_cond;
  // Condition used by client to wait until file has been removed from list.
  std::condition_variable m_purge_files_finished_cond;

  // Find min and max epoch values for given file, since the epochs
  // are known to be contiguous this gives a range to delete between.
  bool find_min_and_max_epochs(Ndb_local_connection &con,
                               const std::string &filename, uint64_t &min_epoch,
                               uint64_t &max_epoch);
  // Processing of first file has completed
  void process_purge_first_file_completed(const std::string &filename);
  // Process the first to purge file piece by piece until there are no more
  // files to purge.
  bool process_purge_first_file(Ndb_local_connection &con);
  // Process the purge file list file by file until no more files left to purge
  void process_purge_files_list();

  // Compares the files referenced in ndb_binlog_index table with the current
  // binary log files, then submits those referencing orphan files for removal.
  void find_and_delete_orphan_purged_rows();

  static constexpr time_t DELETE_SLICE_DELAY_MILLIS = 100;

  // Functionality for RESET MASTER aka. RESET BINARY LOGS AND GTIDS,
  // removes all rows from ndb_binlog_index
  Ndb_binlog_hooks m_binlog_hooks;
  static int do_after_reset_master(void *);

 public:
  Ndb_binlog_purger(bool *binlogging_on, ulong *log_purge_rate);
  Ndb_binlog_purger(const Ndb_binlog_purger &) = delete;
  virtual ~Ndb_binlog_purger() override;

  /*
    @brief Submit the name of a purged binlog file for aynchrounous removal
    of corresponding rows from the ndb_binlog_index table.

    @param session Identifies the session requesting purge. Used for being able
    to wait for purge to complete, use nullptr when there is no need to wait.
    @param filename Name of the binlog file which has been purged
  */
  void submit_purge_binlog_file(void *session, const std::string &filename);

  /*
    @brief Wait until removal of files for the given session has completed.

    @param session Identifies the session requesting purge..
  */
  void wait_purge_completed_for_session(void *session);

 private:
  virtual int do_init() override;
  virtual void do_run() override;
  virtual int do_deinit() override;
  // Wakeup for stop
  virtual void do_wakeup() override;
};

// Returns purger stats for SHOW STATUS
int show_ndb_purger_stats(THD *, SHOW_VAR *var, char *);

#endif
