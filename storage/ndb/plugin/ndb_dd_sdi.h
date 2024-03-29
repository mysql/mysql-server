/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
}  // namespace dd

bool ndb_dd_sdi_deserialize(class THD *thd, const dd::sdi_t &sdi,
                            dd::Table *table);

dd::sdi_t ndb_dd_sdi_serialize(class THD *thd, const dd::Table &table,
                               const dd::String_type &schema_name);

/*
  @brief prettify a JSON formatted SDI. Add whitespace and other
  formatting characters to make the JSON more human readable.

  @sdi the JSON string to prettify

  @return pretty JSON string or empty string on failure.
*/

dd::sdi_t ndb_dd_sdi_prettify(dd::sdi_t sdi);

#endif
