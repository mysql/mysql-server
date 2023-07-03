/*****************************************************************************

Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

*****************************************************************************/
#ifndef log0meb_h
#define log0meb_h

#include "univ.i"

class THD;
struct SYS_VAR;
struct st_mysql_value;
class innodb_session_t;

namespace meb {

/** Performance schema key for the log consumer thread. */
extern mysql_pfs_key_t redo_log_archive_consumer_thread_key;

/** Performance schema key for the redo log archive file. */
extern mysql_pfs_key_t redo_log_archive_file_key;

/* The innodb_redo_log_archive_dirs plugin variable value. */
extern char *redo_log_archive_dirs;

/**
  Check whether a valid value is given to innodb_redo_log_archive_dirs.
  This function is registered as a callback with MySQL.
  @param[in]    thd       thread handle
  @param[in]    var       pointer to system variable
  @param[out]   save      immediate result for update function
  @param[in]    value     incoming string
  @return 0 for valid contents
*/
int validate_redo_log_archive_dirs(THD *thd [[maybe_unused]],
                                   SYS_VAR *var [[maybe_unused]], void *save,
                                   st_mysql_value *value);

/**
  Initialize redo log archiving.
  To be called when the InnoDB handlerton is initialized.
*/
extern void redo_log_archive_init();

/**
  De-initialize redo log archiving.
  To be called when the InnoDB handlerton is de-initialized.
*/
extern void redo_log_archive_deinit();

/**
  Security function to be called when the current session ends. This
  function invokes the stop implementation if this session has started
  the redo log archiving. It is a safe-guard against an infinitely
  active redo log archiving if the client goes away without
  deactivating the logging explicitly.

  @param[in]      session       the current ending session
*/
extern void redo_log_archive_session_end(innodb_session_t *session);

/**
  The producer produces full QUEUE_BLOCK_SIZE redo log blocks. These
  log blocks are enqueued, and are later fetched by the consumer
  thread.

  This function does nothing, if redo log archiving is not active.

  In order to produce full QUEUE_BLOCK_SIZE redo log blocks, the
  producer scans each OS_FILE_LOG_BLOCK_SIZE log block (written by the
  server) to check if they are,

  1. empty
  2. incomplete

  The producer skips empty and incomplete log blocks, unless they
  belong to the last flush, when the contents of its buffer are
  completely enqueued for flushing.

  @param[in]      write_buf     The write buffer that is being written
                                to the redo log archive file.
  @param[in]      write_size    The size of the data being written.
*/
extern void redo_log_archive_produce(const byte *write_buf,
                                     const size_t write_size);

/**
  @return true iff redo log archiving is active.
*/
extern bool redo_log_archive_is_active();

/**
  Register a privilege. We should move this function and other UDF
  registration functions to some common utility file later.
  @param[in]      priv_name     privilege name
  @return         status
    @retval       false         success
    @retval       true          failure
*/
extern bool register_privilege(const char *priv_name);

}  // namespace meb

#endif /* !log0meb_h */
