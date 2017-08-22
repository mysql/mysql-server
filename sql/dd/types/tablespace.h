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

#ifndef DD__TABLESPACE_INCLUDED
#define DD__TABLESPACE_INCLUDED

#include <vector>

#include "dd/collection.h"                // dd::Collection
#include "dd/sdi_fwd.h"                   // RJ_Document
#include "dd/types/entity_object.h"       // dd::Entity_object
#include "my_inttypes.h"

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object_table;
class Global_name_key;
class Object_type;
class Primary_id_key;
class Properties;
class Tablespace_impl;
class Tablespace_file;
class Void_key;

namespace tables {
  class Tablespaces;
}

///////////////////////////////////////////////////////////////////////////

class Tablespace : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Entity_object_table &OBJECT_TABLE();

  typedef Tablespace cache_partition_type;
  typedef tables::Tablespaces cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Global_name_key name_key_type;
  typedef Void_key aux_key_type;
  typedef Collection<Tablespace_file*> Tablespace_file_collection;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(name_key_type *key,
                              const String_type &name);

  virtual bool update_aux_key(aux_key_type*) const
  { return true; }

public:
  virtual ~Tablespace()
  { };


  /**
    Check if the tablespace is empty, i.e., whether it has any tables.

    @param       thd      Thread context.
    @param [out] empty    Whether the tablespace is empty.

    @return true if error, false if success.
  */

  virtual bool is_empty(THD *thd, bool *empty) const= 0;

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const = 0;
  virtual void set_comment(const String_type &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const = 0;

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const String_type &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const = 0;

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &engine() const = 0;
  virtual void set_engine(const String_type &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace file collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Tablespace_file *add_file() = 0;

  virtual bool remove_file(String_type data_file) = 0;

  virtual const Tablespace_file_collection &files() const = 0;

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object.

    @return pointer to dynamically allocated copy
  */
  virtual Tablespace *clone() const = 0;


  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */

  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;
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
