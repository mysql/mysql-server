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
#include <string.h>
#include <algorithm>

#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"
#include "sql/binlog_index.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/handler.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"  // THD
#include "strmake.h"

/**
  Copy content of 'from' file from offset to 'to' file.
  - We do the copy outside of the IO_CACHE as the cache
  buffers would just make things slower and more complicated.
  In most cases the copy loop should only do one read.

  @param from    File to copy.
  @param to      File to copy to.
  @param offset  Offset in 'from' file.
  @param end_pos End position in 'from' file up to which content should be
         copied; 0 means copying till the end of file
  @retval 0 ok
  @retval -1 error
*/
static bool copy_file(IO_CACHE *from, IO_CACHE *to, my_off_t offset,
                      my_off_t end_pos = 0) {
  int bytes_read;
  uchar io_buf[IO_SIZE * 2];
  DBUG_TRACE;
  unsigned int bytes_written = 0;

  mysql_file_seek(from->file, offset, MY_SEEK_SET, MYF(0));
  while (true) {
    if ((bytes_read = (int)mysql_file_read(from->file, io_buf, sizeof(io_buf),
                                           MYF(MY_WME))) < 0)
      return true;
    if (DBUG_EVALUATE_IF("fault_injection_copy_part_file", 1, 0))
      bytes_read = bytes_read / 2;
    if (!bytes_read) break;  // end of file
    if (end_pos != 0 && (bytes_written + bytes_read > end_pos - offset)) {
      bytes_read = end_pos - offset - bytes_written;
    }
    bytes_written += bytes_read;
    if (mysql_file_write(to->file, io_buf, bytes_read, MYF(MY_WME | MY_NABP)))
      return true;
  }
  return false;
}

bool normalize_binlog_name(char *to, const char *from, bool is_relay_log) {
  DBUG_TRACE;
  bool error = false;
  char buff[FN_REFLEN];
  char *ptr = const_cast<char *>(from);
  char *opt_name = is_relay_log ? opt_relay_logname : opt_bin_logname;

  assert(from);

  /* opt_name is not null and not empty and from is a relative path */
  if (opt_name && opt_name[0] && from && !test_if_hard_path(from)) {
    // take the path from opt_name
    // take the filename from from
    char log_dirpart[FN_REFLEN], log_dirname[FN_REFLEN];
    size_t log_dirpart_len, log_dirname_len;
    dirname_part(log_dirpart, opt_name, &log_dirpart_len);
    dirname_part(log_dirname, from, &log_dirname_len);

    /* log may be empty => relay-log or log-bin did not
        hold paths, just filename pattern */
    if (log_dirpart_len > 0) {
      /* create the new path name */
      if (fn_format(buff, from + log_dirname_len, log_dirpart, "",
                    MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == nullptr) {
        error = true;
        goto end;
      }

      ptr = buff;
    }
  }

  assert(ptr);
  if (ptr) {
    size_t length = strlen(ptr);

    // Strips the CR+LF at the end of log name and \0-terminates it.
    if (length && ptr[length - 1] == '\n') {
      ptr[length - 1] = 0;
      length--;
      if (length && ptr[length - 1] == '\r') {
        ptr[length - 1] = 0;
        length--;
      }
    }
    if (!length) {
      error = true;
      goto end;
    }
    strmake(to, ptr, length);
  }
end:
  return error;
}

int compare_log_name(const char *log_1, const char *log_2) {
  const char *log_1_basename = log_1 + dirname_length(log_1);
  const char *log_2_basename = log_2 + dirname_length(log_2);

  return strcmp(log_1_basename, log_2_basename);
}

void exec_binlog_error_action_abort(const char *err_string) {
  THD *thd = current_thd;
  /*
    When the code enters here it means that there was an error at higher layer
    and my_error function could have been invoked to let the client know what
    went wrong during the execution.

    But these errors will not let the client know that the server is going to
    abort. Even if we add an additional my_error function call at this point
    client will be able to see only the first error message that was set
    during the very first invocation of my_error function call.

    The advantage of having multiple my_error function calls are visible when
    the server is up and running and user issues SHOW WARNINGS or SHOW ERROR
    calls. In this special scenario server will be immediately aborted and
    user will not be able execute the above SHOW commands.

    Hence we clear the previous errors and push one critical error message to
    clients.
   */
  if (thd) {
    if (thd->is_error()) thd->clear_error();
    /*
      Send error to both client and to the server error log.
    */
    my_error(ER_BINLOG_LOGGING_IMPOSSIBLE, MYF(ME_FATALERROR), err_string);
  }

  LogErr(ERROR_LEVEL, ER_BINLOG_LOGGING_NOT_POSSIBLE, err_string);
  flush_error_log_messages();

  if (thd) thd->send_statement_status();
  my_abort();
}

Log_info::Log_info()
    : index_file_offset(0),
      index_file_start_offset(0),
      pos(0),
      fatal(false),
      entry_index(0),
      encrypted_header_size(0),
      thread_id(0) {
  memset(log_file_name, 0, FN_REFLEN);
}

Binlog_index::Binlog_index(bool relay_log) : is_relay_log(relay_log) {
  index_file_name[0] = 0;
}

void Binlog_index::set_psi_keys(PSI_file_key key_file_log_index,
                                PSI_file_key key_file_log_index_cache) {
  m_key_file_log_index_cache = key_file_log_index_cache;
  m_key_file_log_index = key_file_log_index;
}

bool Binlog_index::open_index_file(const char *index_file_name_arg, myf opt) {
  bool error = false;
  File index_file_nr = -1;

  /*
    First open of this class instance
    Create an index file that will hold all file names used for logging.
    Add new entries to the end of it.
  */
  if (my_b_inited(&index_file)) goto end;

  fn_format(index_file_name, index_file_name_arg, mysql_data_home, ".index",
            opt);

  if (set_crash_safe_index_file_name(index_file_name_arg)) {
    error = true;
    goto end;
  }

  /*
    We need move crash_safe_index_file to index_file if the index_file
    does not exist and crash_safe_index_file exists when mysqld server
    restarts.
  */
  if (my_access(index_file_name, F_OK) &&
      !my_access(crash_safe_index_file_name, F_OK) &&
      my_rename(crash_safe_index_file_name, index_file_name, MYF(MY_WME))) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_MOVE_TMP_TO_INDEX,
           "Binlog_index::open_index_file");
    error = true;
    goto end;
  }

  if ((index_file_nr = mysql_file_open(m_key_file_log_index, index_file_name,
                                       O_RDWR | O_CREAT, MYF(MY_WME))) < 0 ||
      mysql_file_sync(index_file_nr, MYF(MY_WME)) ||
      init_io_cache_ext(&index_file, index_file_nr, IO_SIZE, READ_CACHE,
                        mysql_file_seek(index_file_nr, 0L, MY_SEEK_END, MYF(0)),
                        false, MYF(MY_WME | MY_WAIT_IF_FULL),
                        m_key_file_log_index_cache) ||
      DBUG_EVALUATE_IF("fault_injection_openning_index", 1, 0)) {
    /*
      TODO: all operations creating/deleting the index file or a log, should
      call my_sync_dir() or my_sync_dir_by_file() to be durable.
      TODO: file creation should be done with mysql_file_create()
      not mysql_file_open().
    */
    if (index_file_nr >= 0) mysql_file_close(index_file_nr, MYF(0));
    error = true;
    goto end;
  }

