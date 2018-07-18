/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone_handler.cc
Clone handler implementation
*/

#include "sql/clone_handler.h"

#include <string.h>

#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin.h"
#include "mysql/plugin_clone.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysqld_error.h"
#include "sql/mysqld.h"
#include "sql/sql_parse.h"
#include "sql/sql_plugin.h"  // plugin_unlock
#include "sql_string.h"      // to_lex_cstring

class THD;

/** Clone handler global */
Clone_handler *clone_handle = nullptr;

/** Clone plugin name */
const char *clone_plugin_nm = "clone";

int Clone_handler::clone_local(THD *thd, const char *data_dir) {
  int error;
  char dir_name[FN_REFLEN];

  error = validate_dir(data_dir, dir_name);

  if (error == 0) {
    error = m_plugin_handle->clone_local(thd, dir_name);
  }

  return error;
}

int Clone_handler::clone_remote_client(THD *thd, const char *remote_host,
                                       uint remote_port,
                                       const char *remote_user,
                                       const char *remote_passwd,
                                       const char *data_dir,
                                       enum mysql_ssl_mode ssl_mode) {
  int error;
  char dir_name[FN_REFLEN];

  error = validate_dir(data_dir, dir_name);

  int mode = static_cast<int>(ssl_mode);

  if (error == 0) {
    error = m_plugin_handle->clone_client(thd, remote_host, remote_port,
                                          remote_user, remote_passwd, dir_name,
                                          mode);
  }

  return error;
}

int Clone_handler::clone_remote_server(THD *thd, MYSQL_SOCKET socket) {
  auto err = m_plugin_handle->clone_server(thd, socket);

  return err;
}

int Clone_handler::init() {
  plugin_ref plugin;

  plugin = my_plugin_lock_by_name(0, to_lex_cstring(m_plugin_name.c_str()),
                                  MYSQL_CLONE_PLUGIN);
  if (plugin == nullptr) {
    m_plugin_handle = nullptr;
    LogErr(ERROR_LEVEL, ER_CLONE_PLUGIN_NOT_LOADED);
    return 1;
  }

  m_plugin_handle = (Mysql_clone *)plugin_decl(plugin)->info;
  plugin_unlock(0, plugin);

  return 0;
}

int Clone_handler::validate_dir(const char *in_dir, char *out_dir) {
  MY_STAT stat_info;

  /* Verify that it is absolute path. */
  if (test_if_hard_path(in_dir) == 0) {
    my_error(ER_WRONG_VALUE, MYF(0), "path", in_dir);
    return ER_WRONG_VALUE;
  }

  /* Verify that the length is not too long. */
  if (strlen(in_dir) >= FN_REFLEN - 1) {
    my_error(ER_PATH_LENGTH, MYF(0), "DATA DIRECTORY");
    return ER_PATH_LENGTH;
  }

  /* Convert the path to native os format. */
  convert_dirname(out_dir, in_dir, nullptr);

  /* Check if the data directory exists already. */
  if (mysql_file_stat(key_file_misc, out_dir, &stat_info, MYF(0)) != nullptr) {
    my_error(ER_DB_CREATE_EXISTS, MYF(0), in_dir);
    return ER_DB_CREATE_EXISTS;
  }

  /* Check if path is within current data directory */
  char tmp_dir[FN_REFLEN + 1];
  size_t length;

  strncpy(tmp_dir, out_dir, FN_REFLEN);
  length = strlen(out_dir);

  /* Loop and remove all non-existent directories from the tail */
  while (length != 0) {
    /* Check if directory exists. */
    if (mysql_file_stat(key_file_misc, tmp_dir, &stat_info, MYF(0)) !=
        nullptr) {
      /* Check if the path is not within data directory. */
      if (test_if_data_home_dir(tmp_dir)) {
        my_error(ER_PATH_IN_DATADIR, MYF(0), in_dir);
        return ER_PATH_IN_DATADIR;
      }

      break;
    }

    size_t new_length;

    /* Remove the last directory separator from string */
    tmp_dir[length - 1] = '\0';
    dirname_part(tmp_dir, tmp_dir, &new_length);

    /* length must always decrease for the loop to terminate */
    if (length <= new_length) {
      DBUG_ASSERT(false);
      break;
    }

    length = new_length;
  }

  return 0;
}

int clone_handle_create(const char *plugin_name) {
  if (clone_handle != nullptr) {
    LogErr(ERROR_LEVEL, ER_CLONE_HANDLER_EXISTS);
    return 1;
  }

  clone_handle = new Clone_handler(plugin_name);

  if (clone_handle == nullptr) {
    LogErr(ERROR_LEVEL, ER_FAILED_TO_CREATE_CLONE_HANDLER);
    return 1;
  }

  return clone_handle->init();
}

int clone_handle_drop() {
  if (clone_handle == nullptr) {
    return 1;
  }

  delete clone_handle;

  clone_handle = nullptr;

  return 0;
}

Clone_handler *clone_plugin_lock(THD *thd, plugin_ref *plugin) {
  *plugin = my_plugin_lock_by_name(thd, to_lex_cstring(clone_plugin_nm),
                                   MYSQL_CLONE_PLUGIN);

  mysql_mutex_lock(&LOCK_plugin);

  /* Return handler only if the plugin is ready. We might successfully
  lock the plugin when initialization is progress. */
  if (*plugin != nullptr && plugin_state(*plugin) == PLUGIN_IS_READY) {
    mysql_mutex_unlock(&LOCK_plugin);

    DBUG_ASSERT(clone_handle != nullptr);
    return clone_handle;
  }

  mysql_mutex_unlock(&LOCK_plugin);

  return nullptr;
}

void clone_plugin_unlock(THD *thd, plugin_ref plugin) {
  plugin_unlock(thd, plugin);
}
