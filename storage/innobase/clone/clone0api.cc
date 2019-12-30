/*****************************************************************************

Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file clone/clone0api.cc
 Innodb Clone Interface

 *******************************************************/
#include <cstdio>
#include <fstream>
#include <iostream>

#include "clone0api.h"
#include "clone0clone.h"
#include "os0thread-create.h"

#include "sql/clone_handler.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/sql_prepare.h"
#include "sql/sql_table.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/strfunc.h"

#include "dict0dd.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dictionary.h"
#include "sql/dd/impl/dictionary_impl.h"  // dd::dd_tablespace_id()
#include "sql/dd/impl/sdi.h"
#include "sql/dd/impl/utils.h"
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"
#include "sql/rpl_msr.h"  // is_slave_configured()

/** Check if clone status file exists.
@param[in]	file_name	file name
@return true if file exists. */
static bool file_exists(std::string &file_name) {
  std::ifstream file(file_name.c_str());

  if (file.is_open()) {
    file.close();
    return (true);
  }
  return (false);
}

/** Rename clone status file. The operation is expected to be atomic
when the files belong to same directory.
@param[in]	from_file	name of current file
@param[in]	to_file		name of new file */
static void rename_file(std::string &from_file, std::string &to_file) {
  auto ret = std::rename(from_file.c_str(), to_file.c_str());

  if (ret != 0) {
    ib::fatal(ER_IB_CLONE_STATUS_FILE)
        << "Error renaming file from: " << from_file.c_str()
        << " to: " << to_file.c_str();
  }
}

/** Create clone status file.
@param[in]	file_name	file name */
static void create_file(std::string &file_name) {
  std::ofstream file(file_name.c_str());

  if (file.is_open()) {
    file.close();
    return;
  }
  ib::error(ER_IB_CLONE_STATUS_FILE)
      << "Error creating file : " << file_name.c_str();
}

/** Delete clone status file.
@param[in]	file	name of file */
static void remove_file(std::string &file) {
  auto ret = std::remove(file.c_str());

  if (ret != 0) {
    ib::error(ER_IB_CLONE_STATUS_FILE)
        << "Error removing file : " << file.c_str();
  }
}

/** Create clone in progress file and error file.
@param[in]	clone	clone handle */
static void create_status_file(const Clone_Handle *clone) {
  const char *path = clone->get_datadir();
  std::string file_name;

  if (clone->replace_datadir()) {
    /* Create error file for rollback. */
    file_name.assign(CLONE_INNODB_ERROR_FILE);
    create_file(file_name);
    return;
  }

  file_name.assign(path);
  /* Add path separator if needed. */
  if (file_name.back() != OS_PATH_SEPARATOR) {
    file_name.append(OS_PATH_SEPARATOR_STR);
  }
  file_name.append(CLONE_INNODB_IN_PROGRESS_FILE);

  create_file(file_name);
}

/** Drop clone in progress file and error file.
@param[in]	clone	clone handle */
static void drop_status_file(const Clone_Handle *clone) {
  const char *path = clone->get_datadir();
  std::string file_name;

  if (clone->replace_datadir()) {
    /* Indicate that clone needs table fix up on recovery. */
    file_name.assign(CLONE_INNODB_FIXUP_FILE);
    create_file(file_name);

    /* drop error file on success. */
    file_name.assign(CLONE_INNODB_ERROR_FILE);
    remove_file(file_name);

    DBUG_EXECUTE_IF("clone_recovery_crash_point", {
      file_name.assign(CLONE_INNODB_RECOVERY_CRASH_POINT);
      create_file(file_name);
    });
    return;
  }

  std::string path_name(path);
  /* Add path separator if needed. */
  if (path_name.back() != OS_PATH_SEPARATOR) {
    path_name.append(OS_PATH_SEPARATOR_STR);
  }

  /* Indicate that clone needs table fix up on recovery. */
  file_name.assign(path_name);
  file_name.append(CLONE_INNODB_FIXUP_FILE);
  create_file(file_name);

  /* Indicate clone needs to update recovery status. */
  file_name.assign(path_name);
  file_name.append(CLONE_INNODB_REPLACED_FILES);
  create_file(file_name);

  /* Mark successful clone operation. */
  file_name.assign(path_name);
  file_name.append(CLONE_INNODB_IN_PROGRESS_FILE);
  remove_file(file_name);
}

void clone_init_list_files() {
  /* Remove any existing list files. */
  std::string new_files(CLONE_INNODB_NEW_FILES);
  if (file_exists(new_files)) {
    remove_file(new_files);
  }
  std::string old_files(CLONE_INNODB_OLD_FILES);
  if (file_exists(old_files)) {
    remove_file(old_files);
  }
  std::string replaced_files(CLONE_INNODB_REPLACED_FILES);
  if (file_exists(replaced_files)) {
    remove_file(replaced_files);
  }
  std::string recovery_file(CLONE_INNODB_RECOVERY_FILE);
  if (file_exists(recovery_file)) {
    remove_file(recovery_file);
  }
}

int clone_add_to_list_file(const char *list_file_name, const char *file_name) {
  std::ofstream list_file;
  list_file.open(list_file_name, std::ofstream::app);

  if (list_file.is_open()) {
    list_file << file_name << std::endl;

    if (list_file.good()) {
      list_file.close();
      return (0);
    }
    list_file.close();
  }
  /* This is an error case. Either open or write call failed. */
  char errbuf[MYSYS_STRERROR_SIZE];
  my_error(ER_ERROR_ON_WRITE, MYF(0), list_file_name, errno,
           my_strerror(errbuf, sizeof(errbuf), errno));
  return (ER_ERROR_ON_WRITE);
}

/** Add all existing redo files to old file list. */
static void track_redo_files() {
  std::string log_file;
  for (uint32_t index = 0; index < srv_n_log_files; ++index) {
    /* Build redo log file name. */
    char file_name[MAX_LOG_FILE_NAME + 1];
    snprintf(file_name, MAX_LOG_FILE_NAME, "%s%u", ib_logfile_basename, index);

    log_file.assign(srv_log_group_home_dir);
    if (!log_file.empty() && log_file.back() != OS_PATH_SEPARATOR) {
      log_file.append(OS_PATH_SEPARATOR_STR);
    }
    log_file.append(file_name);
    clone_add_to_list_file(CLONE_INNODB_OLD_FILES, log_file.c_str());
  }
}

/** Execute sql statement.
@param[in,out]	thd		current THD
@param[in]	sql_stmt	SQL statement
@param[in]	thread_number	executing thread number
@param[in]	skip_error	skip statement on error
@return false, if successful. */
static bool clone_execute_query(THD *thd, const char *sql_stmt,
                                size_t thread_number, bool skip_error);

/** Delete all binary logs before clone.
@param[in]	thd	current THD
@return error code */
static int clone_drop_binary_logs(THD *thd);

/** Drop all user data before starting clone.
@param[in,out]	thd		current THD
@param[in]	allow_threads	allow multiple threads
@return error code */
static int clone_drop_user_data(THD *thd, bool allow_threads);

/** Set security context to skip privilege check.
@param[in,out]	thd	session THD
@param[in,out]	sctx	security context */
static void skip_grants(THD *thd, Security_context &sctx) {
  /* Take care of the possible side effect of skipping grant i.e.
  setting SYSTEM_USER privilege flag. */
  bool saved_flag = thd->is_system_user();
  sctx.skip_grants();
  ut_ad(thd->is_system_user() == saved_flag);
  thd->set_system_user(saved_flag);
}

void innodb_clone_get_capability(Ha_clone_flagset &flags) {
  flags.reset();

  flags.set(HA_CLONE_HYBRID);
  flags.set(HA_CLONE_MULTI_TASK);
  flags.set(HA_CLONE_RESTART);
}