end:
  return error;
}

int Binlog_index::close_index_file() {
  int error = 0;

  if (my_b_inited(&index_file)) {
    end_io_cache(&index_file);
    error = mysql_file_close(index_file.file, MYF(0));
  }

  return error;
}

bool Binlog_index::is_inited_index_file() { return my_b_inited(&index_file); }

int Binlog_index::open_crash_safe_index_file() {
  int error = 0;
  File file = -1;

  DBUG_TRACE;

  if (!my_b_inited(&crash_safe_index_file)) {
    myf flags = MY_WME | MY_NABP | MY_WAIT_IF_FULL;
    if (is_relay_log) flags = flags | MY_REPORT_WAITING_IF_FULL;

    if ((file = my_open(crash_safe_index_file_name, O_RDWR | O_CREAT,
                        MYF(MY_WME))) < 0 ||
        init_io_cache(&crash_safe_index_file, file, IO_SIZE, WRITE_CACHE, 0,
                      false, flags)) {
      error = 1;
      LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_OPEN_TEMPORARY_INDEX_FILE);
    }
  }
  return error;
}

int Binlog_index::close_crash_safe_index_file() {
  int error = 0;

  DBUG_TRACE;

  if (my_b_inited(&crash_safe_index_file)) {
    end_io_cache(&crash_safe_index_file);
    error = my_close(crash_safe_index_file.file, MYF(0));
  }
  crash_safe_index_file = IO_CACHE();

  return error;
}

int Binlog_index::set_crash_safe_index_file_name(const char *base_file_name) {
  int error = 0;
  DBUG_TRACE;
  if (fn_format(crash_safe_index_file_name, base_file_name, mysql_data_home,
                ".index_crash_safe",
                MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH | MY_REPLACE_EXT)) ==
      nullptr) {
    error = 1;
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_SET_TMP_INDEX_NAME);
  }
  return error;
}

bool Binlog_index::is_inited_crash_safe_index_file() {
  return my_b_inited(&crash_safe_index_file);
}

