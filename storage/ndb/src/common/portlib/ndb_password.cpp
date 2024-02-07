/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "portlib/ndb_password.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#else
#include <io.h>
#include <windows.h>
#endif

#include <string.h>
/*
 * ndb_get_password_read_line
 * ndb_get_password_read_line_from_tty
 *
 * These function read one line of input and take as password.
 *
 * Line must end with NL, on Windows CR+NL is also valid.
 *
 * Only printable ASCII are allowed in password.
 *
 * Too long password is not truncated, rather function fails.
 *
 * If input is a terminal and stdout or stderr is also a terminal, prompt will
 * be written to terminal.
 *
 * On success function will return number of characters in password, excluding
 * the terminating NUL character.  buf must have space for size characters
 * including the terminating NUL.
 *
 * On failure it returns a ndb_get_password_error as int.
 *
 * This description should match description in ndb_password.h for
 * ndb_get_password_from_tty and ndb_get_password_from_stdin.
 */

static int ndb_get_password_read_line(int fd_in, char buf[], size_t size);

static int ndb_get_password_read_line_from_tty(int fd_in, int fd_out,
                                               const char prompt[], char buf[],
                                               size_t size) {
#if defined(_WIN32)
  assert(_isatty(fd_in));
  HANDLE hin = (HANDLE)_get_osfhandle(fd_in);
  DWORD old_mode;
  if (!GetConsoleMode(hin, &old_mode)) {
    return static_cast<int>(ndb_get_password_error::system_error);
  }

  DWORD new_mode = old_mode;
  new_mode &= ~ENABLE_ECHO_INPUT;
  new_mode |= (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
  if (!SetConsoleMode(hin, new_mode)) {
    return static_cast<int>(ndb_get_password_error::system_error);
  }
#else
  assert(isatty(fd_in));
  struct termios old_mode;
  if (tcgetattr(fd_in, &old_mode) == -1) {
    return static_cast<int>(ndb_get_password_error::system_error);
  }
  struct termios new_mode = old_mode;
  new_mode.c_lflag &= ~ECHO;
  new_mode.c_lflag |= (ECHONL | ICANON | ISIG);
  // Make sure new line is always a single NL, not a combination with CR.
  new_mode.c_lflag |= ICRNL;
#ifdef VLNEXT
  // Turn off VLNEXT to make sure no literal new line characters can be added
  new_mode.c_cc[VLNEXT] = 0;
#endif
  tcsetattr(fd_in, TCSADRAIN, &new_mode);
#endif

  int ret = 0;
  if (fd_out != -1) {
    if (prompt == nullptr) prompt = "Enter password: ";
    if (write(fd_out, prompt, strlen(prompt)) == -1) {
      ret = static_cast<int>(ndb_get_password_error::system_error);
    }
  }

  if (ret == 0) {
    ret = ndb_get_password_read_line(fd_in, buf, size);
  }

#if defined(_WIN32)
  if (fd_out != -1) {
    if (write(fd_out, "\n", 1) == -1 && ret >= 0) {
      ret = static_cast<int>(ndb_get_password_error::system_error);
    }
  }
  SetConsoleMode(hin, old_mode);
#else
  tcsetattr(fd_in, TCSADRAIN, &old_mode);
#endif
  return ret;
}

int ndb_get_password_read_line(int fd_in, char buf[], size_t size) {
  size_t len = 0;
  int ret = 0;
  while (len < size && (ret = read(fd_in, &buf[len], 1)) == 1) {
    if (buf[len] == '\n') {
      break;
    }
    len++;
  }
  if (ret == -1) {
    return static_cast<int>(ndb_get_password_error::system_error);
  }
  if (ret == 0) {
    // Input ended without end of line
    return static_cast<int>(ndb_get_password_error::no_end);
  }
  assert(ret == 1);
  if (len == size) {
    // Too long password, read out current line and fail
    char ch;
    while (read(fd_in, &ch, 1) == 1) {
      if (ch == '\n') {
        break;
      }
    }
    return static_cast<int>(ndb_get_password_error::too_long);
  }
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x20 || buf[i] > 0x7E) {
      return static_cast<int>(ndb_get_password_error::bad_char);
    }
  }
  if (len > INT_MAX) {
    return static_cast<int>(ndb_get_password_error::too_long);
  }
  buf[len] = 0;
  return len;
}

int ndb_get_password_from_tty(const char prompt[], char buf[], size_t size) {
#if defined(_WIN32)
  // Open CONIN$ for both read and write to be able to turn off echoing.
  FILE *in = fopen("CONIN$", "r+");
  FILE *out = fopen("CONOUT$", "r+");
  const int fd_in = _fileno(in);
  const int fd_out = _fileno(out);
#else
  const int fd_in = open("/dev/tty", O_RDONLY | O_NOCTTY);
  const int fd_out = open("/dev/tty", O_WRONLY | O_NOCTTY);
#endif
  int ret =
      ndb_get_password_read_line_from_tty(fd_in, fd_out, prompt, buf, size);
#if defined(_WIN32)
  fclose(in);
  fclose(out);
#else
  close(fd_in);
  close(fd_out);
#endif
  return ret;
}

int ndb_get_password_from_stdin(const char prompt[], char buf[], size_t size) {
  int fd_out = -1;
#if defined(_WIN32)
  const int fd_in = _fileno(stdin);
  if (_isatty(fd_in)) {
    if (_isatty(_fileno(stdout)))
      fd_out = _fileno(stdout);
    else if (_isatty(_fileno(stderr)))
      fd_out = _fileno(stderr);
#else
  const int fd_in = fileno(stdin);
  if (isatty(fd_in)) {
    if (isatty(fileno(stdout)))
      fd_out = fileno(stdout);
    else if (isatty(fileno(stderr)))
      fd_out = fileno(stderr);
#endif
    errno = 0;
    return ndb_get_password_read_line_from_tty(fd_in, fd_out, prompt, buf,
                                               size);
  }
  // stdin is not a tty
  return ndb_get_password_read_line(fd_in, buf, size);
}
