#ifndef NULLABLE_INCLUDED
#define NULLABLE_INCLUDED

/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "my_dbug.h"

namespace Mysql{

/**
  Class for storing value or NULL value.
*/
template<typename T_value> class Nullable
{
public:
  Nullable()
    : m_has_value(false)
  {}

  Nullable(T_value value)
    : m_has_value(true),
    m_value(value)
  {}

  Nullable(const Nullable<T_value>& other)
    : m_has_value(other.m_has_value),
    m_value(other.m_value)
  {}

  /**
    Assigns value from another Nullable<> object.
  */
  Nullable<T_value>& operator= (const Nullable<T_value>& other)
  {
    this->m_has_value= other.m_has_value;
    if (this->m_has_value)
    {
      this->m_value= other.m_value;
    }
    return *this;
  }

  /**
    Returns true if object has not-NULL value assigned. If this is false, one
    should not try to get value by value().
  */
  bool has_value() const
  {
    return this->m_has_value;
  }

  /**
    Returns actual value of object. Do not call this method if has_value()
    returns false.
  */
  const T_value& value() const
  {
    DBUG_ASSERT(this->m_has_value);
    return this->m_value;
  }


private:
  /**
    Specifies if this object represents NULL value. If this is false, one
    should not try to get value by value().
  */
  bool m_has_value;
  /**
    Actual value, if m_has_value is true. Undefined if m_has_value is false.
  */
  T_value m_value;
};

}

#endif // NULLABLE_INCLUDED
