
/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_priv.h"
#include "rpl_reporting.h"
#include "log.h" // sql_print_error, sql_print_warning,
                 // sql_print_information

Slave_reporting_capability::Slave_reporting_capability(char const *thread_name)
  : m_thread_name(thread_name)
{
  mysql_mutex_init(key_mutex_slave_reporting_capability_err_lock,
                   &err_lock, MY_MUTEX_INIT_FAST);
}

void
Slave_reporting_capability::report(loglevel level, int err_code,
                                   const char *msg, ...) const
{
  void (*report_function)(const char *, ...);
  char buff[MAX_SLAVE_ERRMSG];
  char *pbuff= buff;
  uint pbuffsize= sizeof(buff);
  va_list args;
  va_start(args, msg);

  mysql_mutex_lock(&err_lock);
  switch (level)
  {
  case ERROR_LEVEL:
    /*
      It's an error, it must be reported in Last_error and Last_errno in SHOW
      SLAVE STATUS.
    */
    pbuff= m_last_error.message;
    pbuffsize= sizeof(m_last_error.message);
    m_last_error.number = err_code;
    report_function= sql_print_error;
    break;
  case WARNING_LEVEL:
    report_function= sql_print_warning;
    break;
  case INFORMATION_LEVEL:
    report_function= sql_print_information;
    break;
  default:
    DBUG_ASSERT(0);                            // should not come here
    return;          // don't crash production builds, just do nothing
  }

  my_vsnprintf(pbuff, pbuffsize, msg, args);

  mysql_mutex_unlock(&err_lock);
  va_end(args);

  /* If the msg string ends with '.', do not add a ',' it would be ugly */
  report_function("Slave %s: %s%s Error_code: %d",
                  m_thread_name, pbuff,
                  (pbuff[0] && *(strend(pbuff)-1) == '.') ? "" : ",",
                  err_code);
}

Slave_reporting_capability::~Slave_reporting_capability()
{
  mysql_mutex_destroy(&err_lock);
}
