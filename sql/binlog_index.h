/* Copyright (c) 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#ifndef BINLOG_INDEX_H_INCLUDED
#define BINLOG_INDEX_H_INCLUDED

#include <set>

#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/components/services/bits/psi_file_bits.h"
#include "mysql/utils/error.h"

class THD;

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8
#define LOG_INFO_EMFILE -9
#define LOG_INFO_BACKUP_LOCK -10
#define LOG_INFO_NOT_IN_USE -11

/**
  Turns a relative log binary log path into a full path, based on the
  opt_bin_logname or opt_relay_logname. Also trims the cr-lf at the
  end of the full_path before return to avoid any server startup
  problem on windows.

  @param from         The log name we want to make into an absolute path.
  @param to           The buffer where to put the results of the
                      normalization.
  @param is_relay_log Switch that makes is used inside to choose which
                      option (opt_bin_logname or opt_relay_logname) to
                      use when calculating the base path.

  @returns true if a problem occurs, false otherwise.
 */
bool normalize_binlog_name(char *to, const char *from, bool is_relay_log);

/**
  Compare log file basenames, i.e. without their directory names

  @return It returns an integer less than, equal to, or greater than zero
          if log_1 is found, respectively, to be less than, to match,
          or be greater than log_2
 */
int compare_log_name(const char *log_1, const char *log_2);

/**
  When a fatal error occurs due to which binary logging becomes impossible and
  the user specified binlog_error_action= ABORT_SERVER the following function is
  invoked. This function pushes the appropriate error message to client and logs
  the same to server error log and then aborts the server.

  @param err_string Error string which specifies the exact error
                    message from the caller.
*/
void exec_binlog_error_action_abort(const char *err_string);

struct Log_info {
  Log_info();

  char log_file_name[FN_REFLEN] = {0};
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal;       // if the purge happens to give us a negative offset
  int entry_index;  // used in purge_logs(), calculatd in find_log_pos().
  int encrypted_header_size;
  my_thread_id thread_id;
};

/**
  Binlog_index defines methods which handle binlog index file and its entries.
  @see Binlog_index_monitor to synchronize access to Binlog_index object.
*/
class Binlog_index {
 public:
  Binlog_index(bool relay_log);

  void set_psi_keys(PSI_file_key key_file_log_index,
                    PSI_file_key key_file_log_index_cache);

  /**
    @brief Create an index file that will hold all file names used for logging.
    @param index_file_name_arg name of the index file
    @param opt opening mode
    @return false - success, true - failure
  */
  bool open_index_file(const char *index_file_name_arg, myf opt);

  /**
     @brief Close index file
     @return 0 - success, !0 - failure
  */
  int close_index_file();

  /**
    @brief Check if index file is initalized
    @return true - initalized, false - otherwise
  */
  bool is_inited_index_file();

  /**
    @brief Functions to manage purge index file
    @see purge_index_file
  */
  int open_purge_index_file(bool destroy);
  int close_purge_index_file();
  /**
    @brief simulate failure using fault_injection_registering_index
           debug symbol
  */
  int end_close_purge_index_file();
  int set_purge_index_file_name(const char *base_file_name);
  bool is_inited_purge_index_file();
  int reinit_purge_index_file();
  int sync_purge_index_file();
  /**
    @brief Read purge index file name into to buffer of max_length.
    @param to buffer where to store purge index file name
    @param max_length length of the to buffer
    @return number of characters read, 0 on error
  */
  int gets_purge_index_file(char *to, size_t max_length);
  int error_purge_index_file();
  int register_purge_index_entry(const char *entry);
  int register_create_index_entry(const char *entry);

  /**
    Find the position in the log-index-file for the given log name.

    @param[out] linfo The found log file name will be stored here, along
    with the byte offset of the next log file name in the index file.
    @param log_name Filename to find in the index file, or NULL if we
    want to read the first entry.

    @note
      On systems without the truncate function the file will end with one or
      more empty lines.  These will be ignored when reading the file.

    @retval 0 ok
    @retval LOG_INFO_EOF End of log-index-file found
    @retval LOG_INFO_IO Got IO error while reading file
  */
  int find_log_pos(Log_info *linfo, const char *log_name);

