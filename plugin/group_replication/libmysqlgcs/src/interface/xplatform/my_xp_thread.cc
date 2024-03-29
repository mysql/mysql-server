/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"

#include <errno.h>

#include "my_compiler.h"
#include "mysql/psi/mysql_thread.h"

#ifndef XCOM_STANDALONE
My_xp_thread_server::My_xp_thread_server()
    : m_thread_handle(static_cast<native_thread_handle *>(
          malloc(sizeof(native_thread_handle)))) {}

My_xp_thread_server::~My_xp_thread_server() { free(m_thread_handle); }

native_thread_t *My_xp_thread_server::get_native_thread() {
  return &m_thread_handle->thread;
}

int My_xp_thread_server::create(PSI_thread_key key [[maybe_unused]],
                                const native_thread_attr_t *attr,
                                native_start_routine func, void *arg) {
  return mysql_thread_create(key, m_thread_handle, attr, func, arg);
}

int My_xp_thread_server::create_detached(PSI_thread_key key [[maybe_unused]],
                                         native_thread_attr_t *attr,
                                         native_start_routine func, void *arg) {
  native_thread_attr_t my_attr;
  bool using_my_attr = false;

  if (attr == nullptr) {
    My_xp_thread_util::attr_init(&my_attr);
    attr = &my_attr;
    using_my_attr = true;
  }

  My_xp_thread_util::attr_setdetachstate(attr, NATIVE_THREAD_CREATE_DETACHED);

  int ret_status = create(key, attr, func, arg);

  if (using_my_attr) My_xp_thread_util::attr_destroy(&my_attr);

  return ret_status;
}

int My_xp_thread_server::join(void **value_ptr) {
  return my_thread_join(m_thread_handle, value_ptr);
}

int My_xp_thread_server::cancel() { return my_thread_cancel(m_thread_handle); }

void My_xp_thread_util::exit(void *value_ptr) {
  my_thread_end();
  my_thread_exit(value_ptr);
}

int My_xp_thread_util::attr_init(native_thread_attr_t *attr) {
  return my_thread_attr_init(attr);
}

int My_xp_thread_util::attr_destroy(native_thread_attr_t *attr) {
  return my_thread_attr_destroy(attr);
}

native_thread_t My_xp_thread_util::self() { return my_thread_self(); }

int My_xp_thread_util::equal(native_thread_t t1, native_thread_t t2) {
  return my_thread_equal(t1, t2);
}

int My_xp_thread_util::attr_setstacksize(native_thread_attr_t *attr,
                                         size_t stacksize) {
  return my_thread_attr_setstacksize(attr, stacksize);
}

int My_xp_thread_util::attr_setdetachstate(native_thread_attr_t *attr,
                                           int detachstate) {
  return my_thread_attr_setdetachstate(attr, detachstate);
}

int My_xp_thread_util::attr_getstacksize(native_thread_attr_t *attr,
                                         size_t *stacksize) {
  return my_thread_attr_getstacksize(attr, stacksize);
}

void My_xp_thread_util::yield() { my_thread_yield(); }
#endif
