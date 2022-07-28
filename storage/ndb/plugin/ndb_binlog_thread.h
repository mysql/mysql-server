/*
   Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_BINLOG_THREAD_H
#define NDB_BINLOG_THREAD_H

#include <mutex>
#include <string>
#include <vector>

#include "storage/ndb/plugin/ndb_binlog_hooks.h"
#include "storage/ndb/plugin/ndb_component.h"
#include "storage/ndb/plugin/ndb_metadata_sync.h"

class Ndb;
class NdbEventOperation;
class Ndb_sync_pending_objects_table;
class Ndb_sync_excluded_objects_table;
struct ndb_binlog_index_row;
class injector;
class injector_transaction;
struct TABLE;
union NdbValue;
struct MY_BITMAP;
class Ndb_blobs_buffer;
struct NDB_SHARE;

class Ndb_binlog_thread : public Ndb_component {
  Ndb_binlog_hooks binlog_hooks;
  static int do_after_reset_master(void *);
  Ndb_metadata_sync metadata_sync;

  // Holds reference to share for ndb_apply_status table
  NDB_SHARE *m_apply_status_share{nullptr};
  bool acquire_apply_status_reference();
  void release_apply_status_reference();

 public:
  Ndb_binlog_thread();
  ~Ndb_binlog_thread() override;

  /*
    @brief Check if purge of the specified binlog file can be handled
    by the binlog thread.

    @param filename Name of the binlog file which has been purged

    @return true the binlog thread will handle the purge
    @return false the binlog thread will not handle the purge
  */
  bool handle_purge(const char *filename);

  /*
    @brief Iterate through the excluded objects and check if the mismatches
           are still present or if the user has manually synchronized the
           objects

    @param thd  Thread handle

    @return void
  */
  void validate_sync_excluded_objects(THD *thd);

  /*
    @brief Clear the list of objects excluded from sync

    @return void
  */
  void clear_sync_excluded_objects();

  /*
    @brief Clear the list of objects whose sync has been retried

    @return void
  */
  void clear_sync_retry_objects();

  /*
    @brief Pass the logfile group object detected to the internal implementation
           that shall eventually synchronize the object

    @param logfile_group_name  Name of the logfile group

    @return true on success, false on failure
  */
  bool add_logfile_group_to_check(const std::string &logfile_group_name);

  /*
    @brief Pass the tablespace object detected to the internal implementation
           that shall eventually synchronize the object

    @param tablespace_name  Name of the tablespace

    @return true on success, false on failure
  */
  bool add_tablespace_to_check(const std::string &tablespace_name);

  /*
    @brief Pass the schema object detected to the internal implementation that
           shall eventually synchronize the object

    @param schema_name  Name of the schema

    @return true on success, false on failure
  */
  bool add_schema_to_check(const std::string &schema_name);

  /*
    @brief Pass the table object detected to the internal implementation that
           shall eventually synchronize the object

    @param db_name     Name of the database that the table belongs to
    @param table_name  Name of the table

    @return true on success, false on failure
  */
  bool add_table_to_check(const std::string &db_name,
                          const std::string &table_name);

  /*
    @brief Retrieve information about objects currently excluded from sync

    @param excluded_table  Pointer to excluded objects table object

    @return void
  */
  void retrieve_sync_excluded_objects(
      Ndb_sync_excluded_objects_table *excluded_table);

  /*
    @brief Get the count of objects currently excluded from sync

    @return number of excluded objects
  */
  unsigned int get_sync_excluded_objects_count();

  /*
    @brief Retrieve information about objects currently awaiting sync

    @param pending_table  Pointer to pending objects table object

    @return void
  */
  void retrieve_sync_pending_objects(
      Ndb_sync_pending_objects_table *pending_table);

  /*
    @brief Get the count of objects currently awaiting sync

    @return number pending objects
  */
  unsigned int get_sync_pending_objects_count();

 private:
  int do_init() override;
  void do_run() override;
  int do_deinit() override;
  // Wake up for stop
  void do_wakeup() override;

  /**
    @brief Log an error from NDB to the log.

    @param      ndberr The NDB error to log
  */
  void log_ndb_error(const NdbError &ndberr) const;

  /*
     The Ndb_binlog_thread is supposed to make a continuous recording
     of the activity in the cluster to the mysqlds binlog. When this
     recording is interrupted an incident event(aka. GAP event) is
     written to the binlog thus allowing consumers of the binlog to
     notice that the recording is most likely not continuous.
  */
  enum Reconnect_type {
    // Incident occurred because the mysqld was stopped and
    // is now starting up again
    MYSQLD_STARTUP,
    // Incident occurred because the mysqld was disconnected
    // from the cluster
    CLUSTER_DISCONNECT
  };
  bool check_reconnect_incident(THD *thd, injector *inj,
                                Reconnect_type incident_id) const;

  /**
    @brief Perform any purge requests which has been queued up earlier.

    @param thd Thread handle
  */
  void recall_pending_purges(THD *thd);
  std::mutex m_purge_mutex;                   // Protects m_pending_purges
  std::vector<std::string> m_pending_purges;  // List of pending purges

  /**
     @brief Remove event operations belonging to one Ndb object

     @param ndb The Ndb object to remove event operations from
  */
  static void remove_event_operations(Ndb *ndb);

  /**
     @brief Remove event operations belonging to the different Ndb objects
     (owned by the binlog thread)

     @param s_ndb The schema Ndb object to remove event operations from
     @param i_ndb The injector Ndb object to remove event operations from
  */
  static void remove_all_event_operations(Ndb *s_ndb, Ndb *i_ndb);

  /**
     @brief Synchronize the object that is currently at the front of the queue
     of objects detected for automatic synchronization

     @param thd Thread handle
  */
  void synchronize_detected_object(THD *thd);