int Binlog_index::open_purge_index_file(bool destroy) {
  int error = 0;
  File file = -1;

  DBUG_TRACE;

  if (destroy) close_purge_index_file();

  if (!my_b_inited(&purge_index_file)) {
    myf flags = MY_WME | MY_NABP | MY_WAIT_IF_FULL;
    if (is_relay_log) flags = flags | MY_REPORT_WAITING_IF_FULL;

    if ((file = my_open(purge_index_file_name, O_RDWR | O_CREAT, MYF(MY_WME))) <
            0 ||
        init_io_cache(&purge_index_file, file, IO_SIZE,
                      (destroy ? WRITE_CACHE : READ_CACHE), 0, false, flags)) {
      error = 1;
      LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_OPEN_REGISTER_FILE);
    }
  }
  return error;
}

int Binlog_index::close_purge_index_file() {
  int error = 0;

  DBUG_TRACE;

  if (my_b_inited(&purge_index_file)) {
    end_io_cache(&purge_index_file);
    error = my_close(purge_index_file.file, MYF(0));
  }
  my_delete(purge_index_file_name, MYF(0));
  new (&purge_index_file) IO_CACHE();

  return error;
}

int Binlog_index::end_close_purge_index_file() {
  int error = 0;

  DBUG_TRACE;

  if (my_b_inited(&purge_index_file)) {
    end_io_cache(&purge_index_file);
    error = my_close(purge_index_file.file, MYF(0));
  }

  return error;
}

int Binlog_index::set_purge_index_file_name(const char *base_file_name) {
  int error = 0;
  DBUG_TRACE;
  if (fn_format(
          purge_index_file_name, base_file_name, mysql_data_home, ".~rec~",
          MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH | MY_REPLACE_EXT)) == nullptr) {
    error = 1;
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_SET_PURGE_INDEX_FILE_NAME);
  }
  return error;
}

bool Binlog_index::is_inited_purge_index_file() {
  return my_b_inited(&purge_index_file);
}

int Binlog_index::reinit_purge_index_file() {
  return reinit_io_cache(&purge_index_file, READ_CACHE, 0, false, false);
}

int Binlog_index::sync_purge_index_file() {
  int error = 0;
  DBUG_TRACE;

  if ((error = flush_io_cache(&purge_index_file)) ||
      (error = my_sync(purge_index_file.file, MYF(MY_WME))))
    return error;

  return error;
}

int Binlog_index::gets_purge_index_file(char *to, size_t max_length) {
  return my_b_gets(&purge_index_file, to, max_length);
}

int Binlog_index::error_purge_index_file() { return purge_index_file.error; }

int Binlog_index::register_purge_index_entry(const char *entry) {
  int error = 0;
  DBUG_TRACE;

  if ((error = my_b_write(&purge_index_file, (const uchar *)entry,
                          strlen(entry))) ||
      (error = my_b_write(&purge_index_file, (const uchar *)"\n", 1)))
    return error;

  return error;
}

int Binlog_index::register_create_index_entry(const char *entry) {
  return register_purge_index_entry(entry);
}

