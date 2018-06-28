/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_XPL_LOG_H_
#define PLUGIN_X_SRC_XPL_LOG_H_

#ifndef XPLUGIN_DISABLE_LOG

#include "plugin/x/generated/mysqlx_version.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/plugin.h>
#include <mysql/service_my_plugin_log.h>
#include <mysqld_error.h>

namespace xpl {

extern MYSQL_PLUGIN plugin_handle;

void plugin_log_message(MYSQL_PLUGIN *p, const plugin_log_level, const char *);

}  // namespace xpl

#define log_error(errcode, ...) \
  LogPluginErr(ERROR_LEVEL, errcode, ##__VA_ARGS__)

#define log_warning(errcode, ...) \
  LogPluginErr(WARNING_LEVEL, errcode, ##__VA_ARGS__)

#define log_info(errcode, ...) \
  LogPluginErr(INFORMATION_LEVEL, errcode, ##__VA_ARGS__)

#define log_system(errcode, ...) LogErr(SYSTEM_LEVEL, errcode, ##__VA_ARGS__)

#ifdef XPLUGIN_LOG_DEBUG
#define log_debug(...) \
  LogPluginErrMsg(INFORMATION_LEVEL, ER_XPLUGIN_ERROR_MSG, ##__VA_ARGS__)
#else
#define log_debug(...) \
  do {                 \
  } while (0)
#endif

#else  // XPLUGIN_DISABLE_LOG

#define log_debug(...) \
  do {                 \
  } while (0)
#define log_info(...) \
  do {                \
  } while (0)
#define log_warning(...) \
  do {                   \
  } while (0)
#define log_error(...) \
  do {                 \
  } while (0)
#define log_system(...) \
  do {                  \
  } while (0)

#endif  // XPLUGIN_DISABLE_LOG

#endif  // PLUGIN_X_SRC_XPL_LOG_H_
