/*
   Copyright (c) 2022, Oracle and/or its affiliates.

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
/**
  @file

  @brief
  This file contain methods for accessing query plan info used for
  pushing queries and conditions to the ndb data node
  (for execution by the SPJ block).
*/

#include "sql/item.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"

#include "storage/ndb/plugin/ha_ndbcluster_push.h"

//////////////////////////////////////////
// Get'ers for pushed_table

const TABLE_REF *pushed_table::get_table_ref() const {
  switch (m_path->type) {
    case AccessPath::EQ_REF:
      return m_path->eq_ref().ref;
    case AccessPath::REF:
      return m_path->ref().ref;
    case AccessPath::MRR:
      return m_path->mrr().ref;
    case AccessPath::REF_OR_NULL:
      return m_path->ref_or_null().ref;
    case AccessPath::FULL_TEXT_SEARCH:
      return m_path->full_text_search().ref;
    case AccessPath::CONST_TABLE:
      return m_path->const_table().ref;
    case AccessPath::INDEX_SCAN:
    case AccessPath::INDEX_RANGE_SCAN:
      // Might be requested, but rejected later
      return nullptr;
    default:
      return nullptr;
  }
}

/**
   Check if the specified AccessPath operation require the result
   to be returned using the index order.
*/
bool pushed_table::use_order() const {
  switch (m_path->type) {
    // case AccessPath::EQ_REF:
    //  return m_path->eq_ref().use_order;
    case AccessPath::REF:
      return m_path->ref().use_order;
    case AccessPath::REF_OR_NULL:
      return m_path->ref_or_null().use_order;
    case AccessPath::INDEX_SCAN:
      return m_path->index_scan().use_order;
    case AccessPath::FULL_TEXT_SEARCH:
      return m_path->full_text_search().use_order;
    default:
      return false;
  }
}

/**
  Get the number of key values for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
uint pushed_table::get_no_of_key_fields() const {
  const TABLE_REF *ref = get_table_ref();
  if (ref == nullptr) return 0;
  return ref->key_parts;
}

/**
  Get the field_no'th key values for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
const Item *pushed_table::get_key_field(uint field_no) const {
  const TABLE_REF *ref = get_table_ref();
  if (ref == nullptr) return nullptr;
  assert(field_no < get_no_of_key_fields());
  return ref->items[field_no];
}

/** Get the table that this operation accesses. */
const TABLE *pushed_table::get_table() const { return m_table; }

/** Get the Item_equal's set relevant for the specified 'Item_field' */
Item_equal *pushed_table::get_item_equal(const Item_field *item_field) const {
  assert(item_field->type() == Item::FIELD_ITEM);
  const TABLE_LIST *const table_ref = m_table->pos_in_table_list;
  COND_EQUAL *const cond_equal = table_ref->query_block->join->cond_equal;
  if (cond_equal != nullptr) {
    return item_field->find_item_equal(cond_equal);
  }
  return nullptr;
}

Item *pushed_table::get_condition() const {
  if (m_filter == nullptr) return nullptr;
  return m_filter->filter().condition;
}
