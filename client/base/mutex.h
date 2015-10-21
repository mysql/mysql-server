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

#ifndef MUTEX_INCLUDED
#define MUTEX_INCLUDED


#include "my_global.h"
#include "my_thread.h"
#include "thr_mutex.h"

namespace my_boost{

class mutex
{
public:
  mutex();
  ~mutex();
  void lock();
  bool try_lock();
  void unlock();

  class scoped_lock
  {
  public:
    scoped_lock(my_boost::mutex& mutex_to_lock);
    ~scoped_lock();

  private:
    my_boost::mutex& m_mutex;
  };

private:
  native_mutex_t m_mutex;
};

}

#endif