  /**
    Find the position in the log-index-file for the given log name.

    @param[out] linfo The filename will be stored here, along with the
    byte offset of the next filename in the index file.

    @note
      - Before calling this function, one has to call find_log_pos()
      to set up 'linfo'

    @retval 0 ok
    @retval LOG_INFO_EOF End of log-index-file found
    @retval LOG_INFO_IO Got IO error while reading file
  */
  int find_next_log(Log_info *linfo);

  /**
    Append log file name to index file.

    - To make crash safe, we copy all the content of index file
    to crash safe index file firstly and then append the log
    file name to the crash safe index file. Finally move the
    crash safe index file to index file.

    @param log_name Log file name
    @param log_name_len Length of log file name

    @retval
      0   ok
    @retval
      -1   error
  */
  int add_log_to_index(uchar *log_name, size_t log_name_len);

  /**
    Move crash safe index file to index file.

    @retval 0 ok
    @retval -1 error
  */
  int move_crash_safe_index_file_to_index_file();

  /**
    @brief Remove logs from index file, except files between 'start' and
   'last'
    @details To make it crash safe, we copy the content of the index file
    from index_file_start_offset recorded in log_info to a
    crash safe index file first and then move the crash
    safe index file to the index file.
    @param start_log_info      Metadata of the first log to be kept
                               in the index file.
    @param need_update_threads If we want to update the log coordinates
                               of all threads. False for relay logs,
                               true otherwise.
    @param last_log_info       Metadata of the last log to be kept in the index
                               file; nullptr means that all logs after
                               start_log_info will be kept
    @retval 0 ok
    @retval LOG_INFO_IO Got IO error while reading/writing file
  */
  int remove_logs_outside_range_from_index(Log_info *start_log_info,
                                           bool need_update_threads,
                                           Log_info *last_log_info = nullptr);

  /**
    @brief Register log_info which is used by log_in_use
           and adjust_linfo_offsets functions.
    @param log_info - log_info to be registered
   */
  void register_log_info(Log_info *log_info);

  /**
    @brief Unregister log_info
    @param log_info - log_info to be unregistered
    @see register_log_info
   */
  void unregister_log_info(Log_info *log_info);

  /**
    @brief Return number of logs in use
    @param log_name log_name to be searched among files registered
           by register_log_info
    @return number of logs in use
   */
  int log_in_use(const char *log_name);

  /**
    @brief Adjust all registered log_infos by purge_offset
    @param purge_offset offset by which log_info are offset
   */
  void adjust_linfo_offsets(my_off_t purge_offset);

  const char *get_index_fname() const { return index_file_name; }
  inline IO_CACHE *get_index_file() { return &index_file; }

 private:
  /**
    Open a (new) crash safe index file.

    @note The crash safe index file is a special file
    used for guaranteeing index file crash safe.
    @retval 0 ok
    @retval 1 error
  */
  int open_crash_safe_index_file();

  /**
    Close the crash safe index file.

    @note The crash safe file is just closed, is not deleted.
    Because it is moved to index file later on.
    @retval 0 ok
    @retval 1 error
  */
  int close_crash_safe_index_file();

  /**
    Set the name of crash safe index file.

    @retval 0 ok
    @retval 1 error
  */
  int set_crash_safe_index_file_name(const char *base_file_name);

  /**
    @brief Check if crash safe index is initalized
    @return true - is initalized, false - otherwise
   */
  bool is_inited_crash_safe_index_file();

  /** The instrumentation key to use for opening the log index file. */
  PSI_file_key m_key_file_log_index;
  /** The instrumentation key to use for opening a log index cache file. */
  PSI_file_key m_key_file_log_index_cache;

