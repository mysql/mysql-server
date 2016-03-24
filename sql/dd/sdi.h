/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "dd/types/fwd.h"

#include <string>

class THD;
struct handlerton;
/**
  @file
  @ingroup sdi

  Declares the SDI ((de)serialization) api which exposes
  SDI-related functionality to the rest of the server.
*/

namespace dd {

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
sdi_t serialize(THD *thd, const Table &table, const std::string &schema_name);


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
    @retval false if successful.
    @retval true otherwise.

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

  @return error status
    @retval false if successful.
    @retval true otherwise.

*/

bool deserialize(THD *thd, const sdi_t &sdi, Table *table);


/**
  Deserialize a dd::Tablespace object.

  Populates the dd::Tablespace object provided with data from sdi string.
  Note! Additional objects are dynamically allocated and added to the
  top-level Tablespace object, which assumes ownership.

  @param thd thread context
  @param sdi  serialized representation of schema (as a json string)
  @param tablespace empty top-level object

  @return error status
    @retval false if successful.
    @retval true otherwise.

*/

bool deserialize(THD *thd, const sdi_t &sdi, Tablespace *tablespace);


/**
  Function object (functor) for updating the sdi file for a DD object.

  SDI files cannot really be updated so this must be emulated by
  removing the old file after the new one has been successfully
  stored.

  The Sdi_updater captures the the old SDI file name when it is
  created, and this must happen *before* the DD object is
  modified. The object can then be modified and updated in the DD. If
  this succeeds, one of the overloaded function call operators can be
  invoked to store the new SDI file. The old one is removed *iff* the
  store was successful.
  */

struct Sdi_updater
{
  /**
    Captures an empty string. Indicating that no old sdi file name
    needs removal.
  */
  Sdi_updater() = default;

  /**
    Captures old SDI file name.

    @param schema object which will be updated.
  */
  Sdi_updater(const Schema *schema);

  /**
    Captures old SDI file name unless SE
    supports transactional storage of SDIs.

    @param table object which will be updated.
    @param old_schema_name schema object for old version of object.
    */
  Sdi_updater(const Table *table, const std::string &old_schema_name);


  /**
    Function operator overload for Schemas.
    @param thd
    @param schema object to update.
  */
  bool operator()(THD *thd, const Schema *schema) const;

  /**
    Function operator overload for Tables. If the SE supports
    transactional storage of SDIs, store_sdi is called. Otherwise
    the new sdi_file is stored and the old one removed provided the
    store was successful.

    @param thd thread handle.
    @param table object to update.
    @param new_schema object for schema after update.
    */
  bool operator()(THD *thd, const Table *table, const Schema *new_schema) const;

  /**
    Noop function operator overload for Views.

    @retval false on success.
    @retval true otherwise.
    */
  bool operator()(THD *, const View *, const Schema *) const
  {
    return false;
  }

private:
  std::string m_prev_sdi_fname;
};


/**
  Create an Sdi_updater instance for updating a Schema.
  Factory function overload for Schemas.
  @param schema existing Schema object
 */
Sdi_updater make_sdi_updater(const Schema *schema);

/**
  Create an Sdi_updater instance for updating a Table.
  Factory function overload for Tables.
  @param thd thread context
  @param table existing Table object
  @param schema existing Schema object
 */
Sdi_updater make_sdi_updater(THD *thd, const Table *table,
                             const Schema *schema);

/**
  Create a noop Sdi_updater instance for Views.
  Factory function overload for Views to support generic code.
  @param thd
  @param view unused
  @param schema unused
 */
Sdi_updater make_sdi_updater(THD *thd, const View *view,
                             const Schema *schema);


/**
  Stores the SDI for a Schema.

  Serializes the schema object, and then forwards to SE through handlerton
  api, or falls back to storing the sdi string in an .SDI file in the
  default case.

  @param thd    Thread handle.
  @param s      Schema object.

  @retval false on success.
  @retval true otherwise.
*/

bool store_sdi(THD *thd, const dd::Schema *s);


/**
  Stores the SDI for a table.

  Serializes the table, and then forwards to SE through handlerton
  api, or falls back to storing the sdi string in an .SDI file in the
  default case. The schema object is serialized and stored
  if the schema's SDI file does not exist, or if is missing from the
  tablespace used to store the table.

  @param thd
  @param t Table object.
  @param s t's schema object.

  @retval false on success.
  @retval true otherwise.
*/

bool store_sdi(THD *thd, const dd::Table *t, const dd::Schema *s);


/**
  Noop overload for views.

  SDIs are not created or stored for views, but being able to call
  store_sdi generically on Abstract_table (which may be either a View
  or a Table), greatly simplifies the implementation of some functions
  (e.g. dd::rename_table()).

  @retval false, always (noop).
*/

inline bool store_sdi(THD *,
                      const dd::View *,
                      const dd::Schema *)
{
  return false;
}

/**
  Stores the SDI for a table space.

  Serializes the table space object, and then forwards to SE through
  handlerton api, or falls back to storing the sdi string in an .SDI
  file in the default case.

  @param thd
  @param ts     Tablespace object.

  @retval false on success.
  @retval true otherwise.
*/

bool store_sdi(THD *thd, const dd::Tablespace *ts);



/**
  Remove SDI for a schema.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd
  @param s      Schema object.

  @retval false on success.
  @retval true otherwise
*/

bool remove_sdi(THD *thd, const dd::Schema *s);


/**
  Remove SDI for a table.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd
  @param t Table object.
  @param s Schema object.

  @retval false on success.
  @retval true otherwise
*/

bool remove_sdi(THD *thd, const dd::Table *t, const dd::Schema *s);


/**
  Remove SDI for a table space.

  Forwards to SE through handlerton api, which will remove from
  tablespace, or falls back to deleting the .SDI file in the default
  case.

  @param thd    Thread handle.
  @param ts     Tablespace object.

  @retval false on success.
  @retval true otherwise
*/

bool remove_sdi(THD *thd, const dd::Tablespace *ts);


/**
  Catch-all template for other types.

  SDIs are not created, stored or removed for views or abstract tables
  representing views, but being able to call remove_sdi generically on
  Abstract_table and View objects greatly simplifies the
  implementation of some functions (e.g. dd::drop_table(),
  dd::rename_table(...)).

  @param thd    thread context
  @param ddot   data dictionary object
  @param s      schema object

  @retval false on success.
  @retval true otherwise
*/
template <class DD_object_type>
bool remove_sdi(THD *thd,
                const DD_object_type *ddot,
                const dd::Schema *s)
{
  const Table *t= dynamic_cast<const Table*>(ddot);
  if (!t)
  {
    return false;
  }
  return remove_sdi(thd, t, s);
}


bool import_sdi(THD *thd, Table *table);

} // namespace dd

#endif /* DD__SDI_INCLUDED */
