/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__PARAMETER_TYPE_ELEMENTS_INCLUDED
#define DD_TABLES__PARAMETER_TYPE_ELEMENTS_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Parameter_type_elements : public Object_table_impl
{
public:
  static const Parameter_type_elements &instance()
  {
    static Parameter_type_elements *s_instance= new Parameter_type_elements();
    return *s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("parameter_type_elements");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_PARAMETER_ID,
    FIELD_INDEX,
    FIELD_NAME
  };

public:
  Parameter_type_elements();

  virtual const std::string &name() const
  { return Parameter_type_elements::table_name(); }

public:
  static Object_key *create_key_by_parameter_id(Object_id parameter_id);

  static Object_key *create_primary_key(Object_id parameter_id, int index);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__PARAMETER_TYPE_ELEMENTS_INCLUDED
