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

#include "dd/impl/tables/collations.h"

#include "sql_class.h"                  // THD

#include "dd/dd.h"                      // dd::create_object
#include "dd/cache/dictionary_client.h" // dd::cache::Dictionary_client
#include "dd/impl/raw/object_keys.h"    // Global_name_key

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

// The table is populated when the server is started, unless it is
// started in read only mode.

bool Collations::populate(THD *thd) const
{
  Collation_impl *new_collation= create_object<Collation_impl>();

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
          new_collation->set_id(cl->number);
          new_collation->set_name(cl->name);
          // The id of the primary collation is used as the character set id
          new_collation->set_charset_id(cs->number);
          new_collation->set_is_compiled((cl->state & MY_CS_COMPILED));
          new_collation->set_sort_length(cl->strxfrm_multiply);
          error= thd->dd_client()->store(
                  static_cast<Collation*>(new_collation));
        }
      }
    }
  }

  delete new_collation;
  return error;
}

///////////////////////////////////////////////////////////////////////////

bool Collations::update_object_key(
  Global_name_key *key,
  const std::string &collation_name)
{
  key->update(FIELD_NAME, collation_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

}
}
