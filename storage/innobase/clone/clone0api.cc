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

/** Begin copy from source database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in,out]	loc	locator
@param[in,out]	loc_len	locator length
@param[in]	type	clone type
@return error code */
int innodb_clone_begin(handlerton *hton, THD *thd, byte *&loc, uint &loc_len,
                       Ha_clone_type type) {
  Clone_Handle *clone_hdl;
  dberr_t err = DB_SUCCESS;

#ifdef UNIV_DEBUG
  bool new_clone = false;
#endif /* UNIV_DEBUG */

  /* Encrypted redo or undo log clone is not supported */
  if (srv_redo_log_encrypt || srv_undo_log_encrypt) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Clone Encrypted logs");

    return (ER_NOT_SUPPORTED_YET);
  }

  mutex_enter(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    mutex_exit(clone_sys->get_mutex());

    my_error(ER_DDL_IN_PROGRESS, MYF(0));
    return (ER_DDL_IN_PROGRESS);
  }

  /* Check of clone is already in progress for the reference locator. */
  clone_hdl = clone_sys->find_clone(loc, CLONE_HDL_COPY);

  if (clone_hdl == nullptr) {
    ut_d(new_clone = true);

    /* Create new clone handle for copy. Reference locator
    is used for matching the version. */
    clone_hdl = clone_sys->add_clone(loc, CLONE_HDL_COPY);

    if (clone_hdl == nullptr) {
      mutex_exit(clone_sys->get_mutex());
      return (-1);
    }

    err = clone_hdl->init(loc, type, nullptr);

    if (err != DB_SUCCESS) {
      clone_sys->drop_clone(clone_hdl);
      mutex_exit(clone_sys->get_mutex());
      return (-1);
    }
  }

  /* Add new task for the clone copy operation. */
  err = clone_hdl->add_task();

  mutex_exit(clone_sys->get_mutex());

  if (err != DB_SUCCESS) {
    /* Cannot fail to add first task to a newly created clone. */
    ut_ad(new_clone == false);

    return (-1);
  }

  /* Get the current locator from clone handle. */
  loc = clone_hdl->get_locator(loc_len);

  return (0);
}

/** Copy data from source database in chunks via callback
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	cbk	callback interface for sending data
@return error code */
int innodb_clone_copy(handlerton *hton, THD *thd, byte *loc,
                      Ha_clone_cbk *cbk) {
  dberr_t err;
  Clone_Handle *clone_hdl;

  cbk->set_hton(hton);

  mutex_enter(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    mutex_exit(clone_sys->get_mutex());

    my_error(ER_DDL_IN_PROGRESS, MYF(0));
    return (ER_DDL_IN_PROGRESS);
  }

  mutex_exit(clone_sys->get_mutex());

  /* Get clone handle by locator index. */
  clone_hdl = clone_sys->get_clone_by_index(loc);

  /* Start data copy. */
  err = clone_hdl->copy(cbk);

  return (err == DB_SUCCESS ? 0 : -1);
}

/** End copy from source database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@return error code */
int innodb_clone_end(handlerton *hton, THD *thd, byte *loc) {
  Clone_Handle *clone_hdl;
  uint num_tasks;

  /* Get clone handle by locator index. */
  clone_hdl = clone_sys->get_clone_by_index(loc);

  mutex_enter(clone_sys->get_mutex());

  /* Drop current task. */
  num_tasks = clone_hdl->drop_task();

  if (num_tasks == 0) {
    /* Last task should drop the clone handle. */
    clone_sys->drop_clone(clone_hdl);
  }

  mutex_exit(clone_sys->get_mutex());

  return (0);
}

