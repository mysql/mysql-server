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

#ifndef DD__TABLESPACE_INCLUDED
#define DD__TABLESPACE_INCLUDED

#include "my_global.h"
#include <vector>

#include "dd/types/dictionary_object.h"   // dd::Dictionary_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_type;
class Tablespace_file;
class Primary_id_key;
class Global_name_key;
class Void_key;
class Properties;

namespace tables {
  class Tablespaces;
}

template <typename I> class Iterator;
typedef Iterator<Tablespace_file>        Tablespace_file_iterator;
typedef Iterator<const Tablespace_file>  Tablespace_file_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Tablespace : virtual public Dictionary_object
{
public:
  static const Object_type &TYPE();
  static const Dictionary_object_table &OBJECT_TABLE();

  typedef Tablespace cache_partition_type;
  typedef tables::Tablespaces cache_partition_table_type;
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
  virtual ~Tablespace()
  { };

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const = 0;
  virtual void set_comment(const std::string &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Tablespace *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Tablespace *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const = 0;
  virtual void set_engine(const std::string &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace file collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Tablespace_file *add_file() = 0;

  virtual bool remove_file(std::string data_file) = 0;

  virtual Tablespace_file_const_iterator *files() const = 0;

  virtual Tablespace_file_iterator *files() = 0;

#ifndef DBUG_OFF
  /**
    Allocate a new object graph and invoke the copy contructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  virtual Tablespace *clone() const = 0;
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

const uint32 SDI_KEY_LEN = 8;
const uint32 SDI_TYPE_LEN = 4;

/** Key to identify a dictionary object */
struct sdi_key {
  /** Object id which should be unique in tablespsace */
  uint64 id;
  /** Type of Object, For ex: column, index, etc */
  uint32 type;
};

typedef std::vector<sdi_key> sdi_container;

struct sdi_vector {
  sdi_container m_vec;
};


}

#endif // DD__TABLESPACE_INCLUDED
