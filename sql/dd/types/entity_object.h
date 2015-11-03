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

#ifndef DD__ENTITY_OBJECT_INCLUDED
#define DD__ENTITY_OBJECT_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"             // dd::Object_id
#include "dd/types/weak_object.h"     // dd::Weak_object

#include <string>

namespace dd {

///////////////////////////////////////////////////////////////////////////

/**
  Base class for dictionary objects which has single column
  integer primary key.
*/
class Entity_object : virtual public Weak_object
{
public:
  virtual ~Entity_object()
  { };

  /// The unique dictionary object id.
  virtual Object_id id() const = 0;

  /// Is dictionary object persistent in dictionary tables ?
  virtual bool is_persistent() const = 0;

  virtual const std::string &name() const = 0;
  virtual void set_name(const std::string &name) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ENTITY_OBJECT_INCLUDED
