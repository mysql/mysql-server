/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/tty.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <cstdio>
#else
#include <sys/ioctl.h>
#include <termios.h>  // tcgetattr, ioctl
#endif

#include <iostream>

Tty::state_type Tty::attrs() const {
  state_type tp;

#ifdef _WIN32
  // docs say "cast to HANDLE" as it is intptr_t
  HANDLE fh = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
  if (!GetConsoleMode(fh, &tp)) {
    throw std::system_error(GetLastError(), std::system_category());
  }
#else
  if (-1 == tcgetattr(fd_, &tp)) {
    throw std::system_error(errno, std::system_category());
  }
#endif

  return tp;
}

void Tty::attrs(Tty::state_type &tp) {
#ifdef _WIN32
  // docs say "cast to HANDLE" as it is intptr_t
  HANDLE fh = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
  if (!SetConsoleMode(fh, tp)) {
    throw std::system_error(GetLastError(), std::system_category());
  }
#else
  if (-1 == tcsetattr(fd_, TCSANOW, &tp)) {
    throw std::system_error(errno, std::system_category());
  }
#endif
}

void Tty::echo(bool on) {
  state_type tp = attrs();

#ifdef _WIN32
  size_t bit = Flags::Win32::Input::kEcho;
  if (on) {
    tp |= bit;
  } else {
    tp &= ~bit;
  }
#else
  // local flags
  size_t bit = Flags::Posix::Local::kEcho;
  if (on) {
    tp.c_lflag |= bit;
  } else {
    tp.c_lflag &= ~bit;
  }
#endif

  attrs(tp);
}

namespace portable {
int fileno(FILE *fh) {
#ifdef _WIN32
  return ::_fileno(fh);
#else
  return ::fileno(fh);
#endif
}
}  // namespace portable

Tty::fd_type Tty::fd_from_stream(std::ostream &os) {
  if (&os == &std::cout) {
    return portable::fileno(stdout);
  }

  if (&os == &std::cerr) {
    return portable::fileno(stderr);
  }

  return -1;
}

Tty::fd_type Tty::fd_from_stream(std::istream &is) {
  if (&is == &std::cin) {
    return portable::fileno(stdin);
  }

  return -1;
}

bool Tty::is_tty() const {
#ifdef _WIN32
  return _isatty(fd_) != 0;
#else
  return isatty(fd_) != 0;
#endif
}

bool Tty::ensure_vt100() {
#ifdef _WIN32
#if VER_PRODUCTBUILD >= 10011
  // ENABLE_VIRTUAL_TERMINAL_PROCESSING was added in winsdk 10.0.14393.0
  try {
    state_type tp = attrs();
    tp |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    attrs(tp);
    return true;
  } catch (const std::system_error &) {
    return false;
  }
#else
  return false;
#endif
#else
  return true;
#endif
}

std::pair<uint64_t, uint64_t> Tty::window_size() const {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO buffer_info;
  HANDLE fh = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
  if (0 == GetConsoleScreenBufferInfo(fh, &buffer_info)) {
    throw std::system_error(GetLastError(), std::system_category());
  }

  return {buffer_info.dwSize.X, buffer_info.dwSize.Y};
#else
  struct winsize ws;
  //
  if (-1 == ioctl(fd_, TIOCGWINSZ, &ws)) {
    throw std::system_error(errno, std::system_category());
  }

  return {ws.ws_col, ws.ws_row};
#endif
}
