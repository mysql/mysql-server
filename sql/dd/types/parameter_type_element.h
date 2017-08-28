/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PARAMETER_TYPE_ELEMENT_INCLUDED
#define DD__PARAMETER_TYPE_ELEMENT_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/types/weak_object.h"  // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Parameter;
class Parameter_type_element_impl;

namespace tables {
  class Parameter_type_elements;
};

///////////////////////////////////////////////////////////////////////////

class Parameter_type_element : virtual public Weak_object
{
public:
  typedef Parameter_type_element_impl Impl;
  typedef tables::Parameter_type_elements DD_table;

public:
  virtual ~Parameter_type_element()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Name
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &name() const = 0;
  virtual void set_name(const String_type &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Parameter
  /////////////////////////////////////////////////////////////////////////

  virtual const Parameter &parameter() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index
  /////////////////////////////////////////////////////////////////////////

  virtual uint index() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARAMETER_TYPE_ELEMENT_INCLUDED
