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

#ifndef DD__TABLESPACE_FILES_INCLUDED
#define DD__TABLESPACE_FILES_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/sdi_fwd.h"         // dd::Sdi_wcontext
#include "sql/dd/types/weak_object.h" // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Properties;
class Tablespace;
class Tablespace_file_impl;

namespace tables {
  class Tablespace_files;
};

///////////////////////////////////////////////////////////////////////////

class Tablespace_file : virtual public Weak_object
{
public:
  typedef Tablespace_file_impl Impl;
  typedef tables::Tablespace_files DD_table;

public:
  virtual ~Tablespace_file()
  { };

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // filename.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &filename() const = 0;
  virtual void set_filename(const String_type &filename) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const = 0;

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual const Tablespace &tablespace() const = 0;

  virtual Tablespace &tablespace() = 0;


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

#endif // DD__TABLESPACE_FILES_INCLUDED
