/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQLD_DAEMON_INCLUDED
#define MYSQLD_DAEMON_INCLUDED

namespace mysqld {
namespace runtime {
bool is_daemon();
/**
  Daemonize mysqld.

  This function does sysv style of daemonization of mysqld.

  @retval fd In daemon; file descriptor for the write end of the status pipe.
  @retval -1 In parent, if successful.
  @retval -2 In parent, in case of errors.
 */
int mysqld_daemonize();
void signal_parent(int pipe_write_fd, char status);
}  // namespace runtime
}  // namespace mysqld
#endif  // MYSQLD_DAEMON_INCLUDED
