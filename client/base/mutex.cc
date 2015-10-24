/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "mutex.h"

// We must use full boost::mutex name, because mutex is ambigious on Solaris.

my_boost::mutex::mutex()
{
  native_mutex_init(&m_mutex, NULL);
}
my_boost::mutex::~mutex()
{
  native_mutex_destroy(&m_mutex);
}
void my_boost::mutex::lock()
{
  native_mutex_lock(&m_mutex);
}
bool my_boost::mutex::try_lock()
{
  return native_mutex_trylock(&m_mutex) == 0;
}
void my_boost::mutex::unlock()
{
  native_mutex_unlock(&m_mutex);
}
my_boost::mutex::scoped_lock::scoped_lock(my_boost::mutex& mutex_to_lock)
  : m_mutex(mutex_to_lock)
{
  m_mutex.lock();
}
my_boost::mutex::scoped_lock::~scoped_lock()
{
  m_mutex.unlock();
}

