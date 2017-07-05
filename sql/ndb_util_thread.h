/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_UTIL_THREAD_H
#define NDB_UTIL_THREAD_H

#include "ndb_component.h"
#include <mysql/psi/mysql_thread.h>

class Ndb_util_thread : public Ndb_component
{
public:
  Ndb_util_thread();
  virtual ~Ndb_util_thread();

private:
  mysql_mutex_t LOCK;
  mysql_cond_t COND;

  virtual int do_init();
  virtual void do_run();
  virtual int do_deinit();
  // Wake up for stop
  virtual void do_wakeup();
};

#endif