/** Begin apply to destination database
@param[in]	hton		handlerton for SE
@param[in]	thd		server thread handle
@param[in,out]	loc		locator
@param[in,out]	loc_len		locator length
@param[in]	data_dir	target data directory
@return error code */
int innodb_clone_apply_begin(handlerton *hton, THD *thd, byte *&loc,
                             uint &loc_len, const char *data_dir) {
  Clone_Handle *clone_hdl;
  dberr_t err;
  char errbuf[MYSYS_STRERROR_SIZE];
  char schema_dir[FN_REFLEN + 16];

  /* Create data directory for clone. */
  err = os_file_create_subdirs_if_needed(data_dir);

  if (err == DB_SUCCESS) {
    bool status;

    status = os_file_create_directory(data_dir, false);

    /* Create mysql schema directory. */
    if (status) {
      snprintf(schema_dir, FN_REFLEN + 16, "%s%cmysql", data_dir,
               OS_PATH_SEPARATOR);

      status = os_file_create_directory(schema_dir, true);
    }

    if (!status) {
      err = DB_ERROR;
    }
  }

  if (err != DB_SUCCESS) {
    my_error(ER_CANT_CREATE_DB, MYF(0), data_dir, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_CANT_CREATE_DB);
  }

#ifdef UNIV_DEBUG
  bool new_clone = false;
#endif /* UNIV_DEBUG */

  mutex_enter(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    mutex_exit(clone_sys->get_mutex());

    my_error(ER_DDL_IN_PROGRESS, MYF(0));
    return (ER_DDL_IN_PROGRESS);
  }

  /* Check of clone is already in progress for the reference locator. */
  clone_hdl = clone_sys->find_clone(loc, CLONE_HDL_APPLY);

  if (clone_hdl == nullptr) {
    ut_d(new_clone = true);

    /* Create new clone handle for apply. Reference locator
    is used for matching the version. */
    clone_hdl = clone_sys->add_clone(loc, CLONE_HDL_APPLY);

    if (clone_hdl == nullptr) {
      mutex_exit(clone_sys->get_mutex());
      return (-1);
    }

    err = clone_hdl->init(loc, HA_CLONE_BLOCKING, data_dir);

    if (err != DB_SUCCESS) {
      clone_sys->drop_clone(clone_hdl);
      mutex_exit(clone_sys->get_mutex());
      return (-1);
    }
  }

  if (clone_hdl->is_active()) {
    ut_ad(loc != nullptr);
    /* Add new task for the clone apply operation. */
    err = clone_hdl->add_task();
  }

  mutex_exit(clone_sys->get_mutex());

  if (err != DB_SUCCESS) {
    /* Cannot fail to add first task to a newly created clone. */
    ut_ad(new_clone == false);
    return (-1);
  }

  /* Get the current locator from clone handle. */
  loc = clone_hdl->get_locator(loc_len);

  return (0);
}

/** Apply data to destination database in chunks via callback
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	cbk	callback interface for receiving data
@return error code */
int innodb_clone_apply(handlerton *hton, THD *thd, byte *loc,
                       Ha_clone_cbk *cbk) {
  dberr_t err;
  Clone_Handle *clone_hdl;

  cbk->set_hton(hton);

  mutex_enter(clone_sys->get_mutex());

  /* Check if concurrent ddl has marked abort. */
  if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    mutex_exit(clone_sys->get_mutex());

    my_error(ER_DDL_IN_PROGRESS, MYF(0));
    return (ER_DDL_IN_PROGRESS);
  }

  mutex_exit(clone_sys->get_mutex());

  /* Get clone handle by locator index. */
  clone_hdl = clone_sys->get_clone_by_index(loc);

  /* Apply data received from callback. */
  err = clone_hdl->apply(cbk);

  return (err == DB_SUCCESS ? 0 : -1);
}

/** End apply to destination database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@return error code */
int innodb_clone_apply_end(handlerton *hton, THD *thd, byte *loc) {
  int err;

  err = innodb_clone_end(hton, thd, loc);
  return (err);
}

/** Initialize Clone system */
void clone_init() {
  if (clone_sys == nullptr) {
    ut_ad(Clone_Sys::s_clone_sys_state == CLONE_SYS_INACTIVE);
    clone_sys = UT_NEW(Clone_Sys(), mem_key_clone);
  }
  Clone_Sys::s_clone_sys_state = CLONE_SYS_ACTIVE;
}

/** Uninitialize Clone system */
void clone_free() {
  if (clone_sys != nullptr) {
    ut_ad(Clone_Sys::s_clone_sys_state == CLONE_SYS_ACTIVE);

    UT_DELETE(clone_sys);
    clone_sys = nullptr;
  }

  Clone_Sys::s_clone_sys_state = CLONE_SYS_INACTIVE;
}

/** Mark clone system for abort to disallow database clone
@param[in]	force	abort running database clones
@return true if successful. */
bool clone_mark_abort(bool force) {
  bool aborted;

  mutex_enter(clone_sys->get_mutex());

  aborted = clone_sys->mark_abort(force);

  mutex_exit(clone_sys->get_mutex());

  DEBUG_SYNC_C("clone_marked_abort2");

  return (aborted);
}

/** Mark clone system as active to allow database clone. */
void clone_mark_active() {
  mutex_enter(clone_sys->get_mutex());

  clone_sys->mark_active();

  mutex_exit(clone_sys->get_mutex());
}
