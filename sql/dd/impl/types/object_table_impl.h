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

#ifndef DD__OBJECT_TABLE_IMPL_INCLUDED
#define DD__OBJECT_TABLE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"                        // DD_HEADER_BEGIN
#include "dd/types/object_table.h"                      // Object_table
#include "dd/impl/types/object_table_definition_impl.h" // Object_table_defin...

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_impl : virtual public Object_table
{
protected:
  Object_table_definition_impl m_target_def;

public:
  virtual const Object_table_definition &table_definition() const
  { return m_target_def; }

  virtual bool populate(THD *thd) const
  { return false; }

  virtual bool hidden() const
  { return true; }

  virtual ~Object_table_impl()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__OBJECT_TABLE_IMPL_INCLUDED
