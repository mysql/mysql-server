/* Copyright (c) 2000, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

/*
** Ask for a password from tty
** This is an own file to avoid conflicts with curses
*/
#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"

#ifdef HAVE_GETPASS
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif /* HAVE_PWD_H */
#else  /* ! HAVE_GETPASS */
#if !defined(_WIN32)
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H /* For tty-password */
#include <termios.h>

#define TERMIO struct termios
#else
#ifdef HAVE_TERMIO_H /* For tty-password */
#include <termio.h>

#define TERMIO struct termio
#else
#include <sgtty.h>

#define TERMIO struct sgttyb
#endif
#endif
#else
#include <conio.h>
#endif /* _WIN32 */
#endif /* HAVE_GETPASS */

#ifdef HAVE_GETPASSPHRASE /* For Solaris */
#define getpass(A) getpassphrase(A)
#endif

/**
  @fn char *get_tty_password(const char *opt_message)

  Read a password from the terminal without echoing the typed characters.

  Displays @p opt_message (or the default prompt "Enter password: ") and
  reads characters one by one until the user presses Enter, Ctrl-C, or
  until the internal buffer is exhausted.  The returned string is
  allocated with my_strdup() (MY_FAE flag); the caller must release it
  with my_free().

  @param opt_message  Prompt to display before reading. Pass NULL to use
                      the default "Enter password: ".

  @return  Heap-allocated, NUL-terminated password string.

  @note  Bug#44929 / Bug#92403 / Bug#110570
         The internal stack buffer was historically 80 bytes, which
         silently truncated any password longer than 79 characters; the
         NUL terminator consumed the 80th slot and all further input was
         discarded without warning.  This caused authentication failures
         for high-entropy passwords and made it impossible to use long
         authentication credentials (such as OAuth/OIDC access tokens,
         which routinely exceed 2000 bytes) through the interactive
         prompt.

         Two concrete scenarios exposed this defect:

         (1) Long passwords.  Some deployments (notably managed cloud
         MySQL services) permit account passwords of 128 characters or
         more.  Users who chose a password of 80 characters or more
         could create the account successfully, but every subsequent
         attempt to authenticate with @c mysql @c -p would silently
         strip the password to 79 characters before sending it to the
         server, producing an "Access denied" error with no indication
         that truncation had occurred.

         (2) Token-based authentication.  Authentication plugins such
         as @c mysql_clear_password are commonly used to forward
         externally issued bearer tokens (for example OAuth 2.0 / OIDC
         access tokens) as the password field.  Such tokens routinely
         exceed 2000 characters (a typical JWT bearer token is
         ~2400 characters).  Users who preferred the interactive
         workflow --- running @c mysql @c -p and then pasting the token
         at the "Enter password:" prompt --- were unable to authenticate
         because the token was silently truncated to 79 characters.
         The resulting "Access denied" error gave no hint that the
         credential had been cut short, making the problem difficult to
         diagnose.

         In both cases the only viable workaround was to pass the
         credential inline as @c -p<value> (no space), which bypasses
         get_tty_password() entirely.  However that approach exposes the
         password or token in the operating-system process list and the
         shell command history, which is unacceptable in security-
         conscious environments.

         The buffer has been enlarged to 4096 bytes.  Passwords and
         tokens up to 4095 characters can now be entered interactively.
         The downstream path --- opt_password[], mysql->passwd, and
         clear_password_auth_client() --- imposes no additional length
         restriction, so the full credential is transmitted to the server.
*/
#if defined(_WIN32)
/* Win32: read one character at a time via _getch() to suppress echo. */
char *get_tty_password(const char *opt_message) {
  /*
   * Bug#44929 / Bug#92403 / Bug#110570: Enlarged from 80 to 4096 bytes.
   * See the Doxygen note on get_tty_password() for full rationale.
   */
  char to[4096];
  char *pos = to, *end = to + sizeof(to) - 1;

  DBUG_TRACE;
  _cputs(opt_message ? opt_message : "Enter password: ");
  for (;;) {
    char tmp;
    tmp = _getch();
    if (tmp == '\b' || (int)tmp == 127) {
      if (pos != to) {
        _cputs("\b \b");
        pos--;
        continue;
      }
    }
    if (tmp == '\n' || tmp == '\r' || tmp == 3) break;
    if (iscntrl(tmp) || pos == end) continue;
    _cputs("*");
    *(pos++) = tmp;
  }
  while (pos != to && isspace(pos[-1]) == ' ')
    pos--; /* Allow dummy space at end */
  *pos = 0;
  _cputs("\n");
  return my_strdup(PSI_NOT_INSTRUMENTED, to, MYF(MY_FAE));
}