int Binlog_index::find_log_pos(Log_info *linfo, const char *log_name) {
  int error = 0;
  char *full_fname = linfo->log_file_name;
  char full_log_name[FN_REFLEN], fname[FN_REFLEN];
  DBUG_TRACE;
  full_log_name[0] = full_fname[0] = 0;

  if (!my_b_inited(&index_file)) {
    error = LOG_INFO_IO;
    goto end;
  }

  // extend relative paths for log_name to be searched
  if (log_name) {
    if (normalize_binlog_name(full_log_name, log_name, is_relay_log)) {
      error = LOG_INFO_EOF;
      goto end;
    }
  }

  DBUG_PRINT("enter", ("log_name: %s, full_log_name: %s",
                       log_name ? log_name : "NULL", full_log_name));

  /* As the file is flushed, we can't get an error here */
  my_b_seek(&index_file, (my_off_t)0);

  for (;;) {
    size_t length;
    my_off_t offset = my_b_tell(&index_file);

    DBUG_EXECUTE_IF("simulate_find_log_pos_error", error = LOG_INFO_EOF;
                    break;);
    /* If we get 0 or 1 characters, this is the end of the file */
    if ((length = my_b_gets(&index_file, fname, FN_REFLEN)) <= 1) {
      /* Did not find the given entry; Return not found or error */
      error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
      break;
    }

    // extend relative paths and match against full path
    if (normalize_binlog_name(full_fname, fname, is_relay_log)) {
      error = LOG_INFO_EOF;
      break;
    }
    // if the log entry matches, null string matching anything
    if (!log_name || !compare_log_name(full_fname, full_log_name)) {
      DBUG_PRINT("info", ("Found log file entry"));
      linfo->index_file_start_offset = offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
    linfo->entry_index++;
  }

end:
  return error;
}

int Binlog_index::find_next_log(Log_info *linfo) {
  int error = 0;
  size_t length;
  char fname[FN_REFLEN];
  char *full_fname = linfo->log_file_name;

  if (!my_b_inited(&index_file)) {
    error = LOG_INFO_IO;
    goto err;
  }
  /* As the file is flushed, we can't get an error here */
  my_b_seek(&index_file, linfo->index_file_offset);

  linfo->index_file_start_offset = linfo->index_file_offset;
  if ((length = my_b_gets(&index_file, fname, FN_REFLEN)) <= 1) {
    error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }

  if (fname[0] != 0) {
    if (normalize_binlog_name(full_fname, fname, is_relay_log)) {
      error = LOG_INFO_EOF;
      goto err;
    }
    length = strlen(full_fname);
  }

  linfo->index_file_offset = my_b_tell(&index_file);

err:
  return error;
}

int Binlog_index::add_log_to_index(uchar *log_name, size_t log_name_len) {
  DBUG_TRACE;

  if (open_crash_safe_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_OPEN_TMP_INDEX,
           "Binlog_index::add_log_to_index");
    goto err;
  }

  if (copy_file(&index_file, &crash_safe_index_file, 0)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_COPY_INDEX_TO_TMP,
           "Binlog_index::add_log_to_index");
    goto err;
  }

  if (my_b_write(&crash_safe_index_file, log_name, log_name_len) ||
      my_b_write(&crash_safe_index_file, pointer_cast<const uchar *>("\n"),
                 1) ||
      flush_io_cache(&crash_safe_index_file) ||
      mysql_file_sync(crash_safe_index_file.file, MYF(MY_WME))) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_APPEND_LOG_TO_TMP_INDEX, log_name);
    goto err;
  }

  if (close_crash_safe_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_CLOSE_TMP_INDEX,
           "Binlog_index::add_log_to_index");
    goto err;
  }

  return 0;
err:
  return -1;
}

int Binlog_index::move_crash_safe_index_file_to_index_file() {
  int error = 0;
  File fd = -1;
  DBUG_TRACE;
  int failure_trials = Binlog_index::MAX_RETRIES_FOR_DELETE_RENAME_FAILURE;
  bool file_rename_status = false, file_delete_status = false;
  THD *thd = current_thd;

  if (my_b_inited(&index_file)) {
    end_io_cache(&index_file);
    if (mysql_file_close(index_file.file, MYF(0)) < 0) {
      error = -1;
      LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_CLOSE_INDEX_FILE_WHILE_REBUILDING,
             index_file_name);
      /*
        Delete Crash safe index file here and recover the binlog.index
        state(index_file io_cache) from old binlog.index content.
       */
      mysql_file_delete(key_file_binlog_index, crash_safe_index_file_name,
                        MYF(0));

      goto recoverable_err;
    }

    /*
      Sometimes an outsider can lock index files for temporary viewing
      purpose. For eg: MEB locks binlog.index/relaylog.index to view
      the content of the file. During that small period of time, deletion
      of the file is not possible on some platforms(Eg: Windows)
      Server should retry the delete operation for few times instead of
      panicking immediately.
    */
    while ((file_delete_status == false) && (failure_trials > 0)) {
      if (DBUG_EVALUATE_IF("force_index_file_delete_failure", 1, 0)) break;

      DBUG_EXECUTE_IF("simulate_index_file_delete_failure", {
        /* This simulation causes the delete to fail */
        static char first_char = index_file_name[0];
        index_file_name[0] = 0;
        sql_print_information("Retrying delete");
        if (failure_trials == 1) index_file_name[0] = first_char;
      };);
      file_delete_status = !(mysql_file_delete(key_file_binlog_index,
                                               index_file_name, MYF(MY_WME)));
      --failure_trials;
      if (!file_delete_status) {
        my_sleep(1000);
        /* Clear the error before retrying. */
        if (failure_trials > 0) thd->clear_error();
      }
    }

    if (!file_delete_status) {
      error = -1;
      LogErr(ERROR_LEVEL,
             ER_BINLOG_FAILED_TO_DELETE_INDEX_FILE_WHILE_REBUILDING,
             index_file_name);
      /*
        Delete Crash safe file index file here and recover the binlog.index
        state(index_file io_cache) from old binlog.index content.
       */
      mysql_file_delete(key_file_binlog_index, crash_safe_index_file_name,
                        MYF(0));

      goto recoverable_err;
    }
  }

  DBUG_EXECUTE_IF("crash_create_before_rename_index_file", DBUG_SUICIDE(););
  /*
    Sometimes an outsider can lock index files for temporary viewing
    purpose. For eg: MEB locks binlog.index/relaylog.index to view
    the content of the file. During that small period of time, rename
    of the file is not possible on some platforms(Eg: Windows)
    Server should retry the rename operation for few times instead of panicking
    immediately.
  */
  failure_trials = Binlog_index::MAX_RETRIES_FOR_DELETE_RENAME_FAILURE;
  while ((file_rename_status == false) && (failure_trials > 0)) {
    DBUG_EXECUTE_IF("simulate_crash_safe_index_file_rename_failure", {
      /* This simulation causes the rename to fail */
      static char first_char = index_file_name[0];
      index_file_name[0] = 0;
      sql_print_information("Retrying rename");
      if (failure_trials == 1) index_file_name[0] = first_char;
    };);
    file_rename_status =
        !(my_rename(crash_safe_index_file_name, index_file_name, MYF(MY_WME)));
    --failure_trials;
    if (!file_rename_status) {
      my_sleep(1000);
      /* Clear the error before retrying. */
      if (failure_trials > 0) thd->clear_error();
    }
  }
  if (!file_rename_status) {
    error = -1;
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_RENAME_INDEX_FILE_WHILE_REBUILDING,
           index_file_name);
    goto fatal_err;
  }
  DBUG_EXECUTE_IF("crash_create_after_rename_index_file", DBUG_SUICIDE(););

