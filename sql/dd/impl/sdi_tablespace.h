/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__SDI_TABLESPACE_INCLUDED
#define DD__SDI_TABLESPACE_INCLUDED

#include "sql/dd/impl/sdi.h" // dd::Sdi_type
#include "sql/dd/object_id.h"

class THD;
struct handlerton;

namespace dd {
class Tablespace;
namespace cache {
class Dictionary_client;
}
namespace sdi_tablespace {

/**
  @defgroup sdi_tablespace Storage operations for SDIs in tablespaces.
  @ingroup sdi

  Called from functions in sdi.cc if the dd object resides in an SE
  supporting SDI storage in tablespaces.

  @note
  There is no function for dropping the Tablespace
  SDI. Dropping a tablespace implies that all SDIs in it are dropped
  also.

  @{
*/


/**
  Looks up the relevant tablespaces for the table and stores the
  table SDI in each.

  @param thd
  @param hton
  @param sdi
  @param table
  @param schema
 */
bool store_tbl_sdi(THD *thd, const handlerton &hton, const Sdi_type &sdi,
                   const Table &table, const dd::Schema &schema);

/**
  Stores the tablespace SDI in the tablespace.

  @param hton
  @param sdi
  @param tablespace
 */
bool store_tsp_sdi(const handlerton &hton, const Sdi_type &sdi,
                   const Tablespace &tablespace);

/**
  Looks up the relevant tablespaces for the table and drops the
  table SDI in each.

  @note When the last table in a schema is dropped from a tablespace
  the schema SDI should also be dropped. But leaving them is not a big
  problem as the schema SDIs are small (they only contain the default
  charset for the schema).

  @param thd
  @param hton
  @param table
  @param schema
 */
bool drop_tbl_sdi(THD *thd, const handlerton &hton,
                  const Table &table,
                  const Schema &schema MY_ATTRIBUTE((unused)));

/** @} End of group sdi_tablespace */
}
}
#endif // !DD__SDI_TABLESPACE_INCLUDED
