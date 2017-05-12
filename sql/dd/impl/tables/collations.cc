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

#include "dd/impl/tables/collations.h"

#include <stddef.h>
#include <new>
#include <set>
#include <vector>

#include "dd/cache/dictionary_client.h"           // dd::cache::Dictionary_...
#include "dd/dd.h"                                // dd::create_object
#include "dd/impl/cache/storage_adapter.h"        // Storage_adapter
#include "dd/impl/raw/object_keys.h"              // Global_name_key
#include "dd/impl/types/collation_impl.h"         // dd::Collation_impl
#include "dd/impl/types/object_table_definition_impl.h"
#include "dd/object_id.h"
#include "dd/types/collation.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/psi/mysql_statement.h"
#include "sql_class.h"                            // THD
#include "template_utils.h"

namespace dd {
class Dictionary_object;
class Raw_record;
}  // namespace dd

namespace dd {
namespace tables {

const Collations &Collations::instance()
{
  static Collations *s_instance= new Collations();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Collations::Collations()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_CHARACTER_SET_ID,
                         "FIELD_CHARACTER_SET_ID",
                         "character_set_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_IS_COMPILED,
                         "FIELD_IS_COMPILED",
                         "is_compiled BOOL NOT NULL");
  m_target_def.add_field(FIELD_SORT_LENGTH,
                         "FIELD_SORT_LENGTH",
                         "sort_length INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_PAD_ATTRIBUTE,
                         "FIELD_PAD_ATTRIBUTE",
                         "pad_attribute VARCHAR(9) NOT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY(name)");

  m_target_def.add_foreign_key("FOREIGN KEY (character_set_id) REFERENCES "
                               "character_sets(id)");
}

///////////////////////////////////////////////////////////////////////////

// The table is populated when the server is started, unless it is
// started in read only mode.

bool Collations::populate(THD *thd) const
{
  // Obtain a list of the previously stored collations.
  std::vector<const Collation*> prev_coll;
  if (thd->dd_client()->fetch_global_components(&prev_coll))
    return true;

  std::set<Object_id> prev_coll_ids;
  for (const Collation *coll : prev_coll)
    prev_coll_ids.insert(coll->id());

  // We have an outer loop identifying the primary collations, i.e.,
  // the collations which are default for some character set. The character
  // set of these primary collations is available for use, and non-primary
  // collations referring to these character sets may therefore be
  // made available. This is the purpose of the inner loop, which is
  // executed when we have found a character set of a primary collation.
  // The inner loop will iterate over all collations, and for each available
  // collation referring to the newly identified character set, an entry
  // will be added to the dd.collations table.

  // A simpler solution would be to have a single loop, and to use the
  // CHARSET_INFO::primary_number for identifying the character set id
  // (relying on the fact that the character set ids are the same as the
  // id of the character set's default collation). However, the field
  // 'primary_number' is not assigned correctly, thus, we use the outer
  // loop to identify the primary collations for now.

  Collation_impl *new_collation= create_object<Collation_impl>();
  bool error= false;
  for (int internal_charset_id= 0;
       internal_charset_id < MY_ALL_CHARSETS_SIZE && !error;
       internal_charset_id++)
  {
    CHARSET_INFO *cs= all_charsets[internal_charset_id];
    if (cs &&
        (cs->state & MY_CS_PRIMARY)   &&
        (cs->state & MY_CS_AVAILABLE) &&
        !(cs->state & MY_CS_HIDDEN))
    {
      // We have identified a primary collation
      for (int internal_collation_id= 0;
           internal_collation_id < MY_ALL_CHARSETS_SIZE && !error;
           internal_collation_id++)
      {
        CHARSET_INFO *cl= all_charsets[internal_collation_id];
        if (cl &&
            (cl->state & MY_CS_AVAILABLE) &&
            my_charset_same(cs, cl))
        {
          // Remove the id from the set of non-updated old ids.
          prev_coll_ids.erase(cl->number);

          // Preapre the new collation object.
          new_collation->set_id(cl->number);
          new_collation->set_name(cl->name);

          // The id of the primary collation is used as the character set id
          new_collation->set_charset_id(cs->number);
          new_collation->set_is_compiled((cl->state & MY_CS_COMPILED));
          new_collation->set_sort_length(cl->strxfrm_multiply);
          if (cl->pad_attribute == PAD_SPACE)
            new_collation->set_pad_attribute("PAD SPACE");
          else
            new_collation->set_pad_attribute("NO PAD");

          // If the collation exists, it will be updated; otherwise,
          // it will be inserted.
          error= cache::Storage_adapter::instance()->store(thd,
                  static_cast<Collation*>(new_collation));
        }
      }
    }
  }
  delete new_collation;

  // The remaining ids in the prev_coll_ids set were not updated, and must
  // therefore be deleted from the DD since they are not supported anymore.
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  for (std::set<Object_id>::const_iterator del_it= prev_coll_ids.begin();
       del_it != prev_coll_ids.end(); ++del_it)
  {
    const Collation *del_coll= NULL;
    if (thd->dd_client()->acquire(*del_it, &del_coll))
      return true;

    DBUG_ASSERT(del_coll);
    if (thd->dd_client()->drop(del_coll))
      return true;
  }

  return error;
}

///////////////////////////////////////////////////////////////////////////

Dictionary_object*
Collations::create_dictionary_object(const Raw_record &) const
{
  return new (std::nothrow) Collation_impl();
}

///////////////////////////////////////////////////////////////////////////

bool Collations::update_object_key(
  Global_name_key *key,
  const String_type &collation_name)
{
  key->update(FIELD_NAME, collation_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

}
}