recoverable_err:
  if ((fd = mysql_file_open(key_file_binlog_index, index_file_name,
                            O_RDWR | O_CREAT, MYF(MY_WME))) < 0 ||
      mysql_file_sync(fd, MYF(MY_WME)) ||
      init_io_cache_ext(&index_file, fd, IO_SIZE, READ_CACHE,
                        mysql_file_seek(fd, 0L, MY_SEEK_END, MYF(0)), false,
                        MYF(MY_WME | MY_WAIT_IF_FULL),
                        key_file_binlog_index_cache)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_OPEN_INDEX_FILE_AFTER_REBUILDING,
           index_file_name);
    goto fatal_err;
  }

  return error;

fatal_err:
  /*
    This situation is very very rare to happen (unless there is some serious
    memory related issues like OOM) and should be treated as fatal error.
    Hence it is better to bring down the server without respecting
    'binlog_error_action' value here.
  */
  exec_binlog_error_action_abort(
      "MySQL server failed to update the "
      "binlog.index file's content properly. "
      "It might not be in sync with available "
      "binlogs and the binlog.index file state is in "
      "unrecoverable state. Aborting the server.");
  /*
    Server is aborted in the above function.
    This is dead code to make compiler happy.
   */
  return error;
}

int Binlog_index::remove_logs_outside_range_from_index(
    Log_info *start_log_info, bool need_update_threads,
    Log_info *last_log_info) {
  my_off_t end_offset = 0;
  if (open_crash_safe_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_OPEN_TMP_INDEX,
           "Binlog_index::remove_logs_outside_range_from_index");
    goto err;
  }
  if (last_log_info != nullptr) {
    end_offset = last_log_info->index_file_offset;
  }

  if (copy_file(&index_file, &crash_safe_index_file,
                start_log_info->index_file_start_offset, end_offset)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_COPY_INDEX_TO_TMP,
           "Binlog_index::remove_logs_outside_range_from_index");
    goto err;
  }

  if (close_crash_safe_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_CLOSE_TMP_INDEX,
           "Binlog_index::remove_logs_outside_range_from_index");
    goto err;
  }
  DBUG_EXECUTE_IF("fault_injection_copy_part_file", DBUG_SUICIDE(););

  if (move_crash_safe_index_file_to_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_MOVE_TMP_TO_INDEX,
           "Binlog_index::remove_logs_outside_range_from_index");
    goto err;
  }

  // now update offsets in index file for running threads
  if (need_update_threads)
    adjust_linfo_offsets(start_log_info->index_file_start_offset);
  return 0;

err:
  return LOG_INFO_IO;
}

void Binlog_index::register_log_info(Log_info *log_info) {
  DBUG_TRACE;
  log_info_set.insert(log_info);
}

void Binlog_index::unregister_log_info(Log_info *log_info) {
  DBUG_TRACE;
  log_info_set.erase(log_info);
}

int Binlog_index::log_in_use(const char *log_name) {
  DBUG_TRACE;
  int count = 0;
  int log_name_len = strlen(log_name) + 1;

  std::for_each(
      log_info_set.cbegin(), log_info_set.cend(),
      [log_name, log_name_len, &count](Log_info *log_info) {
        if (!strncmp(log_name, log_info->log_file_name, log_name_len)) {
          LogErr(WARNING_LEVEL, ER_BINLOG_FILE_BEING_READ_NOT_PURGED, log_name,
                 log_info->thread_id);
          count++;
        }
      });

  return count;
}

