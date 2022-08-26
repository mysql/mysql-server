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
#include "sql/range_optimizer/path_helpers.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"

#include "storage/ndb/plugin/ha_ndbcluster_push.h"

/**
  Compute the access type and index (if applicable) of this operation.

  WL#14370 note: We still use the pre-WL access_types to describe how
  the tables are accessed. Rational is to try to reduce the amount of
  changes in that WL (it is still a lot though)
  There will likely be a follow up WL, where entire AQP is merged into
  the ha_ndbcluster plugin code, where this might be reworked.
*/
void pushed_table::compute_type_and_index() {
  DBUG_TRACE;
  switch (m_path->type) {
    case AccessPath::EQ_REF: {
      const TABLE_REF *ref = m_path->eq_ref().ref;
      m_index_no = ref->key;

      if (m_index_no == static_cast<int>(m_table->s->primary_key)) {
        DBUG_PRINT("info", ("Operation %d is a primary key lookup.", m_tab_no));
        m_access_type = AT_PRIMARY_KEY;
      } else {
        DBUG_PRINT("info",
                   ("Operation %d is a unique index lookup.", m_tab_no));
        m_access_type = AT_UNIQUE_KEY;
      }
      break;
    }
    case AccessPath::REF: {
      /**
       * NOTE: From optimizer POW, REF access means: 'may return multiple rows'.
       * This does not necessarily mean that a range type access operation is
       * used by the storage engine, even if that is the most likely case.
       * In particular, if the (UNIQUE) HASH-index type is used (NDB), we have
       * to take care: If the key contain NULL values it will degrade to a
       * full table scan, else it will be an unique single row lookup.
       * (i.e, can never be an index scan as suggested by type = REF!)
       */
      const TABLE_REF *ref = m_path->ref().ref;
      m_index_no = ref->key;

      const KEY *key_info = m_table->s->key_info;
      if (unlikely(key_info[m_index_no].algorithm == HA_KEY_ALG_HASH)) {
        /**
         * Note that there can still be NULL values in the key if
         * it is constructed from Item_fields referring other tables.
         * This is not known until execution time, so below we do
         * a best guess about no NULL values:
         */
        // PK is fully null_rejecting, so can't be the PRIMARY KEY
        assert(m_index_no != static_cast<int>(m_table->s->primary_key));
        m_access_type = AT_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique key referrence.", m_tab_no));
      } else {
        m_access_type = AT_ORDERED_INDEX_SCAN;
        DBUG_PRINT("info", ("Operation %d is an index scan.", m_tab_no));
      }
      break;
    }
    case AccessPath::INDEX_SCAN: {
      // Note that an INDEX_SCAN usually 'use_order'.
      // In such cases it should only be either the root, or a child
      // being duplicate eliminated. (Checked in ::is_pushable_as_child())
      const auto &param = m_path->index_scan();
      m_index_no = param.idx;
      m_access_type = AT_ORDERED_INDEX_SCAN;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;
    }
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN: {
      /*
        It means that the decision on which access method to use
        will be taken late (as rows from the preceding operation arrive).
        This operation is therefore not pushable.
      */
      DBUG_PRINT("info", ("Operation %d has 'dynamic range' -> not pushable",
                          m_tab_no));
      m_access_type = AT_UNDECIDED;
      m_index_no = -1;
      break;
    }
    case AccessPath::INDEX_MERGE: {
      // Is a range_scan using the index_merge access type.
      // It produce a set of (MULTIPLE) PK's from the MERGEed indexes.
      m_index_no = m_table->s->primary_key;
      m_access_type = AT_MULTI_PRIMARY_KEY;
      m_other_access_reason = "Index-merge";
      break;
    }
    /**
     * Note that both INDEX_RANGE_SCAN and MRR use the 'multi-range-read'
     * handler interface, thus they are quite similar.
     *  - INDEX_RANGE_SCAN is generated by the range optimizer, while
     *  - MRR is the inner part of a BKA operation, getting its range_keys
     * from the outer BKA operand. Both operate on a set of ranges.
     */
    case AccessPath::INDEX_RANGE_SCAN: {
      const KEY *key_info = m_table->s->key_info;
      DBUG_EXECUTE("info", dbug_dump(m_path, 0, true););
      m_index_no = used_index(m_path);
      if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH) {
        m_access_type =
            (m_index_no == static_cast<int>(m_table->s->primary_key))
                ? AT_MULTI_PRIMARY_KEY
                : AT_MULTI_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique 'range' referrence.", m_tab_no));
      } else {
        // Note that there can still be single row lookups in the 'MIX'
        m_access_type = AT_MULTI_MIXED;
        DBUG_PRINT("info", ("Operation %d is an range scan.", m_tab_no));
      }
      m_other_access_reason = "Range-scan";
      break;
    }
    case AccessPath::MRR: {
      const TABLE_REF *ref = m_path->mrr().ref;
      const KEY *key_info = m_table->s->key_info;
      m_index_no = ref->key;
      assert(m_index_no != MAX_KEY);

      if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH) {
        m_access_type =
            (m_index_no == static_cast<int>(m_table->s->primary_key))
                ? AT_MULTI_PRIMARY_KEY
                : AT_MULTI_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique mrr-key referrence.", m_tab_no));
      } else {
        // Note that there can still be single row lookups in the 'MIX'
        m_access_type = AT_MULTI_MIXED;
        DBUG_PRINT("info", ("Operation %d is an mrr index scan.", m_tab_no));
      }

      if (m_table->in_use->lex->is_explain()) {
        // Align possible 'EXPLAIN_NO_PUSH' with explain format being used.
        // MRR is explained as a 'Multi range' with iterator-based formats
        // else 'Batched..'
        if (m_table->in_use->lex->explain_format->is_iterator_based()) {
          m_other_access_reason = "Multi-range";
        } else {
          m_other_access_reason = "Batched-key";
        }
      }
      break;
    }
    case AccessPath::TABLE_SCAN: {
      DBUG_PRINT("info", ("Operation %d is a table scan.", m_tab_no));
      m_access_type = AT_TABLE_SCAN;
      break;
    }
    case AccessPath::REF_OR_NULL: {
      DBUG_PRINT("info",
                 ("Operation %d is REF_OR_NULL. (REF + SCAN)", m_tab_no));
      m_access_type = AT_UNDECIDED;  // Is both a REF *and* a SCAN
      break;
    }

    /////////////////////////////////////////
    // Not yet seen *_SKIP_SCAN AccessPath in any test cases.
    // Believe they are only generated from the HG optimizer.
    // Handle them as required in later HG-integration bug reports
    case AccessPath::INDEX_SKIP_SCAN:
      m_access_type = AT_OTHER;
      m_other_access_reason = "'Index skip scan'-AccessPath not handled yet.";
      m_index_no = -1;  // used_index(m_path);
      assert(false);
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      m_access_type = AT_OTHER;
      m_other_access_reason =
          "'Group index skip scan'-AccessPath not handled yet.";
      m_index_no = -1;  // used_index(m_path);
      assert(false);
      break;
    case AccessPath::FOLLOW_TAIL:  // A recursive reference to table
      m_access_type = AT_OTHER;
      m_other_access_reason = "'Follow tail'-AccessPath not implemented.";
      m_index_no = -1;
      assert(false);
      break;

    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::CONST_TABLE:
    default:
      DBUG_PRINT("info",
                 ("Operation %d of AccessPath::Type %d. -> Not pushable.",
                  m_tab_no, m_path->type));
      m_access_type = AT_OTHER;
      m_index_no = -1;
      m_other_access_reason = "This table access method can not be pushed.";
      break;
  }
}

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

/**
  Get the field_no'th KEY_PART_INFO for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
const KEY_PART_INFO *pushed_table::get_key_part_info(uint field_no) const {
  assert(field_no < get_no_of_key_fields());
  assert(m_index_no >= 0);
  const KEY *key = &m_table->key_info[m_index_no];
  return &key->key_part[field_no];
}

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
