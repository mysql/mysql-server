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

#ifndef DD__PARTITION_INCLUDED
#define DD__PARTITION_INCLUDED

#include "dd/sdi_fwd.h"               // dd::Sdi_wcontext
#include "dd/types/entity_object.h"   // dd::Entity_object
#include "my_inttypes.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_table;
class Object_type;
class Partition_impl;
class Partition_index;
class Partition_value;
class Properties;
class Table;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Partition : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef Collection<Partition_index*> Partition_indexes;
  typedef Collection<Partition_value*> Partition_values;
  typedef Partition_impl Impl;

public:
  virtual ~Partition()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const = 0;

  virtual Table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // level.
  /////////////////////////////////////////////////////////////////////////

  virtual uint level() const = 0;
  virtual void set_level(uint level) = 0;

  /////////////////////////////////////////////////////////////////////////
  // number.
  /////////////////////////////////////////////////////////////////////////

  virtual uint number() const = 0;
  virtual void set_number(uint number) = 0;

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &engine() const = 0;
  virtual void set_engine(const String_type &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const = 0;
  virtual void set_comment(const String_type &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const = 0;

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const String_type &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const = 0;

  virtual Properties &se_private_data() = 0;

  virtual bool set_se_private_data_raw(
                 const String_type &se_private_data_raw) = 0;

  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id se_private_id() const = 0;
  virtual void set_se_private_id(Object_id se_private_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition-value collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_value *add_value() = 0;

  virtual const Partition_values &values() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition-index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_index *add_index(Index *idx) = 0;

  virtual const Partition_indexes &indexes() const = 0;

  virtual Partition_indexes *indexes() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Parent partition for subpartitions (NULL otherwise).
  /////////////////////////////////////////////////////////////////////////

  virtual const Partition *parent() const = 0;
  virtual void set_parent(const Partition *parent) = 0;

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

}

#endif // DD__PARTITION_INCLUDED
