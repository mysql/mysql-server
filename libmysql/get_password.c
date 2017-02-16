/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software.

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

#ifdef HAVE_GETPASS
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif /* HAVE_PWD_H */
#else /* ! HAVE_GETPASS */
#if !defined(__WIN__)
#include <sys/ioctl.h>
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
#ifdef alpha_linux_port
#include <asm/ioctls.h>				/* QQ; Fix this in configure */
#include <asm/termiobits.h>
#endif
#else
#include <conio.h>
#endif /* __WIN__ */
#endif /* HAVE_GETPASS */

#ifdef HAVE_GETPASSPHRASE			/* For Solaris */
#define getpass(A) getpassphrase(A)
#endif

#if defined(__WIN__)
/* were just going to fake it here and get input from the keyboard */
void get_tty_password_buff(const char *opt_message, char *to, size_t length)
{
  HANDLE consoleinput;
  DWORD oldstate;
  char *pos=to,*end=to+length-1;
  int i=0;

  consoleinput= CreateFile("CONIN$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ ,
    NULL, OPEN_EXISTING, 0, NULL); 
  if (consoleinput == NULL || consoleinput == INVALID_HANDLE_VALUE) 
  {
     /* This is a GUI application or service  without console input, bail out. */
     *to= 0;
     return;
  }
  _cputs(opt_message ? opt_message : "Enter password: ");

  /* 
     Switch to raw mode (no line input, no echo input).
     Allow Ctrl-C handler with ENABLE_PROCESSED_INPUT.
  */
  GetConsoleMode(consoleinput, &oldstate);
  SetConsoleMode(consoleinput, ENABLE_PROCESSED_INPUT);
  for (;;)
  {
    char tmp;
    DWORD chars_read;
    if (!ReadConsole(consoleinput, &tmp, 1, &chars_read, NULL))
      break;
    if (chars_read == 0)
      break;
    if (tmp == '\b' || tmp == 127)
    {
      if (pos != to)
      {
	_cputs("\b \b");
	pos--;
	continue;
      }
    }
    if (tmp == '\n' || tmp == '\r')
      break;
    if (iscntrl(tmp) || pos == end)
      continue;
    _cputs("*");
    *(pos++) = tmp;
  }
  /* Reset console mode after password input. */ 
  SetConsoleMode(consoleinput, oldstate);
  CloseHandle(consoleinput);
  *pos=0;
  _cputs("\n");
}

#else

#ifndef HAVE_GETPASS
/*
  Can't use fgets, because readline will get confused
  length is max number of chars in to, not counting \0
  to will not include the eol characters.
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
	  fputs("\b \b",stdout);
	  fflush(stdout);
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
      fputc('*',stdout);
      fflush(stdout);
    }
    *(pos++) = tmp;
  }
  *pos=0;
  return;
}
#endif /* ! HAVE_GETPASS */


void get_tty_password_buff(const char *opt_message, char *buff, size_t buflen)
{
#ifdef HAVE_GETPASS
  char *passbuff;
#else /* ! HAVE_GETPASS */
  TERMIO org,tmp;
#endif /* HAVE_GETPASS */

#ifdef HAVE_GETPASS
  passbuff = getpass(opt_message ? opt_message : "Enter password: ");

  /* copy the password to buff and clear original (static) buffer */
  strncpy(buff, passbuff, buflen - 1);
#ifdef _PASSWORD_LEN
  memset(passbuff, 0, _PASSWORD_LEN);
#endif
#else 
  if (isatty(fileno(stdout)))
  {
    fputs(opt_message ? opt_message : "Enter password: ",stdout);
    fflush(stdout);
  }
#if defined(HAVE_TERMIOS_H)
  tcgetattr(fileno(stdin), &org);
  tmp = org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSADRAIN, &tmp);
  get_password(buff, buflen, fileno(stdin), isatty(fileno(stdout)));
  tcsetattr(fileno(stdin), TCSADRAIN, &org);
#elif defined(HAVE_TERMIO_H)
  ioctl(fileno(stdin), (int) TCGETA, &org);
  tmp=org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME]= 0;
  ioctl(fileno(stdin),(int) TCSETA, &tmp);
  get_password(buff,buflen-1,fileno(stdin),isatty(fileno(stdout)));
  ioctl(fileno(stdin),(int) TCSETA, &org);
#else
  gtty(fileno(stdin), &org);
  tmp=org;
  tmp.sg_flags &= ~ECHO;
  tmp.sg_flags |= RAW;
  stty(fileno(stdin), &tmp);
  get_password(buff,buflen-1,fileno(stdin),isatty(fileno(stdout)));
  stty(fileno(stdin), &org);
#endif
  if (isatty(fileno(stdout)))
    fputc('\n',stdout);
#endif /* HAVE_GETPASS */
}
#endif /*__WIN__*/

#ifndef MYSQL_DYNAMIC_PLUGIN
char *get_tty_password(const char *opt_message)
{
  char buff[80];
  get_tty_password_buff(opt_message, buff, sizeof(buff));
  return my_strdup(buff, MYF(MY_FAE));
}
#endif
