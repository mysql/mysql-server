/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

/* Define some external variables for error handling */

/*
  WARNING!
  my_error family functions have to be used according following rules:
  - if message have not parameters use my_message(ER_CODE, ER(ER_CODE), MYF(N))
  - if message registered use my_error(ER_CODE, MYF(N), ...).
  - With some special text of errror message use:
  my_printf_error(ER_CODE, format, MYF(N), ...)
*/

const char ** NEAR my_errmsg[MAXMAPS]={0,0,0,0};
char NEAR errbuff[NRERRBUFFS][ERRMSGSIZE];

/*
   Error message to user

   SYNOPSIS
     my_error()
       nr	Errno
       MyFlags	Flags
       ...	variable list
   NOTE
    The following subset of printf format is supported:
    "%[0-9.-]*l?[sdu]", where all length flags are parsed but ignored.

    Additionally "%.*s" is supported and "%.*[ud]" is correctly parsed but
    the length value is ignored.
*/

int my_error(int nr, myf MyFlags, ...)
{
  const char *format;
  va_list args;
  char ebuff[ERRMSGSIZE + 20];
  DBUG_ENTER("my_error");

  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d", nr, MyFlags, errno));

  if (nr / ERRMOD == GLOB && my_errmsg[GLOB] == 0)
    init_glob_errs();
  format= my_errmsg[nr / ERRMOD][nr % ERRMOD];

  va_start(args,MyFlags);
  (void) my_vsnprintf (ebuff, sizeof(ebuff), format, args);
  va_end(args);
  DBUG_RETURN((*error_handler_hook)(nr, ebuff, MyFlags));
}

/*
  Error as printf

  SYNOPSIS
    my_printf_error()
      error	Errno
      format	Format string
      MyFlags	Flags
      ...	variable list
*/

int my_printf_error(uint error, const char *format, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE+20];
  DBUG_ENTER("my_printf_error");
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d  Format: %s",
		    error, MyFlags, errno, format));

  va_start(args,MyFlags);
  (void) my_vsnprintf (ebuff, sizeof(ebuff), format, args);
  va_end(args);
  DBUG_RETURN((*error_handler_hook)(error, ebuff, MyFlags));
}

/*
  Give message using error_handler_hook

  SYNOPSIS
    my_message()
      error	Errno
      str	Error message
      MyFlags	Flags
*/

int my_message(uint error, const char *str, register myf MyFlags)
{
  return (*error_handler_hook)(error, str, MyFlags);
}
