/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "dd/types/dictionary_object.h"    // dd::Dictionary_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Charset;
class Object_type;

class Primary_id_key;
class Global_name_key;
class Void_key;

namespace tables {
  class Collations;
}

///////////////////////////////////////////////////////////////////////////

class Collation : public Dictionary_object
{
public:
  static const Object_type &TYPE();
  static const Dictionary_object_table &OBJECT_TABLE();

  typedef Collation cache_partition_type;
  typedef tables::Collations cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Global_name_key name_key_type;
  typedef Void_key aux_key_type;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(name_key_type *key,
                              const std::string &name);

  virtual bool update_aux_key(aux_key_type *key) const
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
