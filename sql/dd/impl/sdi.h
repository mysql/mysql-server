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

#ifndef DD__SDI_INCLUDED
#define DD__SDI_INCLUDED

#include "dd/string_type.h"                    // dd::String_type

class THD;
struct handlerton;

/**
  @file
  @ingroup sdi

  Declares the SDI ((de)serialization) api which exposes
  SDI-related functionality to the rest of the dictionary code.
*/

namespace dd {

class Schema;
class Table;
class Tablespace;
class View;
typedef String_type sdi_t;

/**
  Serialize a Schema object.

  @param schema dobject which will be serialized

  @return sdi (as json string).

*/
sdi_t serialize(const Schema &schema);


/**
  Serialize a Table object.

  @param thd
  @param table object which will be serialized
  @param schema_name
  @return sdi (as json string).

*/
sdi_t serialize(THD *thd, const Table &table, const String_type &schema_name);


/**
  Serialize a Tablespace object.

  @param tablespace object which will be serialized

  @return sdi (as json string).

*/

sdi_t serialize(const Tablespace &tablespace);


/**
  Deserialize a dd::Schema object.

  Populates the dd::Schema object provided with data from sdi string.
  Note! Additional objects are dynamically allocated and added to the
  top-level Schema object, which assumes ownership.

  @param thd thread context
  @param sdi  serialized representation of schema (as a json string)
  @param schema empty top-level object

  @return error status
    @retval false if successful
    @retval true otherwise

*/

bool deserialize(THD *thd, const sdi_t &sdi, Schema *schema);


/**
  Deserialize a dd::Table object.

  Populates the dd::Table object provided with data from sdi string.
  Note! Additional objects are dynamically allocated and added to the
  top-level Schema object, which assumes ownership.

  @param thd thread context
  @param sdi  serialized representation of schema (as a json string)
  @param table empty top-level object
  @param deser_schema_name name of schema containing the table

  @return error status
    @retval false if successful
    @retval true otherwise

*/

bool deserialize(THD *thd, const sdi_t &sdi, Table *table,
                 String_type *deser_schema_name= nullptr);


/**
  Deserialize a dd::Tablespace object.

  Populates the dd::Tablespace object provided with data from sdi string.
  Note! Additional objects are dynamically allocated and added to the
  top-level Tablespace object, which assumes ownership.

  @param thd thread context
  @param sdi  serialized representation of schema (as a json string)
  @param tablespace empty top-level object

  @return error status
    @retval false if successful
    @retval true otherwise

*/

bool deserialize(THD *thd, const sdi_t &sdi, Tablespace *tablespace);


/**
  Wl#7524
 */
bool import_sdi(THD *thd, Table *table, const String_type &schema_name);


namespace sdi {

/**
  Generic noop for all types that don't have a specific overload. No
  SDIs are written for these types.

  @param thd
  @param ddo
  @return error status
    @retval false always
 */

template <class DDT>
inline bool store(THD *thd MY_ATTRIBUTE((unused)),
                  const DDT *ddo MY_ATTRIBUTE((unused)))
{
  return false;
}


/**
  Stores the SDI for a Schema.

  Serializes the schema object, and then forwards to SE through handlerton
  api, or falls back to storing the sdi string in an .SDI file in the
  default case.

  @param thd    Thread handle.
  @param s      Schema object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool store(THD *thd, const Schema *s);


/**
  Stores the SDI for a table.

  Serializes the table, and then forwards to SE through handlerton
  api, or falls back to storing the sdi string in an .SDI file in the
  default case. The schema object is serialized and stored
  if the schema's SDI file does not exist, or if is missing from the
  tablespace used to store the table.

  @param thd
  @param t Table object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool store(THD *thd, const Table *t);


/**
  Stores the SDI for a table space.

  Serializes the table space object, and then forwards to SE through
  handlerton api, or falls back to storing the sdi string in an .SDI
  file in the default case.

  @param thd
  @param ts     Tablespace object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool store(THD *thd, const Tablespace *ts);


/**
  Generic noop for all types that don't have a specific overload. No
  SDIs are removed for these types.

  @param thd
  @return error status
    @retval false always
 */

template <class DDT>
inline bool drop(THD *thd MY_ATTRIBUTE((unused)),
                 const DDT*)
{
  return false;
}


/**
  Remove SDI for a schema.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd
  @param s      Schema object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool drop(THD *thd, const Schema *s);


/**
  Remove SDI for a table.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd
  @param t Table object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool drop(THD *thd, const Table *t);


/**
  Remove SDI for a table space.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd    Thread handle.
  @param ts     Tablespace object.

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool drop(THD *thd, const Tablespace *ts);


/**
  Hook for SDI cleanup after updating DD object. Generic noop for all
  types that don't have a specific overload.

  @param thd
  @param old_ddo
  @param new_ddo
  @return error status
    @retval false always
 */

template <class DDT>
inline bool drop_after_update(THD *thd MY_ATTRIBUTE((unused)),
                              const DDT *old_ddo MY_ATTRIBUTE((unused)),
                              const DDT *new_ddo MY_ATTRIBUTE((unused)))
{
  return false;
}


/**
  Schema cleanup hook. When Dictionary_client issues a store which is
  performed as an update in the DD a new schema SDI file will be
  stored. If the update modifies the name of the schema it is
  necessary to remove the old SDI file after the new one has been
  written successfully. If the file names are the same the file is
  updated in place, potentially leaving it corrupted if something goes
  wrong.

  @param thd
  @param old_s old Schema object
  @param new_s new Schema object

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool drop_after_update(THD *thd, const Schema *old_s, const Schema *new_s);


/**
  Table cleanup hook. When a Dictionary_client issues a store which is
  performed as an update in the DD a new table SDI file will be
  stored. If SDI is stored in a file and the update modifies the name
  of the table it is necessary to remove the old SDI file after the
  new one has been written successfully. If the file names are the
  same the file is updated in place, potentially leaving it corrupted
  if something goes wrong. If the SDI is stored in a tablespace it
  will use the same key even if the names change and the update will
  transactional so then this hook does nothing.

  @param thd
  @param old_t old Schema object
  @param new_t new Schema object

  @return error status
    @retval false on success
    @retval true otherwise
*/

bool drop_after_update(THD *thd, const Table *old_t, const Table *new_t);

} // namespace sdi
} // namespace dd

#endif /* DD__SDI_INCLUDED */
