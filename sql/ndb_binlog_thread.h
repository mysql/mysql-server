/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_BINLOG_THREAD_H
#define NDB_BINLOG_THREAD_H

#include <string>
#include <vector>
#include "ndb_component.h"

class Ndb_binlog_thread : public Ndb_component
{
public:
  Ndb_binlog_thread();
  virtual ~Ndb_binlog_thread();
  bool remember_pending_purge(const char *file);
private:
  virtual int do_init();
  virtual void do_run();
  virtual int do_deinit();
  // Wake up for stop
  virtual void do_wakeup();
  void recall_pending_purges(THD* thd);
  mysql_mutex_t m_purge_mutex;
  std::vector<std::string> m_pending_purges;
};

#endif
