/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PARTITION_VALUE_INCLUDED
#define DD__PARTITION_VALUE_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"      // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Object_type;
class Partition;

///////////////////////////////////////////////////////////////////////////

class Partition_value : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Partition_value()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  const Partition &partition() const
  { return const_cast<Partition_value *> (this)->partition(); }

  virtual Partition &partition() = 0;

  /////////////////////////////////////////////////////////////////////////
  // list_num.
  /////////////////////////////////////////////////////////////////////////

  virtual uint list_num() const = 0;
  virtual void set_list_num(uint list_num) = 0;

  /////////////////////////////////////////////////////////////////////////
  // column_num.
  /////////////////////////////////////////////////////////////////////////

  virtual uint column_num() const = 0;
  virtual void set_column_num(uint column_num) = 0;

  /////////////////////////////////////////////////////////////////////////
  // value.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &value_utf8() const = 0;
  virtual void set_value_utf8(const std::string &value) = 0;

  /////////////////////////////////////////////////////////////////////////
  // max_value.
  /////////////////////////////////////////////////////////////////////////

  virtual bool max_value() const = 0;
  virtual void set_max_value(bool max_value) = 0;

  /////////////////////////////////////////////////////////////////////////
  // null_value.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_value_null() const = 0;
  virtual void set_value_null(bool is_null) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this Partition-value from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARTITION_VALUE_INCLUDED
