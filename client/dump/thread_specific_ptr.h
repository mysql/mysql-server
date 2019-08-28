/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef THREAD_SPECIFIC_PTR_INCLUDED
#define THREAD_SPECIFIC_PTR_INCLUDED

#include "my_global.h"
#include "my_thread.h"
#include "my_thread_local.h"

namespace my_boost{

template <typename TPtr> class thread_specific_ptr
{
public:
  thread_specific_ptr()
  {
    if (my_create_thread_local_key(&m_key, NULL))
      throw std::exception();
  }
  ~thread_specific_ptr()
  {
    my_delete_thread_local_key(m_key);
  }
  TPtr* get()
  {
    return (TPtr*)my_get_thread_local(m_key);
  }
  void reset(TPtr* new_pointer)
  {
    my_set_thread_local(m_key, new_pointer);
  }

private:
  thread_local_key_t m_key;
};

}

#endif