int innodb_clone_begin(handlerton *hton, THD *thd, const byte *&loc,
                       uint &loc_len, uint &task_id, Ha_clone_type type,
                       Ha_clone_mode mode) {
  /* Check if reference locator is valid */
  if (loc != nullptr && !clone_validate_locator(loc, loc_len)) {
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Locator");
    return (err);
  }

  /* Acquire clone system mutex which would automatically get released
  when we return from the function [RAII]. */
  IB_mutex_guard sys_mutex(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    if (thd != nullptr) {
      my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
    }

    return (ER_CLONE_DDL_IN_PROGRESS);
  }

  /* Check of clone is already in progress for the reference locator. */
  auto clone_hdl = clone_sys->find_clone(loc, loc_len, CLONE_HDL_COPY);

  int err = 0;

  switch (mode) {
    case HA_CLONE_MODE_RESTART:
      /* Error out if existing clone is not found */
      if (clone_hdl == nullptr) {
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone Restart could not find existing clone");
        return (ER_INTERNAL_ERROR);
      }

      ib::info(ER_IB_CLONE_START_STOP) << "Clone Begin Master Task: Restart";
      err = clone_hdl->restart_copy(thd, loc, loc_len);

      break;

    case HA_CLONE_MODE_START: {
      /* Should not find existing clone for the locator */
      if (clone_hdl != nullptr) {
        clone_sys->drop_clone(clone_hdl);
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone Begin refers existing clone");
        return (ER_INTERNAL_ERROR);
      }
      LEX_CSTRING sctx_user = thd->m_main_security_ctx.user();
      LEX_CSTRING sctx_host = thd->m_main_security_ctx.host_or_ip();

      /* Should not become a donor when provisioning is started. */
      if (Clone_handler::is_provisioning()) {
        if (0 == strcmp(my_localhost, sctx_host.str)) {
          my_error(ER_CLONE_LOOPBACK, MYF(0));
          return (ER_CLONE_LOOPBACK);
        }
        my_error(ER_CLONE_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_CLONES);
        return (ER_CLONE_TOO_MANY_CONCURRENT_CLONES);
      }

      /* Log user and host beginning clone operation. */
      ib::info(ER_IB_CLONE_START_STOP) << "Clone Begin Master Task by "
                                       << sctx_user.str << "@" << sctx_host.str;
      break;
    }

    case HA_CLONE_MODE_ADD_TASK:
      /* Should find existing clone for the locator */
      if (clone_hdl == nullptr) {
        /* Operation has finished already */
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone add task refers non-existing clone");

        return (ER_INTERNAL_ERROR);
      }
      break;

    case HA_CLONE_MODE_VERSION:
    case HA_CLONE_MODE_MAX:
    default:
      ut_ad(false);
      my_error(ER_INTERNAL_ERROR, MYF(0), "Innodb Clone Begin Invalid Mode");

      return (ER_INTERNAL_ERROR);
  }

  if (clone_hdl == nullptr) {
    ut_ad(thd != nullptr);
    ut_ad(mode == HA_CLONE_MODE_START);

    /* Create new clone handle for copy. Reference locator
    is used for matching the version. */
    auto err = clone_sys->add_clone(loc, CLONE_HDL_COPY, clone_hdl);
    if (err != 0) {
      return (err);
    }

    err = clone_hdl->init(loc, loc_len, type, nullptr);

    /* Check and wait if clone is marked for wait. */
    if (err == 0) {
      err = clone_sys->wait_for_free(thd);
    }

    if (err != 0) {
      clone_sys->drop_clone(clone_hdl);
      return (err);
    }
  }

  /* Add new task for the clone copy operation. */
  if (err == 0) {
    /* Release clone system mutex here as we might need to wait while
    adding task. It is safe as the clone handle is acquired and cannot
    be freed till we release it. */
    mutex_exit(clone_sys->get_mutex());
    err = clone_hdl->add_task(thd, nullptr, 0, task_id);
    mutex_enter(clone_sys->get_mutex());
  }

  if (err != 0) {
    clone_sys->drop_clone(clone_hdl);
    return (err);
  }

  if (task_id > 0) {
    ib::info(ER_IB_CLONE_START_STOP) << "Clone Begin Task ID: " << task_id;
  }

  /* Get the current locator from clone handle. */
  loc = clone_hdl->get_locator(loc_len);
  return (0);
}

int innodb_clone_copy(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                      uint task_id, Ha_clone_cbk *cbk) {
  cbk->set_hton(hton);

  /* Get clone handle by locator index. */
  auto clone_hdl = clone_sys->get_clone_by_index(loc, loc_len);

  auto err = clone_hdl->check_error(thd);
  if (err != 0) {
    return (err);
  }

  /* Start data copy. */
  err = clone_hdl->copy(thd, task_id, cbk);
  clone_hdl->save_error(err);

  return (err);
}

int innodb_clone_ack(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err, Ha_clone_cbk *cbk) {
  cbk->set_hton(hton);

  /* Check if reference locator is valid */
  if (loc != nullptr && !clone_validate_locator(loc, loc_len)) {
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Locator");
    return (err);
  }
  mutex_enter(clone_sys->get_mutex());

  /* Find attach clone handle using the reference locator. */
  auto clone_hdl = clone_sys->find_clone(loc, loc_len, CLONE_HDL_COPY);

  mutex_exit(clone_sys->get_mutex());

  /* Must find existing clone for the locator */
  if (clone_hdl == nullptr) {
    my_error(ER_INTERNAL_ERROR, MYF(0),

             "Innodb Clone ACK refers non-existing clone");
    return (ER_INTERNAL_ERROR);
  }

  int err = 0;

  /* If thread is interrupted, then set interrupt error instead. */
  if (thd_killed(thd)) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    in_err = ER_QUERY_INTERRUPTED;
  }

  if (in_err == 0) {
    /* Apply acknowledged data */
    err = clone_hdl->apply(thd, task_id, cbk);

    clone_hdl->save_error(err);
  } else {
    /* For error input, return after saving it */
    ib::info(ER_IB_CLONE_OPERATION) << "Clone set error ACK: " << in_err;
    clone_hdl->save_error(in_err);
  }

  mutex_enter(clone_sys->get_mutex());

  /* Detach from clone handle */
  clone_sys->drop_clone(clone_hdl);

  mutex_exit(clone_sys->get_mutex());

  return (err);
}

int innodb_clone_end(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err) {
  /* Acquire clone system mutex which would automatically get released
  when we return from the function [RAII]. */
  IB_mutex_guard sys_mutex(clone_sys->get_mutex());

  /* Get clone handle by locator index. */
  auto clone_hdl = clone_sys->get_clone_by_index(loc, loc_len);

  /* If thread is interrupted, then set interrupt error instead. */
  if (thd_killed(thd)) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    in_err = ER_QUERY_INTERRUPTED;
  }
  /* Set error, if already not set */
  clone_hdl->save_error(in_err);

  /* Drop current task. */
  bool is_master = false;
  auto wait_reconnect = clone_hdl->drop_task(thd, task_id, in_err, is_master);
  auto is_copy = clone_hdl->is_copy_clone();
  auto is_init = clone_hdl->is_init();
  auto is_abort = clone_hdl->is_abort();

  if (!wait_reconnect || is_abort) {
    if (is_copy && is_master) {
      if (is_abort) {
        ib::info(ER_IB_CLONE_RESTART)
            << "Clone Master aborted by concurrent clone";

      } else if (in_err != 0) {
        /* Make sure re-start attempt fails immediately */
        clone_hdl->set_state(CLONE_STATE_ABORT);
      }
    }

    if (!is_copy && !is_init && is_master) {
      if (in_err == 0) {
        /* On success for apply handle, drop status file. */
        drop_status_file(clone_hdl);
      } else if (clone_hdl->replace_datadir()) {
        /* On failure, rollback if replacing current data directory. */
        clone_files_error();
      }
    }
    clone_sys->drop_clone(clone_hdl);

    auto da = thd->get_stmt_da();
    ib::info(ER_IB_CLONE_START_STOP)
        << "Clone"
        << (is_copy ? " End" : (is_init ? " Apply Version End" : " Apply End"))
        << (is_master ? " Master" : "") << " Task ID: " << task_id
        << (in_err != 0 ? " Failed, code: " : " Passed, code: ") << in_err
        << ": "
        << ((in_err == 0 || da == nullptr || !da->is_error())
                ? ""
                : da->message_text());
    return (0);
  }

  auto da = thd->get_stmt_da();
  ib::info(ER_IB_CLONE_RESTART)
      << "Clone Master wait for restart"
      << " after n/w error code: " << in_err << ": "
      << ((da == nullptr || !da->is_error()) ? "" : da->message_text());

  ut_ad(clone_hdl->is_copy_clone());
  ut_ad(is_master);

  /* Set state to idle and wait for re-connect */
  clone_hdl->set_state(CLONE_STATE_IDLE);
  /* Sleep for 1 second */
  Clone_Msec sleep_time(Clone_Sec(1));
  /* Generate alert message every minute. */
  Clone_Sec alert_interval(Clone_Min(1));
  /* Wait for 5 minutes for client to reconnect back */
  Clone_Sec time_out(Clone_Min(5));

  bool is_timeout = false;
  auto err = Clone_Sys::wait(
      sleep_time, time_out, alert_interval,
      [&](bool alert, bool &result) {
        ut_ad(mutex_own(clone_sys->get_mutex()));
        result = !clone_hdl->is_active();

        if (thd_killed(thd) || clone_hdl->is_interrupted()) {
          ib::info(ER_IB_CLONE_RESTART)
              << "Clone End Master wait for Restart interrupted";
          my_error(ER_QUERY_INTERRUPTED, MYF(0));
          return (ER_QUERY_INTERRUPTED);

        } else if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
          ib::info(ER_IB_CLONE_RESTART)
              << "Clone End Master wait for Restart aborted by DDL";
          my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
          return (ER_CLONE_DDL_IN_PROGRESS);

        } else if (clone_hdl->is_abort()) {
          result = false;
          ib::info(ER_IB_CLONE_RESTART) << "Clone End Master wait for Restart"
                                           " aborted by concurrent clone";
          return (0);
        }

        if (!result) {
          ib::info(ER_IB_CLONE_RESTART)
              << "Clone Master restarted successfully by "
                 "other task after n/w failure";

        } else if (alert) {
          ib::info(ER_IB_CLONE_RESTART)
              << "Clone Master still waiting for restart";
        }
        return (0);
      },
      clone_sys->get_mutex(), is_timeout);

  if (err == 0 && is_timeout && clone_hdl->is_idle()) {
    ib::info(ER_IB_CLONE_TIMEOUT) << "Clone End Master wait "
                                     "for restart timed out after "
                                     "5 Minutes. Dropping Snapshot";
  }
  /* Last task should drop the clone handle. */
  clone_sys->drop_clone(clone_hdl);
  return (0);
}