  IO_CACHE index_file;
  char index_file_name[FN_REFLEN];
  /**
    crash_safe_index_file is temp file used for guaranteeing
    index file crash safe when master server restarts.
  */
  IO_CACHE crash_safe_index_file;
  char crash_safe_index_file_name[FN_REFLEN];
  /**
    purge_file is a temp file used in purge_logs so that the index file
    can be updated before deleting files from disk, yielding better crash
    recovery. It is created on demand the first time purge_logs is called
    and then reused for subsequent calls. It is cleaned up in cleanup().
  */
  IO_CACHE purge_index_file;
  char purge_index_file_name[FN_REFLEN];

  const bool is_relay_log;
  /**
     Set of log info objects that are in usage and might prevent some other
     operations from executing.
  */
  std::set<Log_info *> log_info_set;

  static const int MAX_RETRIES_FOR_DELETE_RENAME_FAILURE = 5;
};

/**
  Binlog_index_monitor synchronizes access to Binlog_index object.
  Methods defined by Binlog_index are exposed through Binlog_index_monitor
  class.

  Please keep in mind that LOCK_index is exposed and its lock
  and unlock methods need to be called with caution.
*/
class Binlog_index_monitor {
 public:
  Binlog_index_monitor(bool relay_log);

  void set_psi_keys(PSI_mutex_key key_LOCK_index,
                    PSI_file_key key_file_log_index,
                    PSI_file_key key_file_log_index_cache);

  void init_pthread_objects();
  void cleanup();

  /**
    @see Binlog_index::open_index_file
  */
  bool open_index_file(const char *index_file_name_arg, const char *log_name,
                       PSI_file_key key_file_log, bool need_lock_index);

  /**
    @see Binlog_index::close_index_file
  */
  int close_index_file(bool need_lock_index);

  /**
    @see Binlog_index::is_inited_index_file
  */
  bool is_inited_index_file();

  /**
    @see Binlog_index::open_purge_index_file
  */
  int open_purge_index_file(bool destroy);
  int close_purge_index_file();
  int end_close_purge_index_file();
  int set_purge_index_file_name(const char *base_file_name);
  bool is_inited_purge_index_file();
  int reinit_purge_index_file();
  int sync_purge_index_file();
  /**
    @see Binlog_index::gets_purge_index_file
  */
  int gets_purge_index_file(char *to, size_t max_length);
  int error_purge_index_file();
  int register_purge_index_entry(const char *entry);
  int register_create_index_entry(const char *entry);
  int purge_index_entry(THD *thd, ulonglong *decrease_log_space,
                        PSI_file_key key_file_log, bool need_lock_index);

  /**
    Find the position in the log-index-file for the given log name.

    @param[out] linfo The found log file name will be stored here, along
    with the byte offset of the next log file name in the index file.
    @param log_name Filename to find in the index file, or NULL if we
    want to read the first entry.
    @param need_lock_index If false, this function acquires LOCK_index;
    otherwise the lock should already be held by the caller.

    @note
      On systems without the truncate function the file will end with one or
      more empty lines.  These will be ignored when reading the file.

    @retval
      0			ok
    @retval
      LOG_INFO_EOF	        End of log-index-file found
    @retval
      LOG_INFO_IO		Got IO error while reading file

    @see Binlog_index::find_log_pos
  */
  int find_log_pos(Log_info *linfo, const char *log_name, bool need_lock_index);

  /**
    Find the position in the log-index-file for the given log name.

    @param[out] linfo The filename will be stored here, along with the
    byte offset of the next filename in the index file.
    @param need_lock_index If true, LOCK_index will be acquired;
    otherwise it should already be held by the caller.

    @note
      - Before calling this function, one has to call find_log_pos()
      to set up 'linfo'
      - Mutex needed because we need to make sure the file pointer does not move
      from under our feet

    @retval 0 ok
    @retval LOG_INFO_EOF End of log-index-file found
    @retval LOG_INFO_IO Got IO error while reading file

    @see Binlog_index::find_next_log
  */
  int find_next_log(Log_info *linfo, bool need_lock_index);

