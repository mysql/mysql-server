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
  - if message have only integer parameters, string constants (created
  inside program) or string put (and cut if it is need) in some limited
  length buffer before passing it as parameter then you can use
  my_error(ER_CODE, MYF(N), ...). Never pass string get from user to
  my_error.
  - in all other cases use my_printf_error(ER_CODE, ER(ER_CODE), MYF(N), ...)
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

int my_error(int nr,myf MyFlags, ...)
{
  va_list	ap;
  uint		olen, plen;
  reg1 const char *tpos;
  reg2 char	*endpos;
  char		* par;
  char		ebuff[ERRMSGSIZE+20];
  int           prec_chars; /* output precision */
  my_bool       prec_supplied;
  DBUG_ENTER("my_error");
  LINT_INIT(prec_chars); /* protected by prec_supplied */

  va_start(ap,MyFlags);
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d", nr, MyFlags, errno));

  if (nr / ERRMOD == GLOB && my_errmsg[GLOB] == 0)
    init_glob_errs();

  olen=(uint) strlen(tpos=my_errmsg[nr / ERRMOD][nr % ERRMOD]);
  endpos=ebuff;

  while (*tpos)
  {
    if (tpos[0] != '%')
    {
      *endpos++= *tpos++;	/* Copy ordinary char */
      continue;
    }
    if (*++tpos == '%')		/* test if %% */
    {
      olen--;
    }
    else
    {
      /*
        Skip size/precision flags to be compatible with printf.
        The only size/precision flag supported is "%.*s".
        If "%.*u" or "%.*d" are encountered, the precision number is read
        from the variable argument list but its value is ignored.
      */
      prec_supplied= 0;
      if (*tpos== '.')
      {
        tpos++;
        olen--;
        if (*tpos == '*')
        {
          tpos++;
          olen--;
          prec_chars= va_arg(ap, int); /* get length parameter */
          prec_supplied= 1;
        }
      }

      if (!prec_supplied)
      {
        while (my_isdigit(&my_charset_latin1, *tpos) || *tpos == '.' ||
               *tpos == '-')
	  tpos++;

        if (*tpos == 'l')				/* Skip 'l' argument */
	  tpos++;
      }

      if (*tpos == 's')				/* String parameter */
      {
	par= va_arg(ap, char *);
	plen= (uint) strlen(par);
        if (prec_supplied && prec_chars > 0)
          plen= min((uint)prec_chars, plen);
	if (olen + plen < ERRMSGSIZE+2)		/* Replace if possible */
	{
          strmake(endpos, par, plen);
          endpos+= plen;
          tpos++;
          olen+= plen-2;
          continue;
	}
      }
      else if (*tpos == 'd' || *tpos == 'u')	/* Integer parameter */
      {
	register int iarg;
	iarg= va_arg(ap, int);
	if (*tpos == 'd')
	  plen= (uint) (int10_to_str((long) iarg, endpos, -10) - endpos);
	else
	  plen= (uint) (int10_to_str((long) (uint) iarg, endpos, 10) - endpos);
	if (olen + plen < ERRMSGSIZE+2) /* Replace parameter if possible */
	{
	  endpos+= plen;
	  tpos++;
	  olen+= plen-2;
	  continue;
	}
      }
    }
    *endpos++= '%';		/* % used as % or unknown code */
  }
  *endpos= '\0';			/* End of errmessage */
  va_end(ap);
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

  va_start(args,MyFlags);
  (void) vsprintf (ebuff,format,args);
  va_end(args);
  return (*error_handler_hook)(error, ebuff, MyFlags);
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