int innodb_clone_apply_begin(handlerton *hton, THD *thd, const byte *&loc,
                             uint &loc_len, uint &task_id, Ha_clone_mode mode,
                             const char *data_dir) {
  /* Check if reference locator is valid */
  if (loc != nullptr && !clone_validate_locator(loc, loc_len)) {
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Locator");
    return (err);
  }

  /* Acquire clone system mutex which would automatically get released
  when we return from the function [RAII]. */
  IB_mutex_guard sys_mutex(clone_sys->get_mutex());

  /* Check if clone is already in progress for the reference locator. */
  auto clone_hdl = clone_sys->find_clone(loc, loc_len, CLONE_HDL_APPLY);

  switch (mode) {
    case HA_CLONE_MODE_RESTART: {
      ib::info(ER_IB_CLONE_RESTART) << "Clone Apply Begin Master Task: Restart";
      auto err = clone_hdl->restart_apply(thd, loc, loc_len);

      /* Reduce reference count */
      clone_sys->drop_clone(clone_hdl);

      /* Restart is done by master task */
      ut_ad(task_id == 0);
      task_id = 0;

      return (err);
    }
    case HA_CLONE_MODE_START:

      if (clone_hdl != nullptr) {
        ut_ad(false);
        clone_sys->drop_clone(clone_hdl);
        ib::error(ER_IB_CLONE_INTERNAL)
            << "Clone Apply Begin Master found duplicate clone";
        clone_hdl = nullptr;
      }

      /* Check if the locator is from current mysqld server. */
      clone_hdl = clone_sys->find_clone(loc, loc_len, CLONE_HDL_COPY);

      if (clone_hdl != nullptr) {
        clone_sys->drop_clone(clone_hdl);
        clone_hdl = nullptr;
        ib::info(ER_IB_CLONE_START_STOP) << "Clone Apply Master Loop Back";
        ut_ad(data_dir != nullptr);
      }
      ib::info(ER_IB_CLONE_START_STOP) << "Clone Apply Begin Master Task";
      break;

    case HA_CLONE_MODE_ADD_TASK:
      /* Should find existing clone for the locator */
      if (clone_hdl == nullptr) {
        /* Operation has finished already */
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone Apply add task to non-existing clone");

        return (ER_INTERNAL_ERROR);
      }
      break;

    case HA_CLONE_MODE_VERSION:
      /* Cannot have input locator or existing clone */
      ib::info(ER_IB_CLONE_START_STOP)
          << "Clone Apply Begin Master Version Check";
      ut_ad(loc == nullptr);
      ut_ad(clone_hdl == nullptr);
      break;

    case HA_CLONE_MODE_MAX:
    default:
      ut_ad(false);

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Innodb Clone Appply Begin Invalid Mode");

      return (ER_INTERNAL_ERROR);
  }

  if (clone_hdl == nullptr) {
    ut_ad(thd != nullptr);

    ut_ad(mode == HA_CLONE_MODE_VERSION || mode == HA_CLONE_MODE_START);

    /* Create new clone handle for apply. Reference locator
    is used for matching the version. */
    auto err = clone_sys->add_clone(loc, CLONE_HDL_APPLY, clone_hdl);
    if (err != 0) {
      return (err);
    }

    err = clone_hdl->init(loc, loc_len, HA_CLONE_BLOCKING, data_dir);

    if (err != 0) {
      clone_sys->drop_clone(clone_hdl);
      return (err);
    }
  }

  if (clone_hdl->is_active()) {
    /* Release clone system mutex here as we might need to wait while
    adding task. It is safe as the clone handle is acquired and cannot
    be freed till we release it. */
    mutex_exit(clone_sys->get_mutex());

    /* Create status file to indicate active clone directory. */
    if (mode == HA_CLONE_MODE_START) {
      create_status_file(clone_hdl);
    }

    int err = 0;
    /* Drop any user data after acquiring backup lock. Don't allow
    concurrent threads as the BACKUP MDL lock would not allow any
    other threads to execute DDL. */
    if (clone_hdl->replace_datadir() && mode == HA_CLONE_MODE_START) {
      /* Safeguard to throw error if innodb read only mode is on. Currently
      not reachable as we would get error much earlier while dropping user
      tables. */
      if (srv_read_only_mode) {
        ut_ad(false);
        err = ER_INTERNAL_ERROR;
        my_error(err, MYF(0),
                 "Clone cannot replace data with innodb_read_only = ON");
      } else {
        track_redo_files();
        err = clone_drop_user_data(thd, false);
        if (err != 0) {
          clone_files_error();
        }
      }
    }

    /* Add new task for the clone apply operation. */
    if (err == 0) {
      ut_ad(loc != nullptr);
      err = clone_hdl->add_task(thd, loc, loc_len, task_id);
    }
    mutex_enter(clone_sys->get_mutex());

    if (err != 0) {
      clone_sys->drop_clone(clone_hdl);
      return (err);
    }

  } else {
    ut_ad(mode == HA_CLONE_MODE_VERSION);

    /* Set all clone status files empty. */
    if (clone_hdl->replace_datadir()) {
      clone_init_list_files();
    }
  }

  if (task_id > 0) {
    ib::info(ER_IB_CLONE_START_STOP)
        << "Clone Apply Begin Task ID: " << task_id;
  }
  /* Get the current locator from clone handle. */
  if (mode != HA_CLONE_MODE_ADD_TASK) {
    loc = clone_hdl->get_locator(loc_len);
  }
  return (0);
}

int innodb_clone_apply(handlerton *hton, THD *thd, const byte *loc,
                       uint loc_len, uint task_id, int in_err,
                       Ha_clone_cbk *cbk) {
  /* Get clone handle by locator index. */
  auto clone_hdl = clone_sys->get_clone_by_index(loc, loc_len);
  ut_ad(in_err != 0 || cbk != nullptr);

  /* For error input, return after saving it */
  if (in_err != 0 || cbk == nullptr) {
    clone_hdl->save_error(in_err);
    auto da = thd->get_stmt_da();
    ib::info(ER_IB_CLONE_OPERATION)
        << "Clone Apply set error code: " << in_err << ": "
        << ((in_err == 0 || da == nullptr || !da->is_error())
                ? ""
                : da->message_text());
    return (0);
  }

  cbk->set_hton(hton);
  auto err = clone_hdl->check_error(thd);
  if (err != 0) {
    return (err);
  }

  /* Apply data received from callback. */
  err = clone_hdl->apply(thd, task_id, cbk);
  clone_hdl->save_error(err);

  return (err);
}

int innodb_clone_apply_end(handlerton *hton, THD *thd, const byte *loc,
                           uint loc_len, uint task_id, int in_err) {
  auto err = innodb_clone_end(hton, thd, loc, loc_len, task_id, in_err);
  return (err);
}

/* Logical bitmap for clone file state. */

/** Data file is found. */
const int FILE_DATA = 1;
/** Saved data file is found */
const int FILE_SAVED = 10;
/** Cloned data file is found */
const int FILE_CLONED = 100;

/** NONE state: file not present. */
const int FILE_STATE_NONE = 0;
/** Normal state: only data file is present. */
const int FILE_STATE_NORMAL = FILE_DATA;
/** Saved state: only saved data file is present. */
const int FILE_STATE_SAVED = FILE_SAVED;
/** Cloned state: data file and cloned data file are present. */
const int FILE_STATE_CLONED = FILE_DATA + FILE_CLONED;
/** Saved clone state: saved data file and cloned data file are present. */
const int FILE_STATE_CLONE_SAVED = FILE_SAVED + FILE_CLONED;
/** Replaced state: saved data file and data file are present. */
const int FILE_STATE_REPLACED = FILE_SAVED + FILE_DATA;

/* Clone data File state transfer.
  [FILE_STATE_NORMAL] --> [FILE_STATE_CLONED]
    Remote data is cloned into another file named <file_name>.clone.

  [FILE_STATE_CLONED] --> [FILE_STATE_CLONE_SAVED]
    Before recovery the datafile is saved in a file named <file_name>.save.

  [FILE_STATE_CLONE_SAVED] --> [FILE_STATE_REPLACED]
    Before recovery the cloned file is moved to datafile.

  [FILE_STATE_REPLACED] --> [FILE_STATE_NORMAL]
    After successful recovery the saved data file is removed.

  Every state transition involves a single file create, delete or rename and
  we consider them atomic. In case of a failure the state rolls back exactly
  in reverse order.
*/

