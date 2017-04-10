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

#ifndef DD__OBJECT_TYPE_INCLUDED
#define DD__OBJECT_TYPE_INCLUDED

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

/**
  A singleton class that represents single dictionary object
  type.
*/
class Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const = 0;

  virtual Weak_object *create_object() const = 0;

public:
  virtual ~Object_type()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__OBJECT_TYPE_INCLUDED
