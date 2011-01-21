/*  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
    
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
  @file
  This service provides functions to report error conditions and log to 
  mysql error log.
*/

#ifndef MYSQL_SERVICE_ERROR_REPORTING_INCLUDED
#define MYSQL_SERVICE_ERROR_REPORTING_INCLUDED

#ifndef MYSQL_ABI_CHECK
#include <stdarg.h>
#endif

/* Max length of a error message. Should be kept in sync with MYSQL_ERRMSG_SIZE. */
#define ERRMSGSIZE      (512)

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

extern struct error_reporting_service
{
  /** send an error back to the user */
  void (*error)(void *, int, const char *, ...);
  /** write a message to the log */
  int (*log_message)(void *, enum plugin_log_level, const char *, ...);
} *error_reporting_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_plugin_error error_reporting_service->error
#define my_plugin_log_message error_reporting_service->log_message

#else

void my_plugin_error(void *plugin, int code, const char *format, ...);
int my_plugin_log_message(void *plugin, enum plugin_log_level level,
                          const char *format, ...);

#endif

#ifdef __cplusplus
}
#endif

#endif
