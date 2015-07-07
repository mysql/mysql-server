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

#ifndef THREAD_INCLUDED
#define THREAD_INCLUDED

#include "my_global.h"
#include "my_thread.h"

namespace my_boost{

class thread
{
public:
  template <typename TCallable> thread(TCallable start)
  {
    context<TCallable>* new_context= new context<TCallable>(start);

    if (my_thread_create(
      &m_thread, NULL, context<TCallable>::entry_point, new_context))
    {
      throw std::exception();
    }
  }

  void join();

private:
  my_thread_handle m_thread;

  template <typename TCallable> class context
  {
  public:
    context(TCallable callable)
      : m_callable(callable)
    {}

    static void* entry_point(void* context_raw)
    {
      context* this_context= (context*)context_raw;
      this_context->m_callable();
      delete this_context;
      return 0;
    }

  private:
    TCallable m_callable;
  };
};

}

#endif
