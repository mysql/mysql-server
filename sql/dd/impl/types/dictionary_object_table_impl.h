/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/os_specific.h"              // DD_HEADER_BEGIN
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Dictionary_object_table_impl : virtual public Dictionary_object_table
{
public:
  virtual ~Dictionary_object_table_impl()
  { };

  virtual bool restore_object_from_record(
    Open_dictionary_tables_ctx *otx,
    const Raw_record &record,
    Dictionary_object **o) const;
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__DICTIONARY_OBJECT_TABLE_IMPL_INCLUDED