#else

#ifndef HAVE_GETPASS
/*
  Can't use fgets, because readline will get confused
  length is max number of chars in to, not counting \0
  to will not include the eol characters.
*/

static void get_password(char *to, uint length, int fd, bool echo) {
  char *pos = to, *end = to + length;

  for (;;) {
    char tmp;
    /* Cast: my_read() expects uchar*; tmp is a single byte buffer. */
    if (my_read(fd, (uchar *)&tmp, 1, MYF(0)) != 1) break;
    if (tmp == '\b' || (int)tmp == 127) {
      if (pos != to) {
        if (echo) {
          fputs("\b \b", stdout);
          fflush(stdout);
        }
        pos--;
        continue;
      }
    }
    if (tmp == '\n' || tmp == '\r' || tmp == 3) break;
    if (iscntrl(tmp) || pos == end) continue;
    if (echo) {
      fputc('*', stdout);
      fflush(stdout);
    }
    *(pos++) = tmp;
  }
  while (pos != to && isspace(pos[-1]) == ' ')
    pos--; /* Allow dummy space at end */
  *pos = 0;
  return;
}
#endif /* ! HAVE_GETPASS */

char *get_tty_password(const char *opt_message) {
#ifdef HAVE_GETPASS
  char *passbuff;
#else  /* ! HAVE_GETPASS */
  TERMIO org, tmp;
#endif /* HAVE_GETPASS */
  /*
   * Bug#44929 / Bug#92403 / Bug#110570: Enlarged from 80 to 4096 bytes.
   * See the Doxygen note on get_tty_password() for full rationale.
   */
  char buff[4096];

  DBUG_TRACE;

#ifdef HAVE_GETPASS
  passbuff = getpass(opt_message ? opt_message : "Enter password: ");

  /* copy the password to buff and clear original (static) buffer */
  strncpy(buff, passbuff, sizeof(buff) - 1);
  buff[sizeof(buff) - 1] = 0;
#ifdef _PASSWORD_LEN
  memset(passbuff, 0, _PASSWORD_LEN);
#endif
#else
  if (isatty(fileno(stdout))) {
    fputs(opt_message ? opt_message : "Enter password: ", stdout);
    fflush(stdout);
  }
#if defined(HAVE_TERMIOS_H)
  tcgetattr(fileno(stdin), &org);
  tmp = org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSADRAIN, &tmp);
  get_password(buff, sizeof(buff) - 1, fileno(stdin), isatty(fileno(stdout)));
  tcsetattr(fileno(stdin), TCSADRAIN, &org);
#elif defined(HAVE_TERMIO_H)
  ioctl(fileno(stdin), (int)TCGETA, &org);
  tmp = org;
  tmp.c_lflag &= ~(ECHO | ISIG | ICANON);
  tmp.c_cc[VMIN] = 1;
  tmp.c_cc[VTIME] = 0;
  ioctl(fileno(stdin), (int)TCSETA, &tmp);
  get_password(buff, sizeof(buff) - 1, fileno(stdin), isatty(fileno(stdout)));
  ioctl(fileno(stdin), (int)TCSETA, &org);
#else
  gtty(fileno(stdin), &org);
  tmp = org;
  tmp.sg_flags &= ~ECHO;
  tmp.sg_flags |= RAW;
  stty(fileno(stdin), &tmp);
  get_password(buff, sizeof(buff) - 1, fileno(stdin), isatty(fileno(stdout)));
  stty(fileno(stdin), &org);
#endif
  if (isatty(fileno(stdout))) fputc('\n', stdout);
#endif /* HAVE_GETPASS */

  /*
    Ensure the buffer is always null-terminated.
    Passwords or tokens longer than 4095 bytes will be silently truncated.
  */
  buff[sizeof(buff) - 1] = '\0';
  return my_strdup(PSI_NOT_INSTRUMENTED, buff, MYF(MY_FAE));
}
#endif /* _WIN32 */
