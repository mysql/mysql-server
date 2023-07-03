/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
#ifndef MYSQL_HARNESS_STDX_PROCESS_INCLUDED
#define MYSQL_HARNESS_STDX_PROCESS_INCLUDED

// partial implementation of P1750R1
//
// see: http://wg21.link/p1750
#include <sys/types.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace stdx {
namespace this_process {
#ifdef _WIN32
using native_handle_type = HANDLE;
using pid_type = int;
#else
using native_handle_type = pid_t;
using pid_type = pid_t;
#endif

inline pid_type get_id() {
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif
}

inline native_handle_type native_handle() {
#ifdef _WIN32
  return GetCurrentProcess();
#else
  return getpid();
#endif
}

}  // namespace this_process
}  // namespace stdx
#endif
