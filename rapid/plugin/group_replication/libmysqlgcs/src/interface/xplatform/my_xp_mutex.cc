/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "xplatform/my_xp_mutex.h"

#ifdef _WIN32
My_xp_mutex_win::My_xp_mutex_win()
  :m_mutex(static_cast<native_mutex_t *>(malloc(sizeof(*m_mutex))))
{}


My_xp_mutex_win::~My_xp_mutex_win()
{
  free(m_mutex);
}


native_mutex_t *My_xp_mutex_win::get_native_mutex()
{
  return m_mutex;
}


int My_xp_mutex_win::init(const native_mutexattr_t *attr)
{
  InitializeCriticalSection(m_mutex);
  return 0;
};


int My_xp_mutex_win::destroy()
{
  DeleteCriticalSection(m_mutex);
  return 0;
}


int My_xp_mutex_win::lock()
{
  EnterCriticalSection(m_mutex);
  return 0;
}


int My_xp_mutex_win::trylock()
{
  if (TryEnterCriticalSection(m_mutex))
  {
    /* Don't allow recursive lock */
    if (m_mutex->RecursionCount > 1)
    {
      LeaveCriticalSection(m_mutex);
      return EBUSY;
    }
    return 0;
  }
  return EBUSY;
}


int My_xp_mutex_win::unlock()
{
  LeaveCriticalSection(m_mutex);
  return 0;
}


int My_xp_mutex_util::attr_init(native_mutexattr_t *attr)
{
  return 0;
}


int My_xp_mutex_util::attr_destroy(native_mutexattr_t *attr)
{
  return 0;
}

#else
My_xp_mutex_pthread::My_xp_mutex_pthread()
  :m_mutex(static_cast<native_mutex_t *>(malloc(sizeof(*m_mutex))))
{}


My_xp_mutex_pthread::~My_xp_mutex_pthread()
{
  free(m_mutex);
}


native_mutex_t *My_xp_mutex_pthread::get_native_mutex()
{
  return m_mutex;
}

int My_xp_mutex_pthread::init(const native_mutexattr_t *attr)
{
  if (m_mutex == NULL)
    return -1;

  return pthread_mutex_init(m_mutex, attr);
};


int My_xp_mutex_pthread::destroy()
{
  return pthread_mutex_destroy(m_mutex);
}


int My_xp_mutex_pthread::lock()
{
  return pthread_mutex_lock(m_mutex);
}

/* purecov: begin deadcode */
int My_xp_mutex_pthread::trylock()
{
  return pthread_mutex_trylock(m_mutex);
}
/* purecov: end */

int My_xp_mutex_pthread::unlock()
{
  return pthread_mutex_unlock(m_mutex);
}

/* purecov: begin deadcode */
int My_xp_mutex_util::attr_init(native_mutexattr_t *attr)
{
  return pthread_mutexattr_init(attr);
}


int My_xp_mutex_util::attr_destroy(native_mutexattr_t *attr)
{
  return pthread_mutexattr_destroy(attr);
}
/* purecov: end */
#endif
