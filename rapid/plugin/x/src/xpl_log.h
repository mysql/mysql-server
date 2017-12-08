/*
 * Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _XPL_LOG_H_
#define _XPL_LOG_H_

#ifndef XPLUGIN_DISABLE_LOG

#include "plugin/x/generated/mysqlx_version.h"

#define LOG_SUBSYSTEM_TAG MYSQLX_PLUGIN_NAME

#include <mysqld_error.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/plugin.h>
#include <mysql/service_my_plugin_log.h>

namespace xpl
{

extern MYSQL_PLUGIN plugin_handle;

void plugin_log_message(MYSQL_PLUGIN *p, const plugin_log_level, const char *);

} // namespace xpl


#define log_error(...)\
  LogPluginErrMsg(ERROR_LEVEL, ER_XPLUGIN_ERROR_MSG, __VA_ARGS__)

#define log_warning(...)\
  LogPluginErrMsg(WARNING_LEVEL, ER_XPLUGIN_ERROR_MSG, __VA_ARGS__)

#define log_info(...)\
  LogPluginErrMsg(INFORMATION_LEVEL, ER_XPLUGIN_ERROR_MSG, __VA_ARGS__)

#ifdef XPLUGIN_LOG_DEBUG
#define log_debug(...) LogPluginErrMsg(INFORMATION_LEVEL, ER_XPLUGIN_ERROR_MSG,\
                                       __VA_ARGS__)
#else
#define log_debug(...) do {} while(0)
#endif


#else // XPLUGIN_DISABLE_LOG


#define log_debug(...) do {} while(0)
#define log_info(...) do {} while(0)
#define log_warning(...) do {} while(0)
#define log_error(...) do {} while(0)


#endif // XPLUGIN_DISABLE_LOG


#endif // _XPL_LOG_H_
