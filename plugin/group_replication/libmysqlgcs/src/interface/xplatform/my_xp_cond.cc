/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_cond.h"

#ifndef XCOM_STANDALONE
My_xp_cond_server::My_xp_cond_server()
    : m_cond(static_cast<mysql_cond_t *>(malloc(sizeof(*m_cond)))) {}

My_xp_cond_server::~My_xp_cond_server() { free(m_cond); }

int My_xp_cond_server::init(PSI_cond_key key) {
  return mysql_cond_init(key, m_cond);
}

int My_xp_cond_server::destroy() { return mysql_cond_destroy(m_cond); }

int My_xp_cond_server::timed_wait(mysql_mutex_t *mutex,
                                  const struct timespec *abstime) {
  return mysql_cond_timedwait(m_cond, mutex, abstime);
}

int My_xp_cond_server::wait(mysql_mutex_t *mutex) {
  return mysql_cond_wait(m_cond, mutex);
}

int My_xp_cond_server::signal() { return mysql_cond_signal(m_cond); }

int My_xp_cond_server::broadcast() { return mysql_cond_broadcast(m_cond); }

mysql_cond_t *My_xp_cond_server::get_native_cond() { return m_cond; }
#endif
