/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"

#ifndef XCOM_STANDALONE
My_xp_mutex_server::My_xp_mutex_server()
  :m_mutex(static_cast<mysql_mutex_t *>(malloc(sizeof(*m_mutex))))
{}


My_xp_mutex_server::~My_xp_mutex_server()
{
  free(m_mutex);
}


mysql_mutex_t *My_xp_mutex_server::get_native_mutex()
{
  return m_mutex;
}


int My_xp_mutex_server::init(PSI_mutex_key key, const native_mutexattr_t *attr)
{
  if (m_mutex == NULL)
    return -1;

  return mysql_mutex_init(key, m_mutex, attr);
}


int My_xp_mutex_server::destroy()
{
  return mysql_mutex_destroy(m_mutex);
}


int My_xp_mutex_server::lock()
{
  return mysql_mutex_lock(m_mutex);
}


int My_xp_mutex_server::trylock()
{
  return mysql_mutex_trylock(m_mutex);
}


int My_xp_mutex_server::unlock()
{
  return mysql_mutex_unlock(m_mutex);
}
#endif


int My_xp_mutex_util::attr_init(native_mutexattr_t *attr)
{
  /*
    On Windows there is no initialization of mutex attributes.
    Therefore, we simply return 0.
  */
#ifdef _WIN32
  return 0;
#else
  return pthread_mutexattr_init(attr);
#endif
}


int My_xp_mutex_util::attr_destroy(native_mutexattr_t *attr)
{
  /*
    On Windows there is no destruction of mutex attributes.
    Therefore, we simply return 0.
  */
#ifdef _WIN32
  return 0;
#else
  return pthread_mutexattr_destroy(attr);
#endif
}