/** Get current state of a clone file.
@param[in]	data_file	data file name
@return current file state. */
static int get_file_state(std::string data_file) {
  int state = 0;
  /* Check if data file is there. */
  if (file_exists(data_file)) {
    state += FILE_DATA;
  }

  std::string saved_file(data_file);
  saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);

  /* Check if saved old file is there. */
  if (file_exists(saved_file)) {
    state += FILE_SAVED;
  }

  std::string cloned_file(data_file);
  cloned_file.append(CLONE_INNODB_REPLACED_FILE_EXTN);

  /* Check if cloned file is there. */
  if (file_exists(cloned_file)) {
    state += FILE_CLONED;
  }
  return (state);
}

/** Roll forward clone file state till final state.
@param[in]	data_file	data file name
@param[in]	final_state	data file state to forward to
@return previous file state before roll forward. */
static int file_roll_forward(std::string &data_file, int final_state) {
  auto cur_state = get_file_state(data_file);

  switch (cur_state) {
    case FILE_STATE_CLONED: {
      if (final_state == FILE_STATE_CLONED) {
        break;
      }
      /* Save data file */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      rename_file(data_file, saved_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Forward: Save data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_CLONE_SAVED: {
      if (final_state == FILE_STATE_CLONE_SAVED) {
        break;
      }
      /* Replace data file with cloned file. */
      std::string cloned_file(data_file);
      cloned_file.append(CLONE_INNODB_REPLACED_FILE_EXTN);
      rename_file(cloned_file, data_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Forward: Rename clone to data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_REPLACED: {
      if (final_state == FILE_STATE_REPLACED) {
        break;
      }
      /* Remove saved data file */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      remove_file(saved_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Forward: Remove saved data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_NORMAL:
      /* Nothing to do. */
      break;

    default:
      ib::fatal(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Forward: Invalid File State: " << cur_state;
  }
  return (cur_state);
}

/** Roll back clone file state to normal state.
@param[in]	data_file	data file name */
static void file_rollback(std::string &data_file) {
  auto cur_state = get_file_state(data_file);

  switch (cur_state) {
    case FILE_STATE_REPLACED: {
      /* Replace data file back to cloned file. */
      std::string cloned_file(data_file);
      cloned_file.append(CLONE_INNODB_REPLACED_FILE_EXTN);
      rename_file(data_file, cloned_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Back: Rename data to cloned file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_CLONE_SAVED: {
      /* Replace data file with saved file. */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      rename_file(saved_file, data_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Back: Rename saved to data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_CLONED: {
      /* Remove cloned data file. */
      std::string cloned_file(data_file);
      cloned_file.append(CLONE_INNODB_REPLACED_FILE_EXTN);
      remove_file(cloned_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Back: Remove cloned file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_NORMAL:
      /* Nothing to do. */
      break;

    default:
      ib::fatal(ER_IB_CLONE_STATUS_FILE)
          << "Clone File Roll Back: Invalid File State: " << cur_state;
  }
}

/* Clone old data File state transfer. These files are present only in
recipient and we haven't drop the database objects (table/tablespace)
before clone. Currently used for user created undo tablespace. Dropping
undo tablespace could be expensive as we need to wait for purge to finish.
  [FILE_STATE_NORMAL] --> [FILE_STATE_SAVED]
    Before recovery the old datafile is saved in a file named <file_name>.save.

  [FILE_STATE_SAVED] --> [FILE_STATE_NONE]
    After successful recovery the saved data file is removed.

  These state transitions involve a single file delete or rename and
  we consider them atomic. In case of a failure the state rolls back.

  [FILE_STATE_SAVED] --> [FILE_STATE_NORMAL]
    On failure saved data file is moved back to original data file.
*/

/** Roll forward old data file state till final state.
@param[in]	data_file	data file name
@param[in]	final_state	data file state to forward to */
static void old_file_roll_forward(std::string &data_file, int final_state) {
  auto cur_state = get_file_state(data_file);

  switch (cur_state) {
    case FILE_STATE_CLONED:
    case FILE_STATE_CLONE_SAVED:
    case FILE_STATE_REPLACED:
      /* If the file is also cloned, we can skip here as it would be handled
      with other cloned files. */
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Forward: Skipped cloned file " << data_file
          << " state: " << cur_state;
      break;
    case FILE_STATE_NORMAL: {
      if (final_state == FILE_STATE_NORMAL) {
        ut_ad(false);
        break;
      }
      /* Save data file */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      rename_file(data_file, saved_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Forward: Saved data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_SAVED: {
      if (final_state == FILE_STATE_SAVED) {
        break;
      }
      /* Remove saved data file */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      remove_file(saved_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Forward: Remove saved file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_NONE:
      /* Nothing to do. */
      break;

    default:
      ib::fatal(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Forward: Invalid File State: " << cur_state;
  }
}

/** Roll back old data file state to normal state.
@param[in]	data_file	data file name */
static void old_file_rollback(std::string &data_file) {
  auto cur_state = get_file_state(data_file);

  switch (cur_state) {
    case FILE_STATE_CLONED:
    case FILE_STATE_CLONE_SAVED:
    case FILE_STATE_REPLACED:
      /* If the file is also cloned, we can skip here as it would be handled
      with other cloned files. */
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Back: Skip cloned file " << data_file
          << " state: " << cur_state;
      break;

    case FILE_STATE_SAVED: {
      /* Replace data file with saved file. */
      std::string saved_file(data_file);
      saved_file.append(CLONE_INNODB_SAVED_FILE_EXTN);
      rename_file(saved_file, data_file);
      ib::info(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Back: Renamed saved data file " << data_file
          << " state: " << cur_state;
    }
      /* Fall through */

    case FILE_STATE_NORMAL:
    case FILE_STATE_NONE:
      /* Nothing to do. */
      break;

    default:
      ib::fatal(ER_IB_CLONE_STATUS_FILE)
          << "Clone Old File Roll Back: Invalid File State: " << cur_state;
  }
}

/** Fatal error callback function. Don't call other functions from here. Don't
use ut_a, ut_ad asserts or ib::fatal to avoid recursive invocation. */
static void clone_files_fatal_error() {
  /* Safeguard to avoid recursive call. */
  static bool started_error_handling = false;
  if (started_error_handling) {
    return;
  }
  started_error_handling = true;

  std::ifstream err_file(CLONE_INNODB_ERROR_FILE);
  if (err_file.is_open()) {
    err_file.close();
  } else {
    /* Create error file if not there. */
    std::ofstream new_file(CLONE_INNODB_ERROR_FILE);
    /* On creation failure, return and abort. */
    if (!new_file.is_open()) {
      return;
    }
    new_file.close();
  }
  /* In case of fatal error, from ib::fatal and ut_a asserts
  we terminate the process here and send the exit status so that a
  managed server can be restarted with older data files. */
  std::_Exit(MYSQLD_RESTART_EXIT);
}

/** Update recovery status file at end of clone recovery.
@param[in]	finished	true if finishing clone recovery
@param[in]	is_error	if recovery error
@param[in]	is_replace	true, if replacing current directory */
static void clone_update_recovery_status(bool finished, bool is_error,
                                         bool is_replace) {
  /* true, when we are recovering a cloned database. */
  static bool recovery_in_progress = false;
  /* true, when replacing current data directory. */
  static bool recovery_replace = false;

  std::function<void()> callback_function;

  /* Mark the beginning of clone recovery. */
  if (!finished) {
    recovery_in_progress = true;
    if (is_replace) {
      recovery_replace = true;
      callback_function = clone_files_fatal_error;
      ut_set_assert_callback(callback_function);
    }
    return;
  }
  is_replace = recovery_replace;
  recovery_replace = false;

  /* Update status only if clone recovery in progress. */
  if (!recovery_in_progress) {
    return;
  }

  /* Mark end of clone recovery process. */
  recovery_in_progress = false;
  ut_set_assert_callback(callback_function);

  std::string file_name;

  file_name.assign(CLONE_INNODB_RECOVERY_FILE);
  if (!file_exists(file_name)) {
    return;
  }

  std::ofstream status_file;
  status_file.open(file_name, std::ofstream::app);
  if (!status_file.is_open()) {
    return;
  }

  /* Write zero for unsuccessful recovery. */
  uint64_t end_time = 0;
  if (is_error) {
    status_file << end_time << std::endl;
    status_file.close();
    /* Set recovery error so that server can restart only for replace. */
    clone_recovery_error = is_replace;
    return;
  }

  /* Write recovery end time */
  end_time = my_micro_time();
  status_file << end_time << std::endl;
  if (!status_file.good()) {
    status_file.close();
    return;
  }

  mtr_t mtr;
  mtr.start();
  byte *binlog_pos = trx_sysf_get(&mtr) + TRX_SYS_MYSQL_LOG_INFO;

  /* Check logfile magic number. */
  if (mach_read_from_4(binlog_pos + TRX_SYS_MYSQL_LOG_MAGIC_N_FLD) !=
      TRX_SYS_MYSQL_LOG_MAGIC_N) {
    mtr.commit();
    status_file.close();
    return;
  }
  /* Write binary log file name. */
  status_file << binlog_pos + TRX_SYS_MYSQL_LOG_NAME << std::endl;
  if (!status_file.good()) {
    mtr.commit();
    status_file.close();
    return;
  }

  auto high = mach_read_from_4(binlog_pos + TRX_SYS_MYSQL_LOG_OFFSET_HIGH);
  auto low = mach_read_from_4(binlog_pos + TRX_SYS_MYSQL_LOG_OFFSET_LOW);

  auto log_offset = static_cast<uint64_t>(high);
  log_offset = (log_offset << 32);
  log_offset |= static_cast<uint64_t>(low);

  /* Write log file offset. */
  status_file << log_offset << std::endl;

  mtr.commit();
  status_file.close();
  /* Set clone startup for GR, only during replace. */
  clone_startup = is_replace;
}

/** Initialize recovery status for cloned recovery.
@param[in]	replace		we are replacing current directory. */
static void clone_init_recovery_status(bool replace) {
  std::string file_name;
  file_name.assign(CLONE_INNODB_RECOVERY_FILE);

  std::ofstream status_file;
  status_file.open(file_name, std::ofstream::out | std::ofstream::trunc);
  if (!status_file.is_open()) {
    return;
  }
  /* Write recovery begin time */
  uint64_t begin_time = my_micro_time();
  status_file << begin_time << std::endl;
  status_file.close();
  clone_update_recovery_status(false, false, replace);
}

void clone_update_gtid_status(std::string &gtids) {
  /* Return if not clone database recovery. */
  std::string replace_files(CLONE_INNODB_REPLACED_FILES);
  if (!file_exists(replace_files)) {
    return;
  }
  /* Return if status file is not created. */
  std::string recovery_file(CLONE_INNODB_RECOVERY_FILE);
  if (!file_exists(recovery_file)) {
    ut_ad(false);
    return;
  }
  /* Open status file to append GTID. */
  std::ofstream status_file;
  status_file.open(recovery_file, std::ofstream::app);
  if (!status_file.is_open()) {
    return;
  }
  status_file << gtids << std::endl;
  status_file.close();

  /* Remove replace file after successful recovery and status update. */
  std::ifstream files;
  files.open(replace_files);

  if (files.is_open()) {
    /* If file is not empty, we are replacing data directory. */
    std::string file_name;
    if (std::getline(files, file_name)) {
      clone_startup = true;
    }
    files.close();
  }
  remove_file(replace_files);
}

void clone_files_error() {
  /* Check if clone file directory exists. */
  os_file_type_t type;
  bool exists = false;
  auto ret = os_file_status(CLONE_FILES_DIR, &exists, &type);
  if (!ret || !exists) {
    return;
  }

  std::string err_file(CLONE_INNODB_ERROR_FILE);

  /* Create error status file if not there. */
  if (!file_exists(err_file)) {
    create_file(err_file);
  }

  std::ifstream files;
  std::string data_file;

  /* Open old file to get all files to be moved. */
  files.open(CLONE_INNODB_OLD_FILES);
  if (files.is_open()) {
    /* Extract and process all files to be moved */
    while (std::getline(files, data_file)) {
      old_file_rollback(data_file);
    }
    files.close();
    std::string old_files(CLONE_INNODB_OLD_FILES);
    remove_file(old_files);
  }

  /* Open file to get all files to be replaced. */
  files.open(CLONE_INNODB_REPLACED_FILES);
  if (files.is_open()) {
    /* Extract and process all files to be replaced */
    while (std::getline(files, data_file)) {
      file_rollback(data_file);
    }
    files.close();
    std::string replace_files(CLONE_INNODB_REPLACED_FILES);
    remove_file(replace_files);
  }

  /* Open file to get all new files to delete. */
  files.open(CLONE_INNODB_NEW_FILES);
  if (files.is_open()) {
    /* Extract and process all files to be replaced */
    while (std::getline(files, data_file)) {
      remove_file(data_file);
    }
    files.close();
    std::string new_files(CLONE_INNODB_NEW_FILES);
    remove_file(new_files);
  }

  /* Remove error status file. */
  remove_file(err_file);

  /* Update recovery status file for recovery error. */
  clone_update_recovery_status(true, true, true);
}

#ifdef UNIV_DEBUG
bool clone_check_recovery_crashpoint(bool is_cloned_db) {
  if (!is_cloned_db) {
    return (true);
  }
  std::string crash_file(CLONE_INNODB_RECOVERY_CRASH_POINT);

  if (file_exists(crash_file)) {
    remove_file(crash_file);
    return (false);
  }
  return (true);
}
#endif

void clone_files_recovery(bool finished) {
  /* Clone error file is present in case of error. */
  std::string file_name;
  file_name.assign(CLONE_INNODB_ERROR_FILE);

  if (file_exists(file_name)) {
    ut_ad(!finished);
    clone_files_error();
    return;
  }

  /* if replace file is not present, remove old file. */
  if (!finished) {
    std::string replace_files(CLONE_INNODB_REPLACED_FILES);
    std::string old_files(CLONE_INNODB_OLD_FILES);
    if (!file_exists(replace_files) && file_exists(old_files)) {
      ut_ad(false);
      remove_file(old_files);
    }
  }

  std::ifstream files;

  /* Open files to get all old files to be saved or removed. Must handle
  the old files before cloned files. This is because during old file
  processing we need to skip the common files based on cloned state. If
  the cloned state is reset then these files would be considered as old
  files and removed. */
  int end_state = finished ? FILE_STATE_NONE : FILE_STATE_SAVED;
  files.open(CLONE_INNODB_OLD_FILES);
  if (files.is_open()) {
    /* Extract and process all files to be saved or removed */
    while (std::getline(files, file_name)) {
      old_file_roll_forward(file_name, end_state);
    }
    files.close();

    /* Remove clone file after successful recovery. */
    if (finished) {
      std::string old_files(CLONE_INNODB_OLD_FILES);
      remove_file(old_files);
    }
  }

  /* Open file to get all files to be replaced. */
  end_state = finished ? FILE_STATE_NORMAL : FILE_STATE_REPLACED;
  files.open(CLONE_INNODB_REPLACED_FILES);

  if (files.is_open()) {
    int prev_state = FILE_STATE_NORMAL;
    /* If file is empty, it is not replace. */
    bool replace = false;

    /* Extract and process all files to be replaced */
    while (std::getline(files, file_name)) {
      replace = true;
      prev_state = file_roll_forward(file_name, end_state);
    }

    files.close();

    if (finished) {
      /* Update recovery status file at the end of clone recovery. We don't
      remove the replace file here. It would be removed only after updating
      GTID state. */
      clone_update_recovery_status(true, false, replace);
    } else {
      /* If previous state was normal, clone recovery is already done. */
      if (!replace || prev_state != FILE_STATE_NORMAL) {
        /* Clone database recovery is started. */
        clone_init_recovery_status(replace);
      }
    }
  }

  file_name.assign(CLONE_INNODB_NEW_FILES);
  auto exists = file_exists(file_name);

  if (exists && finished) {
    /* Remove clone file after successful recovery. */
    std::string new_files(CLONE_INNODB_NEW_FILES);
    remove_file(new_files);
  }
}

dberr_t clone_init() {
  /* Check if incomplete cloned data directory */
  os_file_type_t type;
  bool exists = false;
  auto status = os_file_status(CLONE_INNODB_IN_PROGRESS_FILE, &exists, &type);

  if (status && exists) {
    return (DB_ABORT_INCOMPLETE_CLONE);
  }

  /* Initialize clone files before starting recovery. */
  clone_files_recovery(false);

  if (clone_sys == nullptr) {
    ut_ad(Clone_Sys::s_clone_sys_state == CLONE_SYS_INACTIVE);
    clone_sys = UT_NEW(Clone_Sys(), mem_key_clone);
  }
  Clone_Sys::s_clone_sys_state = CLONE_SYS_ACTIVE;
  Clone_handler::init_xa();

  return (DB_SUCCESS);
}

void clone_free() {
  Clone_handler::uninit_xa();
  if (clone_sys != nullptr) {
    ut_ad(Clone_Sys::s_clone_sys_state == CLONE_SYS_ACTIVE);

    UT_DELETE(clone_sys);
    clone_sys = nullptr;
  }

  Clone_Sys::s_clone_sys_state = CLONE_SYS_INACTIVE;
}

bool clone_mark_abort(bool force) {
  bool aborted;

  mutex_enter(clone_sys->get_mutex());

  aborted = clone_sys->mark_abort(force);

  mutex_exit(clone_sys->get_mutex());

  DEBUG_SYNC_C("clone_marked_abort2");

  return (aborted);
}

void clone_mark_active() {
  mutex_enter(clone_sys->get_mutex());

  clone_sys->mark_active();

  mutex_exit(clone_sys->get_mutex());
}

bool clone_check_active() {
  mutex_enter(clone_sys->get_mutex());
  auto is_active = clone_sys->check_active_clone(false);
  mutex_exit(clone_sys->get_mutex());

  return (is_active || Clone_handler::is_provisioning());
}

bool clone_mark_wait() {
  mutex_enter(clone_sys->get_mutex());
  auto success = clone_sys->mark_wait();
  mutex_exit(clone_sys->get_mutex());
  return (success);
}

void clone_mark_free() {
  mutex_enter(clone_sys->get_mutex());
  clone_sys->mark_free();
  mutex_exit(clone_sys->get_mutex());
}

template <typename T>
using DD_Objs = std::vector<const T *>;

template <typename T>
using DD_Objs_Iter = typename DD_Objs<T>::const_iterator;

using Releaser = dd::cache::Dictionary_client::Auto_releaser;

namespace {

/** Fix schema, table and tablespace. Used for two different purposes.
1. After recovery from cloned database:
A. Create empty data file for non-Innodb tables that are not cloned.
B. Create any schema directory that is not present.

2. Before cloning into current data directory:
A. Drop all user tables.
B. Drop all user schema
C. Drop all user tablespaces.  */

class Fixup_data {
 public:
  /** Constructor.
  @param[in]	concurrent	spawn multiple threads
  @param[in]	is_drop		the operation is drop */
  Fixup_data(bool concurrent, bool is_drop)
      : m_num_tasks(), m_concurrent(concurrent), m_drop(is_drop) {
    m_num_errors.store(0);
  }

  /** Fix tables for which data is not cloned.
  @param[in,out]	thd		current	THD
  @param[in]		dd_objects	table/schema/tablespace from DD
  @return true if error */
  template <typename T>
  bool fix(THD *thd, const DD_Objs<T> &dd_objects) {
    set_num_tasks(dd_objects.size());

    using namespace std::placeholders;
    auto fixup_function =
        std::bind(&Fixup_data::fix_objects<T>, this, thd, _1, _2, _3);

    par_for(PFS_NOT_INSTRUMENTED, dd_objects, get_num_tasks(), fixup_function);

    return (failed());
  }

  /** Remove data cloned from configuration tables which are not relevant
  in recipient.
  @param[in,out]	thd	current	THD
  @return true if error */
  bool fix_config_tables(THD *thd);

  /** Number of system configuration tables. */
  static const size_t S_NUM_CONFIG_TABLES = 2;

  /** Array of configuration tables. */
  static const std::array<const char *, S_NUM_CONFIG_TABLES> s_config_tables;

 private:
  /** Check and fix specific DD object.
  @param[in,out]	thd		current	THD
  @param[in]		object		DD object
  @param[in]		thread_number	current thread number. */
  template <typename T>
  bool fix_one_object(THD *thd, const T *object, size_t thread_number);

  /** Check and fix a rangle of DD objects
  @param[in,out]	thd		current	THD
  @param[in]		begin		first element in current slice
  @param[in]		end		last element in current slice
  @param[in]		thread_number	current thread number. */
  template <typename T>
  void fix_objects(THD *thd, const DD_Objs_Iter<T> &begin,
                   const DD_Objs_Iter<T> &end, size_t thread_number);

  /** @return number of tasks. */
  size_t get_num_tasks() const { return (m_num_tasks); }

  /** Calculate and set number of new tasks to spawn.
  @param[in]	num_entries	number of entries to handle
  @param[in]	concurrent	allow multiple threads */
  void set_num_tasks(size_t num_entries) {
    /* Check if we are allowed to spawn multiple threads. Disable
    multithreading while dropping objects for now. We need more
    work to handle and pass interrupt signal to workers. */
    if (is_drop() || !allow_concurrent()) {
      m_num_tasks = 0;
      return;
    }
    /* Have one task for every 100 entries. */
    m_num_tasks = num_entries / 100;

#ifdef UNIV_DEBUG
    /* Test operation in newly spawned thread. */
    if (m_num_tasks == 0) {
      ++m_num_tasks;
    }
#endif /* UNIV_DEBUG */

    /* Don't go beyond 8 threads for now. */
    if (m_num_tasks > 8) {
      m_num_tasks = 8;
    }
    m_num_errors.store(0);
  }

  /** @return true, if current operation is drop. */
  bool is_drop() const { return (m_drop); }

  /** @return true, if concurrency is allowed. */
  bool allow_concurrent() const { return (m_concurrent); }

  /** Get the table operation string.
  @return sql key word for the operation. */
  const char *sql_operation() {
    if (is_drop()) {
      return ("DROP");
    }
    /* Alternative action is truncate. */
    return ("TRUNCATE");
  }

  /** Check if the current SE type should be skipped.
  @param[in]	type	SE type
  @return true iff the SE needs to be skipped. */
  bool skip_se_tables(enum legacy_db_type type) {
    /* Don't skip any specific DB during drop operation. All existing
    user tables are dropped before cloning a remote database. */
    if (is_drop()) {
      return (false);
    }
    /* Truncate only MyISAM and CSV tables. After clone we need to create
    empty tables for engines that are not cloned. */
    if (type == DB_TYPE_MYISAM || type == DB_TYPE_CSV_DB) {
      return (false);
    }
    return (true);
  }

  /** Check if the schema is performance schema.
  @param[in]	schema_name	schema name
  @return true iff performance schema. */
  bool is_performance_schema(const char *schema_name) const {
    return (0 == strcmp(schema_name, PERFORMANCE_SCHEMA_DB_NAME.str));
  }

  /** Check if the current schema is system schema
  @param[in]	schema_name	schema name
  @return true iff system schema. */
  bool is_system_schema(const char *schema_name) const {
    if (0 == strcmp(schema_name, MYSQL_SCHEMA_NAME.str) ||
        0 == strcmp(schema_name, "sys") ||
        0 == strcmp(schema_name, PERFORMANCE_SCHEMA_DB_NAME.str) ||
        0 == strcmp(schema_name, INFORMATION_SCHEMA_NAME.str)) {
      return (true);
    }
    return (false);
  }

  /** Check if the current schema tables needs to be skipped.
  @param[in]	table		DD table
  @param[in]	table_name	table name
  @param[in]	schema_name	schema name
  @return true iff table needs to be skipped. */
  bool skip_schema_tables(const dd::Table *table, const char *table_name,
                          const char *schema_name) {
    /* Skip specific tables only during drop. */
    if (!is_drop()) {
      return (false);
    }

    /* Handle only visible base tables. */
    if (table->type() != dd::enum_table_type::BASE_TABLE ||
        table->hidden() != dd::Abstract_table::HT_VISIBLE) {
      return (true);
    }

    /* Don't Skip tables in non-system schemas. */
    if (!is_system_schema(schema_name)) {
      return (false);
    }

    /* Skip DD system tables. */
    if (table->is_explicit_tablespace() &&
        table->tablespace_id() == dd::Dictionary_impl::dd_tablespace_id()) {
      return (true);
    }

    /* Skip all in information_schema and performance_schema tables. */
    if (0 == strcmp(schema_name, PERFORMANCE_SCHEMA_DB_NAME.str) ||
        0 == strcmp(schema_name, INFORMATION_SCHEMA_NAME.str)) {
      return (true);
    }

    /* Skip specific tables in mysql schema. */
    if (0 == strcmp(schema_name, MYSQL_SCHEMA_NAME.str) &&
        (0 == strcmp(table_name, GENERAL_LOG_NAME.str) ||
         0 == strcmp(table_name, SLOW_LOG_NAME.str))) {
      return (true);
    }

    /* Skip specific tables in sys schema. */
    if (0 == strcmp(schema_name, "sys") &&
        0 == strcmp(table_name, "sys_config")) {
      return (true);
    }

    return (false);
  }

  /** Check if the current schema needs to be skipped.
  @param[in]	schema_name	schema name
  @return true iff schema needs to be skipped. */
  bool skip_schema(const char *schema_name) {
    /* Don't drop system schema. */
    if (is_drop()) {
      return (is_system_schema(schema_name));
    }
    /* Information schema has no directory */
    if (0 == strcmp(schema_name, INFORMATION_SCHEMA_NAME.str)) {
      return (true);
    }
    return (false);
  }

  /** Check if the current tablespace needs to be skipped.
  @param[in,out]	thd		current	THD
  @param[in]		dd_space	dd tablespace
  @return true iff tablespace needs to be skipped. */
  bool skip_tablespace(THD *thd, const dd::Tablespace *dd_space) {
    /* System tablespaces are in Innodb. Skip other engines. */
    auto se =
        ha_resolve_by_name_raw(thd, lex_cstring_handle(dd_space->engine()));
    auto se_type = ha_legacy_type(se ? plugin_data<handlerton *>(se) : nullptr);
    plugin_unlock(thd, se);
    if (se_type != DB_TYPE_INNODB) {
      return (false);
    }

    /* Skip system tablespace by name. */
    const auto space_name = dd_space->name().c_str();
    const char *innodb_prefix = "innodb_";
    const char *sys_prefix = "sys/";
    if (0 == strcmp(space_name, "mysql") ||
        0 == strncmp(space_name, sys_prefix, strlen(sys_prefix)) ||
        0 == strncmp(space_name, innodb_prefix, strlen(innodb_prefix))) {
      return (true);
    }

    /* Skip undo tablespaces. */
    auto &se_data = dd_space->se_private_data();
    space_id_t space_id = SPACE_UNKNOWN;

    if (se_data.get(dd_space_key_strings[DD_SPACE_ID], &space_id) ||
        space_id == SPACE_UNKNOWN) {
      ut_ad(false);
      return (false);
    }
    bool is_undo = fsp_is_undo_tablespace(space_id);

    /* Add skipped undo tablespace files to list of old files to remove. */
    if (is_undo && !allow_concurrent()) {
      auto dd_file = *(dd_space->files().begin());
      clone_add_to_list_file(CLONE_INNODB_OLD_FILES,
                             dd_file->filename().c_str());
      /* In rare case, the undo might be kept halfway truncated due to some
      error during truncate. Check and add truncate log file as old file if
      present. */
      undo::Tablespace undo_space(space_id);
      const char *log_file_name = undo_space.log_file_name();

      os_file_type_t type;
      bool exists = false;
      auto ret = os_file_status(log_file_name, &exists, &type);
      if (ret && exists) {
        clone_add_to_list_file(CLONE_INNODB_OLD_FILES, log_file_name);
      }
    }

    /* Skip all undo tablespaces. */
    if (is_undo) {
      return (true);
    }

    /* Check and skip file per table tablespace. */
    uint32_t flags = 0;
    if (se_data.get(dd_space_key_strings[DD_SPACE_FLAGS], &flags)) {
      ut_ad(false);
      return (false);
    }

    if (fsp_is_file_per_table(space_id, flags)) {
      return (true);
    }
    return (false);
  }

  /** Form and execute sql command.
  @param[in,out]	thd		current	THD
  @param[in]		schema_name	schema name
  @param[in]		table_name	table name
  @param[in]		tablespace_name	tablespace name
  @param[in]		thread_number	current thread number. */
  bool execute_sql(THD *thd, const char *schema_name, const char *table_name,
                   const char *tablespace_name, size_t thread_number);

  /** @return true, if any thread has failed. */
  bool failed() const { return (m_num_errors.load() != 0); }

 private:
  /** Number of tasks failed. */
  std::atomic_size_t m_num_errors;

  /** Number of tasks. */
  size_t m_num_tasks;

  /** Allow concurrent threads. */
  bool m_concurrent;

  /** If the objects need to be dropped. */
  bool m_drop;
};

/** All configuration tables for which data should not be cloned. From
replication configurations only clone slave_master_info table needed by GR. */
const std::array<const char *, Fixup_data::S_NUM_CONFIG_TABLES>
    Fixup_data::s_config_tables = {"slave_relay_log_info", "slave_worker_info"};

bool Fixup_data::fix_config_tables(THD *thd) {
  /* No privilege check needed for individual tables. */
  auto saved_sctx = thd->security_context();
  Security_context sctx(*saved_sctx);
  skip_grants(thd, sctx);
  thd->set_security_context(&sctx);

  /* Disable binary logging. */
  char sql_stmt[FN_LEN + FN_LEN + 64];
  snprintf(sql_stmt, sizeof(sql_stmt), "SET SQL_LOG_BIN = OFF");
  static_cast<void>(clone_execute_query(thd, &sql_stmt[0], 1, false));

  /* Loop through all objects and fix. */
  bool ret = false;
  for (auto table : s_config_tables) {
    ret = execute_sql(thd, "mysql", table, nullptr, 1);
    if (ret) break;
  }
  /* Set back old security context. */
  thd->set_security_context(saved_sctx);
  return (ret);
}

template <typename T>
void Fixup_data::fix_objects(THD *thd, const DD_Objs_Iter<T> &begin,
                             const DD_Objs_Iter<T> &end, size_t thread_number) {
  ib::info(ER_IB_CLONE_SQL) << "Clone: Fix Object count: " << (end - begin)
                            << " task: " << thread_number;

  bool thread_created = false;

  /* For newly spawned threads, create server THD */
  if (thread_number != get_num_tasks()) {
    thd = create_thd(false, true, true, PSI_NOT_INSTRUMENTED);
    thread_created = true;
  }

  /* Save system thread type to be safe. */
  auto saved_thd_system = thd->system_thread;

  /* No privilege check needed for individual tables. */
  auto saved_sctx = thd->security_context();
  Security_context sctx(*saved_sctx);
  skip_grants(thd, sctx);
  thd->set_security_context(&sctx);

  char sql_stmt[FN_LEN + FN_LEN + 64];

  /* Disable binary logging. */
  snprintf(sql_stmt, sizeof(sql_stmt), "SET SQL_LOG_BIN = OFF");
  if (clone_execute_query(thd, &sql_stmt[0], thread_number, false)) {
    ++m_num_errors;
  }

  /* Disable foreign key check. */
  snprintf(sql_stmt, sizeof(sql_stmt), "SET FOREIGN_KEY_CHECKS=0");
  if (clone_execute_query(thd, &sql_stmt[0], thread_number, false)) {
    ++m_num_errors;
  }

  if (thread_created) {
    /* For concurrent worker threads set timeout for MDL lock. */
    snprintf(sql_stmt, sizeof(sql_stmt), "SET LOCAL LOCK_WAIT_TIMEOUT=1");
    if (clone_execute_query(thd, &sql_stmt[0], thread_number, false)) {
      ++m_num_errors;
    }
  }

  /* Loop through all objects and fix. */
  for (auto it = begin; it != end && m_num_errors == 0; ++it) {
    if (fix_one_object<T>(thd, *it, thread_number)) {
      break;
    }
  }

  /* Set back old security context. */
  thd->set_security_context(saved_sctx);
  thd->system_thread = saved_thd_system;

  /* Destroy thread if newly spawned task */
  if (thread_created) {
    destroy_thd(thd);
  }
}

template <>
bool Fixup_data::fix_one_object(THD *thd, const dd::Table *table,
                                size_t thread_number) {
  auto se = ha_resolve_by_name_raw(thd, lex_cstring_handle(table->engine()));
  auto se_type = ha_legacy_type(se ? plugin_data<handlerton *>(se) : nullptr);

  plugin_unlock(thd, se);

  if (skip_se_tables(se_type)) {
    return (false);
  }

  auto dc = dd::get_dd_client(thd);
  Releaser releaser(dc);

  const dd::Schema *table_schema = nullptr;

  auto saved_thread_type = thd->system_thread;
  thd->system_thread = SYSTEM_THREAD_DD_INITIALIZE;

  if (dc->acquire(table->schema_id(), &table_schema)) {
    ++m_num_errors;
    thd->system_thread = saved_thread_type;
    return (true);
  }

  const auto schema_name = table_schema->name().c_str();
  const auto table_name = table->name().c_str();

  /* For performance schema drop the SDI table. */
  if (is_drop() && is_performance_schema(schema_name)) {
    dd::sdi::drop(thd, table);
  }
  thd->system_thread = saved_thread_type;

  if (skip_schema_tables(table, table_name, schema_name)) {
    return (false);
  }

  /* Throw warning for MyIsam and CSV tables for which data is
  not cloned. These tables would be empty after clone. */
  if (!is_drop() && !is_system_schema(schema_name)) {
    ib::warn(ER_IB_CLONE_NON_INNODB_TABLE, schema_name, table_name);
  }

  auto ret_val =
      execute_sql(thd, schema_name, table_name, nullptr, thread_number);
  return (ret_val);
}

bool Fixup_data::execute_sql(THD *thd, const char *schema_name,
                             const char *table_name,
                             const char *tablespace_name,
                             size_t thread_number) {
  char sql_stmt[FN_LEN + FN_LEN + 64];

  if (tablespace_name != nullptr) {
    /* TABLESPACE operation */
    snprintf(sql_stmt, sizeof(sql_stmt), "DROP TABLESPACE `%s`",
             tablespace_name);

  } else if (table_name != nullptr) {
    /* TABLE operation */
    snprintf(sql_stmt, sizeof(sql_stmt), "%s TABLE `%s`.`%s`", sql_operation(),
             schema_name, table_name);
  } else {
    /* SCHEMA operation */
    snprintf(sql_stmt, sizeof(sql_stmt), "DROP SCHEMA `%s`", schema_name);
  }

  auto saved_thread_type = thd->system_thread;
  if (!is_drop()) {
    /* No MDL locks during initialization phase. */
    thd->system_thread = SYSTEM_THREAD_DD_INITIALIZE;
  }

  /* Skip error while attempting drop concurrently using multiple workers.
  We will handle the skipped objects later in in main thread.*/
  bool skip_error = is_drop() && allow_concurrent();

  auto ret_val =
      clone_execute_query(thd, &sql_stmt[0], thread_number, skip_error);
  if (ret_val) {
    ++m_num_errors;
  }

  thd->system_thread = saved_thread_type;

  if (is_drop() && !ret_val && !thd->check_clone_vio()) {
    auto err = ER_QUERY_INTERRUPTED;
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    ++m_num_errors;

    auto da = thd->get_stmt_da();
    ib::info(ER_IB_CLONE_SQL)
        << "Clone: Failed to " << sql_stmt << " task: " << thread_number
        << " code: " << err << ": "
        << ((da == nullptr || !da->is_error()) ? "" : da->message_text());
  }
  return (ret_val);
}

template <>
bool Fixup_data::fix_one_object(THD *thd, const dd::Schema *schema,
                                size_t thread_number) {
  const auto schema_name = schema->name().c_str();

  if (skip_schema(schema_name)) {
    return (false);
  }

  if (is_drop()) {
    auto ret_val =
        execute_sql(thd, schema_name, nullptr, nullptr, thread_number);
    return (ret_val);
  }

  /* Convert schema name to directory name to handle special characters. */
  char schema_dir[FN_REFLEN];
  static_cast<void>(
      tablename_to_filename(schema_name, schema_dir, sizeof(schema_dir)));
  MY_STAT stat_info;
  if (mysql_file_stat(key_file_misc, schema_dir, &stat_info, MYF(0)) !=
      nullptr) {
    /* Schema directory exists */
    return (false);
  }

  if (my_mkdir(schema_dir, 0777, MYF(0)) < 0) {
    ib::error(ER_IB_CLONE_INTERNAL)
        << "Clone: Failed to create schema directory: " << schema_name
        << " task: " << thread_number;
    ++m_num_errors;
    return (true);
  }

  ib::info(ER_IB_CLONE_SQL)
      << "Clone: Fixed Schema: " << schema_name << " task: " << thread_number;
  return (false);
}

template <>
bool Fixup_data::fix_one_object(THD *thd, const dd::Tablespace *tablespace,
                                size_t thread_number) {
  ut_ad(is_drop());

  if (skip_tablespace(thd, tablespace)) {
    return (false);
  }

  const auto tablespace_name = tablespace->name().c_str();

  auto ret_val =
      execute_sql(thd, nullptr, nullptr, tablespace_name, thread_number);
  return (ret_val);
}
} /* namespace */

bool fix_cloned_tables(THD *thd) {
  std::string fixup_file(CLONE_INNODB_FIXUP_FILE);

  /* Check if table fix up is needed. */
  if (!file_exists(fixup_file)) {
    return (false);
  }

  auto dc = dd::get_dd_client(thd);
  Releaser releaser(dc);

  Fixup_data clone_fixup(true, false);

  ib::info(ER_IB_CLONE_SQL) << "Clone Fixup: check and create schema directory";
  DD_Objs<dd::Schema> schemas;

  if (dc->fetch_global_components(&schemas) || clone_fixup.fix(thd, schemas)) {
    return (true);
  }

  ib::info(ER_IB_CLONE_SQL)
      << "Clone Fixup: create empty MyIsam and CSV tables";
  DD_Objs<dd::Table> tables;

  if (dc->fetch_global_components(&tables) || clone_fixup.fix(thd, tables)) {
    return (true);
  }

  ib::info(ER_IB_CLONE_SQL) << "Clone Fixup: replication configuration tables";
  if (clone_fixup.fix_config_tables(thd)) {
    return (true);
  }

  ib::info(ER_IB_CLONE_SQL) << "Clone Fixup: finished successfully";
  remove_file(fixup_file);
  return (false);
}

static bool clone_execute_query(THD *thd, const char *sql_stmt,
                                size_t thread_number, bool skip_error) {
  thd->set_query_id(next_query_id());

  /* We use the code from dd::excute_query here to capture the error. */
  Ed_connection con(thd);
  std::string query(sql_stmt);

  LEX_STRING str;
  lex_string_strmake(thd->mem_root, &str, query.c_str(), query.length());

  auto saved_thd_system = thd->system_thread;
  /* For visibility in SHOW PROCESS LIST during execute direct. */
  if (thd->system_thread == NON_SYSTEM_THREAD) {
    thd->system_thread = SYSTEM_THREAD_BACKGROUND;
  }

  if (con.execute_direct(str)) {
    thd->system_thread = saved_thd_system;
    auto sql_errno = con.get_last_errno();
    const char *sql_state = mysql_errno_to_sqlstate(sql_errno);
    const char *sql_errmsg = con.get_last_error();

    /* Skip error, if asked. Don't skip query interruption request. */
    if (skip_error && sql_errno != ER_QUERY_INTERRUPTED) {
      ib::info(ER_IB_CLONE_SQL)
          << "Clone: Skipped " << sql_stmt << " task: " << thread_number
          << " Reason = " << sql_errno << ": " << sql_errmsg;
      return (false);
    }

    ib::info(ER_IB_CLONE_SQL)
        << "Clone: Failed to " << sql_stmt << " task: " << thread_number
        << " code: " << sql_errno << ": " << sql_errmsg;

    /* Update the error to THD. */
    auto da = thd->get_stmt_da();
    if (da != nullptr) {
      da->set_overwrite_status(true);
      da->set_error_status(sql_errno, sql_errmsg, sql_state);
      da->push_warning(thd, sql_errno, sql_state, Sql_condition::SL_ERROR,
                       sql_errmsg);
      da->set_overwrite_status(false);
    }
    return (true);
  }

  thd->system_thread = saved_thd_system;
  return (false);
}

static int clone_drop_binary_logs(THD *thd) {
  int err = 0;
  /* No privilege check needed for individual tables. */
  auto saved_sctx = thd->security_context();
  Security_context sctx(*saved_sctx);
  skip_grants(thd, sctx);
  thd->set_security_context(&sctx);

  /* 1. Attempt to stop slaves if any. */
  char sql_stmt[FN_LEN + FN_LEN + 64];
  snprintf(sql_stmt, sizeof(sql_stmt), "STOP SLAVE");

  channel_map.rdlock();
  auto is_slave = is_slave_configured();
  channel_map.unlock();

  if (is_slave && clone_execute_query(thd, &sql_stmt[0], 1, false)) {
    err = ER_INTERNAL_ERROR;
    my_error(err, MYF(0), "Clone failed to stop slave");
  }

  if (err == 0) {
    /* Clear warnings if any. */
    thd->clear_error();

    /* 2. Clear all binary logs and GTID. */
    snprintf(sql_stmt, sizeof(sql_stmt), "RESET MASTER");

    if (clone_execute_query(thd, &sql_stmt[0], 1, false)) {
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Clone failed to reset binary logs");
    }
  }

  /* Set back old security context. */
  thd->set_security_context(saved_sctx);
  return (err);
}

static int clone_drop_user_data(THD *thd, bool allow_threads) {
  ib::warn(ER_IB_CLONE_USER_DATA, "Started");

  auto dc = dd::get_dd_client(thd);
  Releaser releaser(dc);
  Fixup_data clone_fixup(allow_threads, true);

  ib::info(ER_IB_CLONE_SQL) << "Clone Drop all user data";
  DD_Objs<dd::Table> tables;

  if (dc->fetch_global_components(&tables) || clone_fixup.fix(thd, tables)) {
    ib::info(ER_IB_CLONE_SQL) << "Clone failed to drop all user tables";
    my_error(ER_INTERNAL_ERROR, MYF(0), "Clone failed to drop all user tables");

    /* Get the first error reported. */
    auto da = thd->get_stmt_da();
    return (da->mysql_errno());
  }

  ib::info(ER_IB_CLONE_SQL) << "Clone Drop User schemas";
  DD_Objs<dd::Schema> schemas;

  if (dc->fetch_global_components(&schemas) || clone_fixup.fix(thd, schemas)) {
    ib::info(ER_IB_CLONE_SQL) << "Clone failed to drop all user schemas";
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Clone failed to drop all user schemas");

    /* Get the first error reported. */
    auto da = thd->get_stmt_da();
    return (da->mysql_errno());
  }

  ib::info(ER_IB_CLONE_SQL) << "Clone Drop User tablespaces";
  DD_Objs<dd::Tablespace> tablesps;

  if (dc->fetch_global_components(&tablesps) ||
      clone_fixup.fix(thd, tablesps)) {
    ib::info(ER_IB_CLONE_SQL) << "Clone failed to drop all user tablespaces";
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Clone failed to drop all user tablespaces");

    /* Get the first error reported. */
    auto da = thd->get_stmt_da();
    return (da->mysql_errno());
  }

  /* Clean binary logs after removing all user data. */
  if (!allow_threads) {
    auto err = clone_drop_binary_logs(thd);
    if (err != 0) {
      return (err);
    }
  }
  ib::info(ER_IB_CLONE_SQL) << "Clone Drop: finished successfully";
  ib::warn(ER_IB_CLONE_USER_DATA, "Finished");
  return (0);
}
