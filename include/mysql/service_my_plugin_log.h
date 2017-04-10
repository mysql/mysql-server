/*  Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file include/mysql/service_my_plugin_log.h
  This service provides functions to report error conditions and log to 
  mysql error log.
*/

#ifndef MYSQL_SERVICE_MY_PLUGIN_LOG_INCLUDED
#define MYSQL_SERVICE_MY_PLUGIN_LOG_INCLUDED

#ifndef MYSQL_ABI_CHECK
#include <stdarg.h>
#endif

#if defined __SUNPRO_C || defined __SUNPRO_CC || defined _MSC_VER
# define MY_ATTRIBUTE(A)
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

/**
   @ingroup group_ext_plugin_services

   Enables plugins to log messages into the server's error log.
*/
extern struct my_plugin_log_service
{
  /** Write a message to the log */
  int (*my_plugin_log_message)(MYSQL_PLUGIN *, enum plugin_log_level, const char *, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)));
} *my_plugin_log_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_plugin_log_message my_plugin_log_service->my_plugin_log_message

#else

int my_plugin_log_message(MYSQL_PLUGIN *plugin, enum plugin_log_level level,
                          const char *format, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));

#endif

#ifdef __cplusplus
}
#endif

#endif
