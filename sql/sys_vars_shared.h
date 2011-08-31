#ifndef SYS_VARS_SHARED_INCLUDED
#define SYS_VARS_SHARED_INCLUDED

/* Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file
  "protected" interface to sys_var - server configuration variables.

  This header is included by files implementing support and utility
  functions of sys_var's (set_var.cc) and files implementing
  classes in the sys_var hierarchy (sql_plugin.cc)
*/

#include <sql_priv.h>
#include "set_var.h"

extern bool throw_bounds_warning(THD *thd, const char *name,
                                 bool fixed, bool is_unsigned, longlong v);
extern bool throw_bounds_warning(THD *thd, const char *name, bool fixed,
                                 double v);
extern sys_var *intern_find_sys_var(const char *str, uint length);

extern sys_var_chain all_sys_vars;

/** wrapper to hide a mutex and an rwlock under a common interface */
class PolyLock
{
public:
  virtual void rdlock()= 0;
  virtual void wrlock()= 0;
  virtual void unlock()= 0;
  virtual ~PolyLock() {}
};

class PolyLock_mutex: public PolyLock
{
  mysql_mutex_t *mutex;
public:
  PolyLock_mutex(mysql_mutex_t *arg): mutex(arg) {}
  void rdlock() { mysql_mutex_lock(mutex); }
  void wrlock() { mysql_mutex_lock(mutex); }
  void unlock() { mysql_mutex_unlock(mutex); }
};

class PolyLock_rwlock: public PolyLock
{
  mysql_rwlock_t *rwlock;
public:
  PolyLock_rwlock(mysql_rwlock_t *arg): rwlock(arg) {}
  void rdlock() { mysql_rwlock_rdlock(rwlock); }
  void wrlock() { mysql_rwlock_wrlock(rwlock); }
  void unlock() { mysql_rwlock_unlock(rwlock); }
};

class AutoWLock
{
  PolyLock *lock;
public:
  AutoWLock(PolyLock *l) : lock(l) { if (lock) lock->wrlock(); }
  ~AutoWLock() { if (lock) lock->unlock(); }
};

class AutoRLock
{
  PolyLock *lock;
public:
  AutoRLock(PolyLock *l) : lock(l) { if (lock) lock->rdlock(); }
  ~AutoRLock() { if (lock) lock->unlock(); }
};


#endif /* SYS_VARS_SHARED_INCLUDED */
