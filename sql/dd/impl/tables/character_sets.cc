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

#include "dd/impl/tables/character_sets.h"

#include "sql_class.h"                  // THD

#include "dd/dd.h"                      // dd::create_object
#include "dd/cache/dictionary_client.h" // dd::cache::Dictionary_client
#include "dd/impl/raw/object_keys.h"    // Global_name_key

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

// The table is populated when the server is started, unless it is
// started in read only mode.

bool Character_sets::populate(THD *thd) const
{
  Charset_impl *new_charset= create_object<Charset_impl>();

  // We have an outer loop identifying the primary collations, i.e.,
  // the collations which are default for some character set. Then,
  // the character set of each primary collation is stored in an entry
  // in the dd.character_sets table. This means that if there are
  // collations referring to a character set which has no default
  // collation, we will not have an entry for this character set in
  // the dd.character_sets table. This also means that a given character
  // set can have only one primary collation, since character set identity
  // is given by the character set name, and we have a unique key on the
  // character set name in the dd.character_sets table. Populating the
  // dd.collations table follows a similar pattern, but has an additional
  // inner loop adding the actual collations referring to the character
  // sets. Each character set is stored with the id (primary key) of its
  // corresponding primary collation as the id (primary key).

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
      // The character set is stored on the same id as its primary collation
      new_charset->set_id(cs->number);
      new_charset->set_name(cs->csname);
      new_charset->set_default_collation_id(cs->number);
      new_charset->set_mb_max_length(cs->mbmaxlen);
      new_charset->set_comment(cs->comment ? cs->comment : "");

      error= thd->dd_client()->store(static_cast<Charset*>(new_charset));
    }
  }

  delete new_charset;
  return error;
}

///////////////////////////////////////////////////////////////////////////

bool Character_sets::update_object_key(
  Global_name_key *key,
  const std::string &charset_name)
{
  key->update(FIELD_NAME, charset_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

}
}