void Binlog_index::adjust_linfo_offsets(my_off_t purge_offset) {
  DBUG_TRACE;
  std::for_each(log_info_set.cbegin(), log_info_set.cend(),
                [purge_offset](Log_info *log_info) {
                  /*
                    Index file offset can be less that purge offset only if
                    we just started reading the index file. In that case
                    we have nothing to adjust.
                  */
                  if (log_info->index_file_offset < purge_offset)
                    log_info->fatal = (log_info->index_file_offset != 0);
                  else
                    log_info->index_file_offset -= purge_offset;
                });
}

Binlog_index_monitor::Binlog_index_monitor(bool relay_log)
    : m_binlog_index(relay_log), m_is_relay_log(relay_log) {}

void Binlog_index_monitor::set_psi_keys(PSI_mutex_key key_LOCK_index,
                                        PSI_file_key key_file_log_index,
                                        PSI_file_key key_file_log_index_cache) {
  m_key_LOCK_index = key_LOCK_index;
  m_binlog_index.set_psi_keys(key_file_log_index, key_file_log_index_cache);
}

void Binlog_index_monitor::init_pthread_objects() {
  mysql_mutex_init(m_key_LOCK_index, &m_LOCK_index, MY_MUTEX_INIT_SLOW);
}

void Binlog_index_monitor::cleanup() { mysql_mutex_destroy(&m_LOCK_index); }

bool Binlog_index_monitor::open_index_file(const char *index_file_name_arg,
                                           const char *log_name,
                                           PSI_file_key key_file_log,
                                           bool need_lock_index) {
  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  myf opt = MY_UNPACK_FILENAME;
  if (!index_file_name_arg) {
    index_file_name_arg = log_name;  // Use same basename for index file
    opt = MY_UNPACK_FILENAME | MY_REPLACE_EXT;
  }

  bool error = m_binlog_index.open_index_file(index_file_name_arg, opt);
  if (error) {
    goto end;
  }

  /*
    Sync the index by purging any binary log file that is not registered.
    In other words, either purge binary log files that were removed from
    the index but not purged from the file system due to a crash or purge
    any binary log file that was created but not register in the index
    due to a crash.
  */
  if (m_binlog_index.set_purge_index_file_name(index_file_name_arg) ||
      m_binlog_index.open_purge_index_file(false) ||
      purge_index_entry(nullptr, nullptr, key_file_log, false) ||
      m_binlog_index.close_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_recovering_index", 1, 0)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_SYNC_INDEX_FILE);
    error = true;
    goto end;
  }

end:
  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);

  return error;
}

int Binlog_index_monitor::close_index_file(bool need_lock_index) {
  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  int ret = m_binlog_index.close_index_file();

  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);

  return ret;
}

bool Binlog_index_monitor::is_inited_index_file() {
  return m_binlog_index.is_inited_index_file();
}

int Binlog_index_monitor::open_purge_index_file(bool destroy) {
  return m_binlog_index.open_purge_index_file(destroy);
}
int Binlog_index_monitor::close_purge_index_file() {
  return m_binlog_index.close_purge_index_file();
}
int Binlog_index_monitor::end_close_purge_index_file() {
  return m_binlog_index.end_close_purge_index_file();
}
int Binlog_index_monitor::set_purge_index_file_name(
    const char *base_file_name) {
  return m_binlog_index.set_purge_index_file_name(base_file_name);
}
bool Binlog_index_monitor::is_inited_purge_index_file() {
  return m_binlog_index.is_inited_purge_index_file();
}
int Binlog_index_monitor::reinit_purge_index_file() {
  return m_binlog_index.reinit_purge_index_file();
}
int Binlog_index_monitor::sync_purge_index_file() {
  return m_binlog_index.sync_purge_index_file();
}
int Binlog_index_monitor::gets_purge_index_file(char *to, size_t max_length) {
  return m_binlog_index.gets_purge_index_file(to, max_length);
}
int Binlog_index_monitor::error_purge_index_file() {
  return m_binlog_index.error_purge_index_file();
}
int Binlog_index_monitor::register_purge_index_entry(const char *entry) {
  return m_binlog_index.register_purge_index_entry(entry);
}
int Binlog_index_monitor::register_create_index_entry(const char *entry) {
  return m_binlog_index.register_create_index_entry(entry);
}

