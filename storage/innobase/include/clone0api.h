/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file include/clone0api.h
 Innodb Clone Interface

 *******************************************************/

#ifndef CLONE_API_INCLUDE
#define CLONE_API_INCLUDE

#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "sql/handler.h"

/** Get capability flags for clone operation
@param[out]     flags   capability flag */
void innodb_clone_get_capability(Ha_clone_flagset &flags);

/** Begin copy from source database
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in,out]  loc     locator
@param[in,out]  loc_len locator length
@param[out]     task_id task identifier
@param[in]      type    clone type
@param[in]      mode    mode for starting clone
@return error code */
int innodb_clone_begin(handlerton *hton, THD *thd, const byte *&loc,
                       uint &loc_len, uint &task_id, Ha_clone_type type,
                       Ha_clone_mode mode);

/** Copy data from source database in chunks via callback
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in]      loc     locator
@param[in]      loc_len locator length in bytes
@param[in]      task_id task identifier
@param[in]      cbk     callback interface for sending data
@return error code */
int innodb_clone_copy(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                      uint task_id, Ha_clone_cbk *cbk);

/** Acknowledge data to source database
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in]      loc     locator
@param[in]      loc_len locator length in bytes
@param[in]      task_id task identifier
@param[in]      in_err  inform any error occurred
@param[in]      cbk     callback interface for receiving data
@return error code */
int innodb_clone_ack(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err, Ha_clone_cbk *cbk);

/** End copy from source database
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in]      loc     locator
@param[in]      loc_len locator length in bytes
@param[in]      task_id task identifier
@param[in]      in_err  error code when ending after error
@return error code */
int innodb_clone_end(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err);

/** Begin apply to destination database
@param[in]      hton            handlerton for SE
@param[in]      thd             server thread handle
@param[in,out]  loc             locator
@param[in,out]  loc_len         locator length
@param[out]     task_id         task identifier
@param[in]      mode            mode for starting clone
@param[in]      data_dir        target data directory
@return error code */
int innodb_clone_apply_begin(handlerton *hton, THD *thd, const byte *&loc,
                             uint &loc_len, uint &task_id, Ha_clone_mode mode,
                             const char *data_dir);

/** Apply data to destination database in chunks via callback
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in]      loc     locator
@param[in]      loc_len locator length in bytes
@param[in]      task_id task identifier
@param[in]      in_err  inform any error occurred
@param[in]      cbk     callback interface for receiving data
@return error code */
int innodb_clone_apply(handlerton *hton, THD *thd, const byte *loc,
                       uint loc_len, uint task_id, int in_err,
                       Ha_clone_cbk *cbk);

/** End apply to destination database
@param[in]      hton    handlerton for SE
@param[in]      thd     server thread handle
@param[in]      loc     locator
@param[in]      loc_len locator length in bytes
@param[in]      task_id task identifier
@param[in]      in_err  error code when ending after error
@return error code */
int innodb_clone_apply_end(handlerton *hton, THD *thd, const byte *loc,
                           uint loc_len, uint task_id, int in_err);

/** Check and delete any old list files. */
void clone_init_list_files();

/** Add file name to clone list file for future replacement or rollback.
@param[in]      list_file_name  list file name where to add the file
@param[in]      file_name       file name to add to the list
@return error code */
int clone_add_to_list_file(const char *list_file_name, const char *file_name);

/** Remove one of the clone list files.
@param[in]      file_name       list file name to delete */
void clone_remove_list_file(const char *file_name);

/** Revert back clone changes in case of an error. */
void clone_files_error();

#ifdef UNIV_DEBUG
/** Debug function to check and crash during recovery.
@param[in]      is_cloned_db    if cloned database recovery */
bool clone_check_recovery_crashpoint(bool is_cloned_db);
#endif

/** Change cloned file states during recovery.
@param[in]      finished        if recovery is finishing */
void clone_files_recovery(bool finished);

/** Update cloned GTIDs to recovery status file.
@param[in]      gtids   cloned GTIDs */
void clone_update_gtid_status(std::string &gtids);

/** Initialize Clone system
@return inndodb error code */
dberr_t clone_init();

/** Uninitialize Clone system */
void clone_free();

/** Check if active clone is running.
@return true, if any active clone is found. */
bool clone_check_active();

/** @return true, if clone provisioning in progress. */
bool clone_check_provisioning();
#endif /* !UNIV_HOTBACKUP */

/** Fix cloned non-Innodb tables during recovery.
@param[in,out]  thd     current THD
@return true if error */
bool fix_cloned_tables(THD *thd);

/** Clone Notification handler. */
class Clone_notify {
 public:
  /** Notification type. Currently used by various DDL commands. */
  enum class Type {
    /* Space is being created. */
    SPACE_CREATE,
    /* Space is being dropped. */
    SPACE_DROP,
    /* Space is being renamed. */
    SPACE_RENAME,
    /* Space is being discarded or imported. */
    SPACE_IMPORT,
    /* Space encryption property is altered. */
    SPACE_ALTER_ENCRYPT,
    /* Space encryption property of general tablespace is altered. */
    SPACE_ALTER_ENCRYPT_GENERAL,
    /* Space encryption flags of general tablespace are altered. */
    SPACE_ALTER_ENCRYPT_GENERAL_FLAGS,
    /* In place Alter general notification. */
    SPACE_ALTER_INPLACE,
    /* Inplace Alter bulk operation. */
    SPACE_ALTER_INPLACE_BULK,
    /* Special consideration is needed for UNDO as these DDLs
    don't use DDL log and needs special consideration during recovery. */
    SPACE_UNDO_DDL,
    /* Redo logging is being disabled. */
    SYSTEM_REDO_DISABLE
  };

#ifdef UNIV_HOTBACKUP
  Clone_notify(Type, space_id_t, bool) : m_error() {}
  ~Clone_notify() {}
#else
  /** Constructor to initiate notification.
  @param[in]    type    notification type
  @param[in]    space   tablespace ID for which notification is sent
  @param[in]    no_wait set error and return immediately if needs to wait */
  Clone_notify(Type type, space_id_t space, bool no_wait);

  /** Destructor to automatically end notification. */
  ~Clone_notify();
#endif /* UNIV_HOTBACKUP */

  /** Get notification message for printing.
  @param[in]    begin    true if notification begin otherwise end
  @param[out]   mesg    notification message */
  void get_mesg(bool begin, std::string &mesg);

  /** @return true iff notification failed. */
  bool failed() const { return m_error != 0; }

  /** @return saved error code. */
  int get_error() const { return m_error; }

  /** Disable copy construction */
  Clone_notify(Clone_notify &) = delete;

  /** Disable assignment */
  Clone_notify &operator=(Clone_notify const &) = delete;

 private:
  /** Notification wait type set. */
  enum class Wait_at {
    /* Clone doesn't need to wait. */
    NONE,
    /* Clone needs to wait before entering. */
    ENTER,
    /* Clone needs to wait before state change. */
    STATE_CHANGE,
    /* Clone needs to abort. */
    ABORT
  };

 private:
  /** Tablespace ID for which notification is sent. */
  space_id_t m_space_id;

  /** Notification type. */
  Type m_type;

  /** Wait type set. */
  Wait_at m_wait;

  /** Blocked clone state if clone is blocked. */
  uint32_t m_blocked_state;

  /** Saved error. */
  int m_error;
};

#endif /* CLONE_API_INCLUDE */
