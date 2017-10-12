/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_DD_SDI_H
#define NDB_DD_SDI_H

/*
   Interface to the SDI serialize and deserialize functions which are
   in the impl/ directory and thus apparently not exposed as an
   internal API.
*/

#include "sql/dd/string_type.h"

namespace dd {
  class Table;
  typedef String_type sdi_t;
}

bool ndb_dd_sdi_deserialize(class THD* thd, const dd::sdi_t &sdi,
                            dd::Table *table);

dd::sdi_t ndb_dd_sdi_serialize(class THD *thd, const dd::Table &table,
                               const dd::String_type &schema_name);

#endif
