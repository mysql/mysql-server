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

#include "dd/impl/collection_impl.h"

#include "dd/impl/object_key.h"         // Needed for destructor
#include "dd/impl/utils.h"              // Delete_ptr
#include "dd/impl/raw/raw_record_set.h" // dd::Raw_record_set
#include "dd/impl/raw/raw_table.h"      // dd::Raw_table

#include <memory>     // std::unique_ptr

namespace dd {

///////////////////////////////////////////////////////////////////////////

void Base_collection::clear_all_items()
{
  std::for_each(m_items.begin(), m_items.end(), Delete_ptr());
  m_items.erase(m_items.begin(), m_items.end());
  std::for_each(m_removed_items.begin(), m_removed_items.end(), Delete_ptr());
  m_removed_items.erase(m_removed_items.begin(), m_removed_items.end());
}

///////////////////////////////////////////////////////////////////////////

Base_collection::~Base_collection()
{
  clear_all_items();
}

///////////////////////////////////////////////////////////////////////////

Collection_item *Base_collection::add(
  const Collection_item_factory &item_factory)
{
  Collection_item *item= item_factory.create_item();
  item->set_ordinal_position(static_cast<uint>(m_items.size() + 1));
  m_items.push_back(item);

  return item;
}

///////////////////////////////////////////////////////////////////////////

// Needed for WL#7141
/* purecov: begin deadcode */
Collection_item *Base_collection::add_first(
  const Collection_item_factory &item_factory)
{
  Collection_item *item= item_factory.create_item();

  m_items.insert(m_items.begin(), item);

  renumerate_items();

  return item;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
void Base_collection::remove(Collection_item *item)
{
  if (std::find(m_items.begin(), m_items.end(), item) != m_items.end())
  {
    m_removed_items.push_back(item);

    // Remove items using the "erase-remove" idiom.
    m_items.erase(
      std::remove(m_items.begin(), m_items.end(), item),
      m_items.end());

    renumerate_items();
  }
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

static bool item_compare(const Collection_item *a,
                         const Collection_item *b)
{ return a->ordinal_position() < b->ordinal_position(); }

/**
  @brief
    Populate collection with items read from DD table.

  @details
    Iterate through DD tables to find rows that match the 'Object_key'
    supplied. Create collection item for each row we find and populate
    the item with data read from DD.

  @param item_factory - Collection item object factory.
  @param otx - Context with information about open tables.
  @param table - The DD table from which read rows for items.
  @param key - The search key to be used to find rows.

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Base_collection::restore_items(
  const Collection_item_factory &item_factory,
  Open_dictionary_tables_ctx *otx,
  Raw_table *table,
  Object_key *key)
{
  DBUG_ENTER("Base_collection::restore_items");

  // NOTE: if this assert is firing, that means the table was not registered
  // for that transaction. Use Open_dictionary_tables_ctx::register_tables().
  DBUG_ASSERT(table);

  DBUG_ASSERT(is_empty());

  std::unique_ptr<Object_key> key_holder(key);

  std::unique_ptr<Raw_record_set> rs;
  if (table->open_record_set(key, rs))
    DBUG_RETURN(true);

  // Process records.

  Raw_record *r= rs->current_record();
  while (r)
  {
    Collection_item *item= add(item_factory);

    if (item->restore_attributes(*r) ||
        item->restore_children(otx) ||
        item->validate() ||
        rs->next(r))
    {
      clear_all_items();
      DBUG_RETURN(true);
    }

  }

  // The record fetched from DB may not be ordered based on ordinal position.
  // So we need to sort the elements in m_item based on ordinal position.
  std::sort(m_items.begin(), m_items.end(), item_compare);

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    store items in collection on to DD table.

  @details
    Iterate through collection and stores them in DD tables.

  @param otx - Context with information about open tables.

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Base_collection::store_items(Open_dictionary_tables_ctx *otx)
{
  DBUG_ENTER("Base_collection::store_items");

  if (is_empty())
    DBUG_RETURN(false);

  // Drop items from m_removed_items.

  for (Array::iterator it= m_removed_items.begin();
       it != m_removed_items.end();
       ++it)
  {
    if ((*it)->validate() || (*it)->drop(otx))
      DBUG_RETURN(true);
  }

  std::for_each(m_removed_items.begin(), m_removed_items.end(), Delete_ptr());
  m_removed_items.erase(m_removed_items.begin(), m_removed_items.end());

  // Add new items and update existing if needed.

  for (Array::iterator it= m_items.begin();
       it != m_items.end();
       ++it)
  {
    if ((*it)->validate() || (*it)->store(otx))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    Remove all items details from DD table.

  @details
    Iterate through the collection and remove respective rows
    from DD tables.

  @param otx - Context with information about open tables.
  @param table - The DD table from which rows are removed.
  @param key - The search key to use to find rows.

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Base_collection::drop_items(Open_dictionary_tables_ctx *otx,
                                 Raw_table *table,
                                 Object_key *key)
{
  DBUG_ENTER("Base_collection::drop_items");

  // Make sure key gets deleted
  std::unique_ptr<Object_key> key_holder(key);

  if (is_empty())
    DBUG_RETURN(false);

  // Drop items

  for (Array::iterator it= m_items.begin();
       it != m_items.end();
       ++it)
  {
    if ((*it)->drop_children(otx))
      DBUG_RETURN(true);
  }

  std::unique_ptr<Raw_record_set> rs;
  if (table->open_record_set(key, rs))
    DBUG_RETURN(true);

  // Process records.

  Raw_record *r= rs->current_record();
  while (r)
  {
    // Drop the item record from DD table
    if (r->drop())
      DBUG_RETURN(true);

    if (rs->next(r))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

}
