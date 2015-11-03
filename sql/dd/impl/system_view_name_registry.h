/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__SYSTEM_VIEW_NAME_REGISTRY_INCLUDED
#define DD__SYSTEM_VIEW_NAME_REGISTRY_INCLUDED

#include "my_global.h"

namespace dd {

template <typename I> class Iterator;

///////////////////////////////////////////////////////////////////////////

class System_view_name_registry
{
public:
  static System_view_name_registry *instance()
  {
    static System_view_name_registry s_instance;
    return &s_instance;
  }

public:
  Iterator<const char> *names();

private:
  System_view_name_registry()
  { }

  ~System_view_name_registry()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SYSTEM_VIEW_NAME_REGISTRY_INCLUDED
