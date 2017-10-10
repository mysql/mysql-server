/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SERVER_HOOKS_H
#define NDB_SERVER_HOOKS_H


class Ndb_server_hooks
{
  using hook_t = int (void*);

  struct Server_state_observer* m_server_state_observer = nullptr;
  struct Binlog_relay_IO_observer* m_binlog_relay_io_observer = nullptr;

public:
  ~Ndb_server_hooks();

  bool register_server_started(hook_t*);
  bool register_applier_start(hook_t*);
  void unregister_all(void);
};

#endif
