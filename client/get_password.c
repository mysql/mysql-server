/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/*
** Ask for a password from tty
** This is an own file to avoid conflicts with curses
*/
#include <my_global.h>
#include <my_sys.h>
#include "mysql.h"
#include <m_string.h>
#include <m_ctype.h>
#include <mysql/get_password.h>

#ifdef HAVE_GETPASS
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif /* HAVE_PWD_H */
#else /* ! HAVE_GETPASS */
#ifndef _WIN32
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H				/* For tty-password */
#include	<termios.h>
#define TERMIO	struct termios
#else
#ifdef HAVE_TERMIO_H				/* For tty-password */
#include	<termio.h>
#define TERMIO	struct termio
#else
#include	<sgtty.h>
#define TERMIO	struct sgttyb
#endif
#endif
#else
#include <conio.h>
#endif /* _WIN32 */
#endif /* HAVE_GETPASS */

#ifdef HAVE_GETPASSPHRASE			/* For Solaris */
#define getpass(A) getpassphrase(A)
#endif

#ifdef _WIN32
/* were just going to fake it here and get input from
   the keyboard */

char *get_tty_password_ext(const char *opt_message,
                           strdup_handler_t strdup_function)
{
  char to[80];
  char *pos=to,*end=to+sizeof(to)-1;
  int i=0;
  DBUG_ENTER("get_tty_password_ext");
  _cputs(opt_message ? opt_message : "Enter password: ");
  for (;;)
  {
    char tmp;
    tmp=_getch();
    if (tmp == '\b' || (int) tmp == 127)
    {
      if (pos != to)
      {
	_cputs("\b \b");
	pos--;
	continue;
      }
    }
    if (tmp == '\n' || tmp == '\r' || tmp == 3)
      break;
    if (iscntrl(tmp) || pos == end)
      continue;
    _cputs("*");
    *(pos++) = tmp;
  }
  while (pos != to && isspace(pos[-1]) == ' ')
    pos--;					/* Allow dummy space at end */
  *pos=0;
  _cputs("\n");
  DBUG_RETURN(strdup_function(to,MYF(MY_FAE)));
}

#else


#ifndef HAVE_GETPASS
/*
** Can't use fgets, because readline will get confused
** length is max number of chars in to, not counting \0
*  to will not include the eol characters.
*/

static void get_password(char *to,uint length,int fd, my_bool echo)
{
  char *pos=to,*end=to+length;

  for (;;)
  {
    char tmp;
    if (my_read(fd,&tmp,1,MYF(0)) != 1)
      break;
    if (tmp == '\b' || (int) tmp == 127)
    {
      if (pos != to)
      {
	if (echo)
	{
	  fputs("\b \b",stderr);
	  fflush(stderr);
	}
	pos--;
	continue;
      }
    }
    if (tmp == '\n' || tmp == '\r' || tmp == 3)
      break;
    if (iscntrl(tmp) || pos == end)
      continue;
    if (echo)
    {
      fputc('*',stderr);
      fflush(stderr);
    }
    *(pos++) = tmp;
  }
  while (pos != to && isspace(pos[-1]) == ' ')
    pos--;					/* Allow dummy space at end */
  *pos=0;
  return;
}

#endif /* ! HAVE_GETPASS */


char *get_tty_password_ext(const char *opt_message,
                           strdup_handler_t strdup_function)
{
#ifdef HAVE_GETPASS
  char *passbuff;
#else /* ! HAVE_GETPASS */
  TERMIO org,tmp;
#endif /* HAVE_GETPASS */
  char buff[80];

  DBUG_ENTER("get_tty_password_ext");

#ifdef HAVE_GETPASS
  passbuff = getpass(opt_message ? opt_message : "Enter password: ");

  /* copy the password to buff and clear original (static) buffer */
  my_stpnmov(buff, passbuff, sizeof(buff) - 1);
#ifdef _PASSWORD_LEN
  memset(passbuff, 0, _PASSWORD_LEN);
#endif
#else 
  if (isatty(fileno(stderr)))
  {
    fputs(opt_message ? opt_message : "Enter password: ",stderr);
    fflush(stderr);
  }
#if defined(HAVE_TERMIOS_H)
  tcgetattr(fileno(stdin), &org);
  tmp = org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSADRAIN, &tmp);
  get_password(buff, sizeof(buff)-1, fileno(stdin), isatty(fileno(stderr)));
  tcsetattr(fileno(stdin), TCSADRAIN, &org);
#elif defined(HAVE_TERMIO_H)
  ioctl(fileno(stdin), (int) TCGETA, &org);
  tmp=org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME]= 0;
  ioctl(fileno(stdin),(int) TCSETA, &tmp);
  get_password(buff,sizeof(buff)-1,fileno(stdin),isatty(fileno(stderr)));
  ioctl(fileno(stdin),(int) TCSETA, &org);
#else
  gtty(fileno(stdin), &org);
  tmp=org;
  tmp.sg_flags &= ~ECHO;
  tmp.sg_flags |= RAW;
  stty(fileno(stdin), &tmp);
  get_password(buff,sizeof(buff)-1,fileno(stdin),isatty(fileno(stderr)));
  stty(fileno(stdin), &org);
#endif
  if (isatty(fileno(stderr)))
    fputc('\n',stderr);
#endif /* HAVE_GETPASS */

  DBUG_RETURN(strdup_function(buff,MYF(MY_FAE)));
}

#endif /* _WIN32 */

static char * my_strdup_fct(const char *str, myf flags)
{
  return my_strdup(PSI_NOT_INSTRUMENTED, str, flags);
}

char *get_tty_password(const char *opt_message)
{
  return get_tty_password_ext(opt_message, my_strdup_fct);
}
