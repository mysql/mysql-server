/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__DICTIONARY_OBJECT_TABLE_IMPL_INCLUDED
#define DD__DICTIONARY_OBJECT_TABLE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/object_table_impl.h"  // Object_table_impl
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Dictionary_object_table_impl : public Object_table_impl,
                                     public Dictionary_object_table
{
public:
  virtual ~Dictionary_object_table_impl()
  { };

  virtual bool restore_object_from_record(
    Open_dictionary_tables_ctx *otx,
    const Raw_record &record,
    Dictionary_object **o) const;

  // Fix "inherits ... via dominance" warnings
  virtual const Object_table_definition *table_definition(
                  uint version MY_ATTRIBUTE((unused))) const
  { return Object_table_impl::table_definition(version); }

  virtual const Object_table_definition *table_definition(
                  THD *thd MY_ATTRIBUTE((unused))) const
  { return Object_table_impl::table_definition(thd); }
  virtual uint default_dd_version(THD *thd) const
  { return Object_table_impl::default_dd_version(thd); }

  virtual bool populate(THD *thd) const
  { return Object_table_impl::populate(thd); }

  virtual bool hidden() const
  { return Object_table_impl::hidden(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DICTIONARY_OBJECT_TABLE_IMPL_INCLUDED
