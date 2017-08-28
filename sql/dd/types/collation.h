/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__COLLATION_INCLUDED
#define DD__COLLATION_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/types/entity_object.h" // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Charset;
class Collation_impl;
class Primary_id_key;
class Global_name_key;
class Void_key;

namespace tables {
  class Collations;
}

///////////////////////////////////////////////////////////////////////////

class Collation : virtual public Entity_object
{
public:
  typedef Collation_impl Impl;
  typedef Collation Cache_partition;
  typedef tables::Collations DD_table;
  typedef Primary_id_key Id_key;
  typedef Global_name_key Name_key;
  typedef Void_key Aux_key;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(Id_key *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(Id_key *key, Object_id id);

  virtual bool update_name_key(Name_key *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(Name_key *key,
                              const String_type &name);

  virtual bool update_aux_key(Aux_key*) const
  { return true; }

public:
  virtual ~Collation()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Character set.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id charset_id() const = 0;
  virtual void set_charset_id(Object_id charset_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // compiled
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_compiled() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // sort_length
  /////////////////////////////////////////////////////////////////////////

  virtual uint sort_length() const = 0;

  /**
    Allocate a new object and invoke the copy contructor.

    @return pointer to dynamically allocated copy
  */
  virtual Collation *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLLATION_INCLUDED