#ifndef NDEBUG
  /**
     @brief As the Binlog thread is not a client thread, the 'set debug'
     command does not affect it. This functions updates the thread-local
     debug value from the global debug value.

     @note function need to be called regularly in the binlog thread loop.
   */
  void dbug_sync_setting() const;
#endif

  // Functions for handling received events
  int handle_data_get_blobs(const TABLE *table,
                            const NdbValue *const value_array,
                            Ndb_blobs_buffer &buffer, ptrdiff_t ptrdiff) const;
  void handle_data_unpack_record(TABLE *table, const NdbValue *value,
                                 MY_BITMAP *defined, uchar *buf) const;
  int handle_error(NdbEventOperation *pOp) const;
  void handle_non_data_event(THD *thd, NdbEventOperation *pOp,
                             ndb_binlog_index_row &row);
  int handle_data_event(const NdbEventOperation *pOp,
                        ndb_binlog_index_row **rows,
                        injector_transaction &trans, unsigned &trans_row_count,
                        unsigned &replicated_row_count) const;
  bool handle_events_for_epoch(THD *thd, injector *inj, Ndb *i_ndb,
                               NdbEventOperation *&i_pOp,
                               const Uint64 current_epoch);

  // Functions for injecting events
  bool inject_apply_status_write(injector_transaction &trans,
                                 ulonglong gci) const;
  void inject_incident(injector *inj, THD *thd,
                       NdbDictionary::Event::TableEvent event_type,
                       Uint64 gap_epoch) const;
  void inject_table_map(injector_transaction &trans, Ndb *ndb) const;
  void commit_trans(injector_transaction &trans, THD *thd, Uint64 current_epoch,
                    ndb_binlog_index_row *rows, unsigned trans_row_count,
                    unsigned replicated_row_count) const;
};

/*
  Called as part of SHOW STATUS or performance_schema queries. Returns
  information about the number of NDB metadata objects synched
*/
int show_ndb_metadata_synced(THD *, SHOW_VAR *var, char *);

#endif
