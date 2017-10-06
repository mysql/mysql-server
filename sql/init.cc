/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  Init and dummy functions for interface with unireg
*/

#include "sql/init.h"

#include "my_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "m_string.h"                  // my_stpcpy
#include "my_dbug.h"
#include "my_sys.h"
#include "my_time.h"                   // my_init_time
#include "sql/mysqld.h"                // connection_events_loop_aborted(), ...

#ifdef _WIN32
#include <process.h> // getpid
#endif

void unireg_init(ulong options)
{
  DBUG_ENTER("unireg_init");

  error_handler_hook = my_message_stderr;
  set_connection_events_loop_aborted(false);

  current_pid=(ulong) getpid();		/* Save for later ref */
  my_init_time();			/* Init time-functions (read zone) */

  (void) my_stpcpy(reg_ext,".frm");
  reg_ext_length= 4;
  specialflag= options;  /* Set options from argv */
  DBUG_VOID_RETURN;
}
