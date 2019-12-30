/*
   Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SERVER_HOOKS_H
#define NDB_SERVER_HOOKS_H

class Ndb_server_hooks {
  using hook_t = int(void *);

  struct Server_state_observer *m_server_state_observer = nullptr;
  struct Binlog_relay_IO_observer *m_binlog_relay_io_observer = nullptr;

 public:
  ~Ndb_server_hooks();

  bool register_server_hooks(hook_t *, hook_t *);
  bool register_applier_start(hook_t *);
  void unregister_all(void);
};

#endif
