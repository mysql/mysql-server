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

#include "dd/impl/dictionary_object_collection.h"

#include "sql_class.h"                   // THD

#include "dd/object_id.h"                // dd::Object_id
#include "dd/cache/dictionary_client.h"  // dd::Dictionary_client
#include "dd/impl/transaction_impl.h"    // dd::Transaction_ro
#include "dd/impl/raw/raw_record_set.h"  // dd::Raw_record_set
#include "dd/impl/raw/raw_table.h"       // dd::Raw_table
#include "dd/types/object_type.h"        // dd::Object_type
#include "dd/types/table.h"              // dd::Table
#include "dd/types/view.h"               // dd::View

namespace dd {

///////////////////////////////////////////////////////////////////////////

template <typename Object_type>
Dictionary_object_collection<Object_type>::~Dictionary_object_collection()
{
  // Since the objects in the collection are uncached, we must
  // delete them here.
  for (typename Array::const_iterator it= m_array.begin();
       it != m_array.end(); ++it)
    delete *it;
}

///////////////////////////////////////////////////////////////////////////


/**
  Fetch objects from DD tables that match the supplied key.

  @param object_key   The search key. If key is not supplied, then
                      we do full index scan.

  @return false       Success.
  @return true        Failure (error is reported).
*/

template <typename Object_type>
bool Dictionary_object_collection<Object_type>::fetch(
        const Object_key *object_key)
{
  std::vector<Object_id> ids;

  {
    Transaction_ro trx(m_thd);
    trx.otx.register_tables<Object_type>();
    Raw_table *table= trx.otx.get_table<Object_type>();
    DBUG_ASSERT(table);

    if (trx.otx.open_tables())
    {
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
      return true;
    }

    // Retrieve list of object ids. Do this in a nested scope to make sure
    // the record set is deleted before the transaction is committed (a
    // dependency in the Raw_record_set destructor.
    {
      std::unique_ptr<Raw_record_set> rs;
      if (table->open_record_set(object_key, rs))
      {
        DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
        return true;
      }

      Raw_record *r= rs->current_record();
      while (r)
      {
        ids.push_back(r->read_int(0)); // Read ID, which is always 1st field.

        if (rs->next(r))
        {
          DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
          return true;
        }
      }
    }

    // Close the scope to end DD transaction. This allows to avoid
    // nested DD transaction when loading objects.
  }

  // Load objects by id. This must be done without caching the
  // objects since the dictionary object collection is used in
  // situations where we do not have an MDL lock (e.g. a SHOW statement).
  for (std::vector<Object_id>::const_iterator it= ids.begin();
       it != ids.end(); ++it)
  {
    const Object_type *o= NULL;
    if (m_thd->dd_client()->acquire_uncached(*it, &o))
    {
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
      return true;
    }

    if (!o)
    {
      my_error(ER_INVALID_DD_OBJECT_ID, MYF(0), *it);
      return true;
    }

    m_array.push_back(o);
  }

  // Initialize the iterator.
  m_iterator= m_array.begin();
  return false;
}

///////////////////////////////////////////////////////////////////////////

// Explicitly instantiate the type for the various usages.
template class Dictionary_object_collection<const Schema>;
template class Dictionary_object_collection<const Abstract_table>;
template class Dictionary_object_collection<const Table>;
template class Dictionary_object_collection<const View>;
template class Dictionary_object_collection<const Tablespace>;

}
