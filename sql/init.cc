/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
  uint i;
  double nr;
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
  specialflag=SPECIAL_SAME_DB_NAME | options;  /* Set options from argv */
  /* Make a tab of powers of 10 */
  for (i=0,nr=1.0; i < array_elements(log_10) ; i++)
  {					/* It's used by filesort... */
    log_10[i]= nr ; nr*= 10.0;
  }
  DBUG_VOID_RETURN;
}
