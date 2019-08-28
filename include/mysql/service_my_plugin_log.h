/*  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
    
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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file
  This service provides functions to report error conditions and log to 
  mysql error log.
*/

#ifndef MYSQL_SERVICE_MY_PLUGIN_LOG_INCLUDED
#define MYSQL_SERVICE_MY_PLUGIN_LOG_INCLUDED

#ifndef MYSQL_ABI_CHECK
#include <stdarg.h>
#endif

/* keep in sync with the loglevel enum in my_sys.h */
enum plugin_log_level
{
  MY_ERROR_LEVEL,
  MY_WARNING_LEVEL,
  MY_INFORMATION_LEVEL
};  


#ifdef __cplusplus
extern "C" {
#endif

extern struct my_plugin_log_service
{
  /** write a message to the log */
  int (*my_plugin_log_message)(MYSQL_PLUGIN *, enum plugin_log_level, const char *, ...);
} *my_plugin_log_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_plugin_log_message my_plugin_log_service->my_plugin_log_message

#else

int my_plugin_log_message(MYSQL_PLUGIN *plugin, enum plugin_log_level level,
                          const char *format, ...);

#endif

#ifdef __cplusplus
}
#endif

#endif
