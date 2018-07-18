/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "clone0api.h"
#include "clone0clone.h"

void innodb_clone_get_capability(Ha_clone_flagset &flags) {
  flags.reset();

  flags.set(HA_CLONE_HYBRID);
  flags.set(HA_CLONE_MULTI_TASK);
  flags.set(HA_CLONE_RESTART);
}

int innodb_clone_begin(handlerton *hton, THD *thd, const byte *&loc,
                       uint &loc_len, uint &task_id, Ha_clone_type type,
                       Ha_clone_mode mode) {
  Clone_Handle *clone_hdl;

  /* Encrypted redo or undo log clone is not supported */
  if (srv_redo_log_encrypt || srv_undo_log_encrypt) {
    if (thd != nullptr) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Clone Encrypted logs");
    }

    return (ER_NOT_SUPPORTED_YET);
  }

  /* Check if reference locator is valid */
  if (loc != nullptr && !clone_validate_locator(loc, loc_len)) {
    int err = ER_CLONE_PROTOCOL_ERROR;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Locator");
    return (err);
  }

  /* Acquire clone system mutex which would automatically get released
  when we return from the function [RAII]. */
  IB_mutex_guard sys_mutex(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    if (thd != nullptr) {
      my_error(ER_DDL_IN_PROGRESS, MYF(0));
    }

    return (ER_DDL_IN_PROGRESS);
  }

  /* Check of clone is already in progress for the reference locator. */
  clone_hdl = clone_sys->find_clone(loc, loc_len, CLONE_HDL_COPY);

  int err = 0;

  switch (mode) {
    case HA_CLONE_MODE_RESTART:
      /* Error out if existing clone is not found */
      if (clone_hdl == nullptr) {
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone Restart could not find existing clone");
        return (ER_INTERNAL_ERROR);
      }

      ib::info(ER_IB_MSG_151) << "Clone Begin Master Task: Restart";
      err = clone_hdl->restart_copy(thd, loc, loc_len);

      break;

    case HA_CLONE_MODE_START:
      /* Should not find existing clone for the locator */
      if (clone_hdl != nullptr) {
        clone_sys->drop_clone(clone_hdl);
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "Innodb Clone Begin refers existing clone");
        return (ER_INTERNAL_ERROR);
      }
      ib::info(ER_IB_MSG_151) << "Clone Begin Master Task";
      break;

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
    ib::info(ER_IB_MSG_151) << "Clone Begin Task ID: " << task_id;
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
    int err = ER_CLONE_PROTOCOL_ERROR;
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

  if (in_err == 0) {
    /* Apply acknowledged data */
    err = clone_hdl->apply(thd, task_id, cbk);

    clone_hdl->save_error(err);
  } else {
    /* For error input, return after saving it */
    ib::info(ER_IB_MSG_151) << "Clone set error ACK: " << in_err;
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
        ib::info(ER_IB_MSG_151) << "Clone Master aborted by concurrent clone";

      } else if (in_err != 0) {
        /* Make sure re-start attempt fails immediately */
        clone_hdl->set_state(CLONE_STATE_ABORT);
      }
    }
    clone_sys->drop_clone(clone_hdl);

    ib::info(ER_IB_MSG_151)
        << "Clone"
        << (is_copy ? " End" : (is_init ? "Apply Version End" : " Apply End"))
        << (is_master ? " Master" : "") << " Task ID: " << task_id
        << (in_err != 0 ? " Failed, code: " : " Passed, code: ") << in_err;
    return (0);
  }

  ib::info(ER_IB_MSG_151) << "Clone Master wait for restart"
                          << " after n/w error code: " << in_err;

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
          ib::info(ER_IB_MSG_151)
              << "Clone End Master wait for Restart interrupted";
          my_error(ER_QUERY_INTERRUPTED, MYF(0));
          return (ER_QUERY_INTERRUPTED);

        } else if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
          ib::info(ER_IB_MSG_151)
              << "Clone End Master wait for Restart aborted by DDL";
          my_error(ER_DDL_IN_PROGRESS, MYF(0));
          return (ER_DDL_IN_PROGRESS);

        } else if (clone_hdl->is_abort()) {
          result = false;
          ib::info(ER_IB_MSG_151) << "Clone End Master wait for Restart"
                                     " aborted by concurrent clone";
          return (0);
        }

        if (!result) {
          ib::info(ER_IB_MSG_151) << "Clone Master restarted successfully by "
                                     "other task after n/w failure";

        } else if (alert) {
          ib::info(ER_IB_MSG_151) << "Clone Master still waiting for restart";
        }
        return (0);
      },
      clone_sys->get_mutex(), is_timeout);

  if (err == 0 && is_timeout && clone_hdl->is_idle()) {
    ib::info(ER_IB_MSG_151) << "Clone End Master wait "
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
    int err = ER_CLONE_PROTOCOL_ERROR;
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
      ib::info(ER_IB_MSG_151) << "Clone Apply Begin Master Task: Restart";
      auto err = clone_hdl->restart_apply(thd, loc, loc_len);

      /* Reduce reference count */
      clone_sys->drop_clone(clone_hdl);

      /* Restart is done by master task */
      ut_ad(task_id == 0);
      task_id = 0;

      return (err);
    }
    case HA_CLONE_MODE_START:

      ut_ad(clone_hdl == nullptr);
      ib::info(ER_IB_MSG_151) << "Clone Apply Begin Master Task";
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
      ib::info(ER_IB_MSG_151) << "Clone Apply Begin Master Version Check";
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
    /* Add new task for the clone apply operation. */
    ut_ad(loc != nullptr);
    auto err = clone_hdl->add_task(thd, loc, loc_len, task_id);
    mutex_enter(clone_sys->get_mutex());

    if (err != 0) {
      clone_sys->drop_clone(clone_hdl);
      return (err);
    }
  } else {
    ut_ad(mode == HA_CLONE_MODE_VERSION);
  }

  if (task_id > 0) {
    ib::info(ER_IB_MSG_151) << "Clone Apply Begin Task ID: " << task_id;
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
    ib::info(ER_IB_MSG_151) << "Clone Apply set error code: " << in_err;
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

dberr_t clone_init() {
  /* Check if in complete cloned data directory */
  os_file_type_t type;
  bool exists = false;
  auto status = os_file_status(CLONE_IN_PROGRESS_FILE, &exists, &type);

  if (status && exists) {
    return (DB_ABORT_INCOMPLETE_CLONE);
  }

  if (clone_sys == nullptr) {
    ut_ad(Clone_Sys::s_clone_sys_state == CLONE_SYS_INACTIVE);
    clone_sys = UT_NEW(Clone_Sys(), mem_key_clone);
  }
  Clone_Sys::s_clone_sys_state = CLONE_SYS_ACTIVE;
  return (DB_SUCCESS);
}

void clone_free() {
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
