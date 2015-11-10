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

#ifndef DD__COLLECTION_ITEM_INCLUDED
#define DD__COLLECTION_ITEM_INCLUDED

#include "my_global.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Raw_record;

///////////////////////////////////////////////////////////////////////////

class Collection_item
{
public:
  virtual void set_ordinal_position(uint ordinal_position) = 0;

  virtual uint ordinal_position() const = 0;

  virtual bool is_hidden() const = 0;

  virtual bool restore_attributes(const Raw_record &r) = 0;

  virtual bool restore_children(Open_dictionary_tables_ctx *trx) = 0;

  virtual bool drop_children(Open_dictionary_tables_ctx *trx) = 0;

  virtual bool validate() const = 0;

  virtual bool store(Open_dictionary_tables_ctx *trx) = 0;

  virtual bool drop(Open_dictionary_tables_ctx *trx) = 0;

  virtual void drop() = 0; /* purecov: deadcode */

  virtual ~Collection_item()
  { }
};

///////////////////////////////////////////////////////////////////////////

class Collection_item_factory
{
public:
  virtual Collection_item *create_item() const = 0;

  virtual ~Collection_item_factory()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLLECTION_ITEM_INCLUDED