int Binlog_index_monitor::purge_index_entry(THD *thd,
                                            ulonglong *decrease_log_space,
                                            PSI_file_key key_file_log,
                                            bool need_lock_index) {
  MY_STAT s;
  int error = 0;
  Log_info log_info;
  Log_info check_log_info;

  DBUG_TRACE;

  assert(is_inited_purge_index_file());

  if ((error = reinit_purge_index_file())) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_REINIT_REGISTER_FILE);
    goto err;
  }

  for (;;) {
    size_t length;

    if ((length = gets_purge_index_file(log_info.log_file_name, FN_REFLEN)) <=
        1) {
      if ((error = error_purge_index_file())) {
        LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_READ_REGISTER_FILE, error);
        goto err;
      }

      /* Reached EOF */
      break;
    }

    /* Get rid of the trailing '\n' */
    log_info.log_file_name[length - 1] = 0;

    if (!mysql_file_stat(key_file_log, log_info.log_file_name, &s, MYF(0))) {
      if (my_errno() == ENOENT) {
        /*
          It's not fatal if we can't stat a log file that does not exist;
          If we could not stat, we won't delete.
        */
        if (thd) {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_LOG_PURGE_NO_FILE,
              ER_THD(thd, ER_LOG_PURGE_NO_FILE), log_info.log_file_name);
        }
        LogErr(INFORMATION_LEVEL, ER_CANT_STAT_FILE, log_info.log_file_name);
        set_my_errno(0);
      } else {
        /*
          Other than ENOENT are fatal
        */
        if (thd) {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_BINLOG_PURGE_FATAL_ERR,
                              "a problem with getting info on being purged %s; "
                              "consider examining correspondence "
                              "of your binlog index file "
                              "to the actual binlog files",
                              log_info.log_file_name);
        } else {
          LogErr(INFORMATION_LEVEL,
                 ER_BINLOG_CANT_DELETE_LOG_FILE_DOES_INDEX_MATCH_FILES,
                 log_info.log_file_name);
        }
        error = LOG_INFO_FATAL;
        goto err;
      }
    } else {
      if ((error = find_log_pos(&check_log_info, log_info.log_file_name,
                                need_lock_index))) {
        if (error != LOG_INFO_EOF) {
          if (thd) {
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_BINLOG_PURGE_FATAL_ERR,
                                "a problem with deleting %s and "
                                "reading the binlog index file",
                                log_info.log_file_name);
          } else {
            LogErr(INFORMATION_LEVEL,
                   ER_BINLOG_CANT_DELETE_FILE_AND_READ_BINLOG_INDEX,
                   log_info.log_file_name);
          }
          goto err;
        }

        error = 0;
        if (!m_is_relay_log)
          ha_binlog_index_purge_file(current_thd, log_info.log_file_name);

        DBUG_PRINT("info", ("purging %s", log_info.log_file_name));
        if (!mysql_file_delete(key_file_binlog, log_info.log_file_name,
                               MYF(0))) {
          DBUG_EXECUTE_IF("wait_in_purge_index_entry", {
            const char action[] =
                "now SIGNAL in_purge_index_entry WAIT_FOR go_ahead_sql";
            assert(!debug_sync_set_action(thd, STRING_WITH_LEN(action)));
            DBUG_SET("-d,wait_in_purge_index_entry");
          };);

          if (decrease_log_space) *decrease_log_space -= s.st_size;
        } else {
          if (my_errno() == ENOENT) {
            if (thd) {
              push_warning_printf(
                  thd, Sql_condition::SL_WARNING, ER_LOG_PURGE_NO_FILE,
                  ER_THD(thd, ER_LOG_PURGE_NO_FILE), log_info.log_file_name);
            }
            LogErr(INFORMATION_LEVEL, ER_BINLOG_CANT_DELETE_FILE,
                   log_info.log_file_name);
            set_my_errno(0);
          } else {
            if (thd) {
              push_warning_printf(thd, Sql_condition::SL_WARNING,
                                  ER_BINLOG_PURGE_FATAL_ERR,
                                  "a problem with deleting %s; "
                                  "consider examining correspondence "
                                  "of your binlog index file "
                                  "to the actual binlog files",
                                  log_info.log_file_name);
            } else {
              LogErr(INFORMATION_LEVEL,
                     ER_BINLOG_CANT_DELETE_LOG_FILE_DOES_INDEX_MATCH_FILES,
                     log_info.log_file_name);
            }
            if (my_errno() == EMFILE) {
              DBUG_PRINT("info", ("my_errno: %d, set ret = LOG_INFO_EMFILE",
                                  my_errno()));
              error = LOG_INFO_EMFILE;
              goto err;
            }
            error = LOG_INFO_FATAL;
            goto err;
          }
        }
      }
    }
  }

err:
  return error;
}

int Binlog_index_monitor::move_crash_safe_index_file_to_index_file(
    bool need_lock_index) {
  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  int error = m_binlog_index.move_crash_safe_index_file_to_index_file();

  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);

  return error;
}

int Binlog_index_monitor::remove_logs_outside_range_from_index(
    Log_info *start_log_info, bool need_update_threads,
    Log_info *last_log_info) {
  return m_binlog_index.remove_logs_outside_range_from_index(
      start_log_info, need_update_threads, last_log_info);
}

