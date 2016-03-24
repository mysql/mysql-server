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

#ifndef DD__PARTITION_INDEX_INCLUDED
#define DD__PARTITION_INDEX_INCLUDED

#include "my_global.h"

#include "dd/sdi_fwd.h"               // dd::Sdi_wcontext
#include "dd/types/index.h"           // dd::Index
#include "dd/types/weak_object.h"     // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_type;
class Partition;
class Tablespace;

class Properties;

///////////////////////////////////////////////////////////////////////////

class Partition_index : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Partition_index()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  const Partition &partition() const
  { return const_cast<Partition_index *> (this)->partition(); }

  virtual Partition &partition() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  const Index &index() const
  { return const_cast<Partition_index *> (this)->index(); }

  virtual Index &index() = 0;

  const std::string &name() const
  { return index().name(); }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Partition_index *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Partition_index *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(
                 const std::string &se_private_data_raw) = 0;

  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this partition_index from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;


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

#endif // DD__PARTITION_INDEX_INCLUDED
