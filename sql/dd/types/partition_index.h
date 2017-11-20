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

#ifndef DD__PARTITION_INDEX_INCLUDED
#define DD__PARTITION_INDEX_INCLUDED


#include "sql/dd/sdi_fwd.h"           // dd::Sdi_wcontext
#include "sql/dd/types/index.h"       // dd::Index
#include "sql/dd/types/weak_object.h" // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_type;
class Partition;
class Partition_index_impl;
class Properties;
class Tablespace;

///////////////////////////////////////////////////////////////////////////

class Partition_index : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef Partition_index_impl Impl;

public:
  virtual ~Partition_index()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  virtual const Partition &partition() const = 0;

  virtual Partition &partition() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  virtual const Index &index() const = 0;

  virtual Index &index() = 0;

  const String_type &name() const
  { return index().name(); }

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
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;


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