int Binlog_index_monitor::remove_logs_outside_range_from_index(
    const std::string &first, bool need_update_threads,
    const std::string &last) {
  Log_info first_linfo;
  Log_info last_linfo;

  MUTEX_LOCK(g, &m_LOCK_index);
  int error = find_log_pos(&first_linfo, first.c_str(), false);
  if (error) return error;
  error = find_log_pos(&last_linfo, last.c_str(), false);
  if (error) return error;
  return m_binlog_index.remove_logs_outside_range_from_index(
      &first_linfo, need_update_threads, &last_linfo);
}

void Binlog_index_monitor::register_log_info(Log_info *log_info) {
  return m_binlog_index.register_log_info(log_info);
}
void Binlog_index_monitor::unregister_log_info(Log_info *log_info) {
  return m_binlog_index.unregister_log_info(log_info);
}

int Binlog_index_monitor::add_log_to_index(uchar *log_name, size_t log_name_len,
                                           bool need_lock_index) {
  if (m_binlog_index.add_log_to_index(log_name, log_name_len)) {
    goto err;
  }

  if (move_crash_safe_index_file_to_index_file(need_lock_index)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_MOVE_TMP_TO_INDEX,
           "Binlog_index_monitor::add_log_to_index");
    goto err;
  }

  return 0;

err:
  return -1;
}

int Binlog_index_monitor::log_in_use(const char *log_name) {
  mysql_mutex_assert_owner(&m_LOCK_index);
  return m_binlog_index.log_in_use(log_name);
}

void Binlog_index_monitor::adjust_linfo_offsets(my_off_t purge_offset) {
  mysql_mutex_assert_owner(&m_LOCK_index);
  return m_binlog_index.adjust_linfo_offsets(purge_offset);
}

const char *Binlog_index_monitor::get_index_fname() const {
  return m_binlog_index.get_index_fname();
}

int Binlog_index_monitor::find_log_pos(Log_info *linfo, const char *log_name,
                                       bool need_lock_index) {
  /*
    Mutex needed because we need to make sure the file pointer does not
    move from under our feet
  */
  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  int error = m_binlog_index.find_log_pos(linfo, log_name);

  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);
  return error;
}

int Binlog_index_monitor::find_next_log(Log_info *linfo, bool need_lock_index) {
  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  int error = m_binlog_index.find_next_log(linfo);

  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);

  return error;
}

std::pair<int, std::list<std::string>> Binlog_index_monitor::get_log_index(
    bool need_lock_index) {
  DBUG_TRACE;
  Log_info log_info;

  if (need_lock_index)
    mysql_mutex_lock(&m_LOCK_index);
  else
    mysql_mutex_assert_owner(&m_LOCK_index);

  std::list<std::string> filename_list;
  int error = 0;
  for (error = find_log_pos(&log_info, nullptr, false /*need_lock_index*/);
       error == 0;
       error = find_next_log(&log_info, false /*need_lock_index*/)) {
    filename_list.push_back(std::string(log_info.log_file_name));
  }

  if (need_lock_index) mysql_mutex_unlock(&m_LOCK_index);

  return std::make_pair(error, filename_list);
}

std::pair<std::list<std::string>, mysql::utils::Error>
Binlog_index_monitor::get_filename_list() {
  std::pair<std::list<std::string>, mysql::utils::Error> result;
  auto &[filename_list, internal_error] = result;
  Log_info linfo;
  int error = 0;
  std::string error_message;
  MUTEX_LOCK(g, &m_LOCK_index);
  try {
    for (error = find_log_pos(&linfo, nullptr, false); !error;
         error = find_next_log(&linfo, false)) {
      filename_list.push_back(std::string(linfo.log_file_name));
    }
  } catch (std::bad_alloc &) {
    internal_error = mysql::utils::Error("Binlog_index_monitor", __FILE__,
                                         __LINE__, "Out of memory");
  }
  if (error != LOG_INFO_EOF) {
    internal_error =
        mysql::utils::Error("Binlog_index_monitor", __FILE__, __LINE__,
                            "Error while reading index file");
  }
  return result;
}

int Binlog_index_monitor::find_next_relay_log(char log_name[FN_REFLEN + 1]) {
  Log_info info;
  int error;
  char relative_path_name[FN_REFLEN + 1];

  if (fn_format(relative_path_name, log_name + dirname_length(log_name),
                mysql_data_home, "", 0) == nullptr)
    return 1;

  mysql_mutex_lock(&m_LOCK_index);

  error = find_log_pos(&info, relative_path_name, false);
  if (error == 0) {
    error = find_next_log(&info, false);
    if (error == 0) strcpy(log_name, info.log_file_name);
  }

  mysql_mutex_unlock(&m_LOCK_index);
  return error;
}