  /**
    Append log file name to index file.

    - To make crash safe, we copy all the content of index file
    to crash safe index file firstly and then append the log
    file name to the crash safe index file. Finally move the
    crash safe index file to index file.

    @param log_name Log file name
    @param log_name_len Length of log file name
    @param need_lock_index If true, LOCK_index will be acquired;
    otherwise it should already be held by the caller.

    @retval
      0   ok
    @retval
      -1   error

    @see Binlog_index::add_log_to_index
  */
  int add_log_to_index(uchar *log_name, size_t log_name_len,
                       bool need_lock_index);

  /**
    Move crash safe index file to index file.

    @param need_lock_index If true, LOCK_index will be acquired;
    otherwise it should already be held.

    @retval 0 ok
    @retval -1 error

    @see Binlog_index::move_crash_safe_index_file_to_index_file
  */
  int move_crash_safe_index_file_to_index_file(bool need_lock_index);

  /**
    @see Binlog_index::remove_logs_outside_range_from_index
  */
  int remove_logs_outside_range_from_index(Log_info *start_log_info,
                                           bool need_update_threads,
                                           Log_info *last_log_info = nullptr);

  /**
    @brief Remove logs from index file except logs between first and last
    @param first Filename of the first relay log to be kept in index file
    @param need_update_threads If we want to update the log coordinates
                               of all threads. False for relay logs,
                               true otherwise
    @param last Filename of the last relay log to be kept in index file

    @retval 0 OK
    @retval LOG_INFO_IO    Got IO error while reading/writing file
    @retval LOG_INFO_EOF   Could not find requested log file (first or last)
  */
  int remove_logs_outside_range_from_index(const std::string &first,
                                           bool need_update_threads,
                                           const std::string &last);

  /**
    @see Binlog_index::register_log_info
  */
  void register_log_info(Log_info *log_info);

  /**
    @see Binlog_index::unregister_log_info
  */
  void unregister_log_info(Log_info *log_info);

  /**
    Check if any threads use log name.
    @note This method expects the LOCK_index to be taken so there are no
    concurrent edits against linfo objects being iterated
    @param log_name name of a log which is checked for usage

  */
  int log_in_use(const char *log_name);

  /**
    @see Binlog_index::adjust_linfo_offsets
  */
  void adjust_linfo_offsets(my_off_t purge_offset);

  const char *get_index_fname() const;
  inline IO_CACHE *get_index_file() { return m_binlog_index.get_index_file(); }

  /**
    Retrieves the contents of the index file associated with this log object
    into an `std::list<std::string>` object. The order held by the index file is
    kept.

    @param need_lock_index whether or not the lock over the index file should be
                          acquired inside the function.

    @return a pair: a function status code; a list of `std::string` objects with
            the content of the log index file.
  */
  std::pair<int, std::list<std::string>> get_log_index(
      bool need_lock_index = true);

  /**
    @brief Obtains the list of logs from the index file
    @return List of log filenames
   */
  std::pair<std::list<std::string>, mysql::utils::Error> get_filename_list();

  /**
    Find the relay log name following the given name from relay log index file.
    @param[in,out] log_name  The name is full path name.
    @return return 0 if it finds next relay log. Otherwise return the error
    code.
  */
  int find_next_relay_log(char log_name[FN_REFLEN + 1]);

  inline Binlog_index &get_index() { return m_binlog_index; }

  inline mysql_mutex_t *get_index_lock() { return &m_LOCK_index; }
  inline void lock() { mysql_mutex_lock(&m_LOCK_index); }
  inline void unlock() { mysql_mutex_unlock(&m_LOCK_index); }
  inline void assert_owner() { mysql_mutex_assert_owner(&m_LOCK_index); }

 private:
  /** The instrumentation key to use for @ LOCK_index. */
  PSI_mutex_key m_key_LOCK_index;

  /** POSIX thread objects are inited by init_pthread_objects() */
  mysql_mutex_t m_LOCK_index;
  Binlog_index m_binlog_index;
  const bool m_is_relay_log;
};

#endif /* BINLOG_INDEX_H_INCLUDED */
