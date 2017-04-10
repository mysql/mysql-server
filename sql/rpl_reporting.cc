/* Copyright (c) 2007, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "rpl_reporting.h"

#include <stddef.h>

#include "current_thd.h"
#include "log.h"               // sql_print_warning
#include "m_string.h"
#include "my_dbug.h"
#include "mysql/service_my_snprintf.h"
#include "mysqld.h"            // slave_trans_retries
#include "mysqld_error.h"
#include "sql_class.h"         // THD
#include "sql_error.h"         // Diagnostics_area
#include "thr_mutex.h"
#include "transaction_info.h"

Slave_reporting_capability::Slave_reporting_capability(char const *thread_name)
  : m_thread_name(thread_name)
{
  mysql_mutex_init(key_mutex_slave_reporting_capability_err_lock,
                   &err_lock, MY_MUTEX_INIT_FAST);
}

/**
  Check if the current error is of temporary nature or not.
  Some errors are temporary in nature, such as
  ER_LOCK_DEADLOCK and ER_LOCK_WAIT_TIMEOUT.  Ndb also signals
  that the error is temporary by pushing a warning with the error code
  ER_GET_TEMPORARY_ERRMSG, if the originating error is temporary.

  @param      thd  a THD instance, typically of the slave SQL thread's.
  @param error_arg  the error code for assessment. 
              defaults to zero which makes the function check the top
              of the reported errors stack.
  @param silent     bool indicating whether the error should be silently handled.

  @return 1 as the positive and 0 as the negative verdict
*/
int Slave_reporting_capability::has_temporary_error(THD *thd, 
                                                    uint error_arg, bool* silent) const
{
  uint error;
  DBUG_ENTER("has_temporary_error");

  DBUG_EXECUTE_IF("all_errors_are_temporary_errors",
                  if (thd->get_stmt_da()->is_error())
                  {
                    thd->clear_error();
                    my_error(ER_LOCK_DEADLOCK, MYF(0));
                  });

  /*
    The slave can't be regarded as experiencing a temporary failure in cases of
    is_fatal_error is TRUE, or if no error is in THD and error_arg is not set.
  */
  if (thd->is_fatal_error || (!thd->is_error() && error_arg == 0))
    DBUG_RETURN(0);

  error= (error_arg == 0)? thd->get_stmt_da()->mysql_errno() : error_arg;

  /*
    Temporary error codes:
    currently, InnoDB deadlock detected by InnoDB or lock
    wait timeout (innodb_lock_wait_timeout exceeded).
    Notice, the temporary error requires slave_trans_retries != 0)
  */
  if (slave_trans_retries &&
      (error == ER_LOCK_DEADLOCK || error == ER_LOCK_WAIT_TIMEOUT))
    DBUG_RETURN(1);

  /*
    currently temporary error set in ndbcluster
  */
  Diagnostics_area::Sql_condition_iterator it=
    thd->get_stmt_da()->sql_conditions();
  const Sql_condition *err;
  while ((err= it++))
  {
    DBUG_PRINT("info", ("has condition %d %s", err->mysql_errno(),
                        err->message_text()));
    switch (err->mysql_errno())
    {
    case ER_GET_TEMPORARY_ERRMSG:
      DBUG_RETURN(1);
    case ER_SLAVE_SILENT_RETRY_TRANSACTION:
    {
      if (silent != NULL)
        *silent= true;
      DBUG_RETURN(1);
    }
    default:
      break;
    }
  }
  DBUG_RETURN(0);
}


void
Slave_reporting_capability::report(loglevel level, int err_code,
                                   const char *msg, ...) const
{
  va_list args;
  va_start(args, msg);
  do_report(level, err_code, msg, args);
  va_end(args);
}

void
Slave_reporting_capability::va_report(loglevel level, int err_code,
                                      const char *prefix_msg,
                                      const char *msg, va_list args) const
{
  THD *thd= current_thd;
  void (*report_function)(const char *, ...);
  char buff[MAX_SLAVE_ERRMSG];
  char *pbuff= buff;
  char *curr_buff;
  uint pbuffsize= sizeof(buff);

  if (thd && level == ERROR_LEVEL && has_temporary_error(thd, err_code) &&
      !thd->get_transaction()->cannot_safely_rollback(
          Transaction_ctx::SESSION))
    level= WARNING_LEVEL;

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
    m_last_error.update_timestamp();
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
  curr_buff= pbuff;
  if (prefix_msg)
    curr_buff += my_snprintf(curr_buff, pbuffsize, "%s; ", prefix_msg);
  my_vsnprintf(curr_buff, pbuffsize - (curr_buff - pbuff), msg, args);

  mysql_mutex_unlock(&err_lock);

  /* If the msg string ends with '.', do not add a ',' it would be ugly */
  report_function("Slave %s%s: %s%s Error_code: %d",
                  m_thread_name, get_for_channel_str(false), pbuff,
                  (curr_buff[0] && *(strend(curr_buff)-1) == '.') ? "" : ",",
                  err_code);
}

Slave_reporting_capability::~Slave_reporting_capability()
{
  mysql_mutex_destroy(&err_lock);
}
