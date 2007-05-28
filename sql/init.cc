/* Copyright (C) 2000-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Init and dummy functions for interface with unireg */

#include "mysql_priv.h"
#include <m_ctype.h>

void unireg_init(ulong options)
{
  DBUG_ENTER("unireg_init");

  MYSYS_PROGRAM_DONT_USE_CURSES();
  abort_loop=0;

  my_disable_async_io=1;		/* aioread is only in shared library */
  wild_many='%'; wild_one='_'; wild_prefix='\\'; /* Change to sql syntax */

  current_pid=(ulong) getpid();		/* Save for later ref */
  init_time();				/* Init time-functions (read zone) */
#ifndef EMBEDDED_LIBRARY
  my_abort_hook=unireg_abort;		/* Abort with close of databases */
#endif

  VOID(strmov(reg_ext,".frm"));
  reg_ext_length= 4;
  specialflag=SPECIAL_SAME_DB_NAME | options;  /* Set options from argv */
  DBUG_VOID_RETURN;
}
