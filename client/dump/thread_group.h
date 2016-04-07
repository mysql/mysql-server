/*
  Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef THREAD_GROUP_INCLUDED
#define THREAD_GROUP_INCLUDED

#include "thread.h"
#include <vector>

namespace my_boost{

class thread_group
{
public:
  template <typename TCallable> void create_thread(TCallable entry_point)
  {
    m_threads.push_back(new thread(entry_point));
  }

  void join_all();
  ~thread_group();
private:
  std::vector<my_boost::thread*> m_threads;
};

}

#endif
