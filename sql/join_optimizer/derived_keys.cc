/* Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/join_optimizer/derived_keys.h"

#include <assert.h>
#include <sys/types.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "my_base.h"
#include "my_inttypes.h"
#include "prealloced_array.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "template_utils.h"

/**
   Add a field of a derived table to the set of fields for which we want to
   generate keys.
   @param thd The current thread.
   @param query_block The current Query_block. Ignore 'field_item' if it
      belongs to another.
   @param field_item The field to add to the set.
   @param equal_to The expression that field_item is equal to. We may have to
     create separate keys for each pair of (equi-)joined tables. Therefore,
     we need this.
   @returns true if there was an error.
*/
static bool AddKeyField(THD *thd, const Query_block *query_block,
                        const Item_field *field_item, Item *equal_to) {
  Table_ref *const table_ref{field_item->m_table_ref};

  if (table_ref->query_block != query_block ||
      !table_ref->is_view_or_derived() ||
      // Index access on UNION and other set operations is not supported.
      table_ref->derived_query_expression()->set_operation() != nullptr) {
    return false;
  }

  Field *const field{field_item->field};

  assert(std::find(field->table->field,
                   field->table->field + field->table->s->fields,
                   field) != field->table->field + field->table->s->fields);

  bool allocated{false};
  return table_ref->update_derived_keys(thd, field, &equal_to, 1, &allocated);
}

/**
   Add any field from derived_tab.field=expression to the set of fields to
   generate keys for.
   @param thd The current thread.
   @param query_block The current Query_block. Ignore fields belonging to
     other Query_block instances.
   @param eq The predicate to search for fields of derived tables.
   @returns true if there was an error.
*/
static bool AddKeyFieldsFromEqual(THD *thd, const Query_block *query_block,
                                  const Item_func_eq *eq) {
  for (int i : {0, 1}) {
    if (eq->arguments()[i]->type() != Item::FIELD_ITEM) {
      continue;
    }

    const Item_field *field_item{down_cast<Item_field *>(eq->arguments()[i])};

    Item *other{eq->arguments()[1 - i]};
    if (AddKeyField(thd, query_block, field_item, other)) {
      return true;
    }
  }
  return false;
}

/**
   Add any field from derived_tab.field=expression to the set of fields to
   generate keys for.
   @param thd The current thread.
   @param query_block The current Query_block. Ignore fields belonging to
     other Query_block instances.
   @param equal The predicate to search for fields of derived tables.
   @returns true if there was an error.
*/
static bool AddKeyFieldsFromMultiEqual(THD *thd, const Query_block *query_block,
                                       Item_multi_eq *equal) {
  for (const Item_field &left : Item_multi_eq::ConstFieldProxy(equal)) {
    if (left.m_table_ref->query_block != query_block ||
        !left.m_table_ref->is_view_or_derived()) {
      continue;
    }

    if (equal->const_arg() != nullptr) {
      if (AddKeyField(thd, query_block, &left, equal->const_arg())) {
        return true;
      }
      continue;
    }

    for (Item &right : Item_multi_eq::FieldProxy(equal)) {
      if (&left == &right) {
        continue;
      }

      if (AddKeyField(thd, query_block, &left, &right)) {
        return true;
      }
    }
  }
  return false;
}

bool MakeDerivedKeys(THD *thd, JOIN *join) {
  assert(join->query_block->materialized_derived_table_count);

  auto extract_keys{[&](Item *item) {
    if (is_function_of_type(item, Item_func::Functype::EQ_FUNC)) {
      return AddKeyFieldsFromEqual(thd, join->query_block,
                                   down_cast<const Item_func_eq *>(item));

    } else if (is_function_of_type(item, Item_func::Functype::MULTI_EQ_FUNC)) {
      return AddKeyFieldsFromMultiEqual(thd, join->query_block,
                                        down_cast<Item_multi_eq *>(item));
    } else {
      return false;
    }
  }};

  if (join->where_cond != nullptr &&
      WalkItem(join->where_cond, enum_walk::PREFIX, extract_keys)) {
    return true;
  }

  for (Table_ref *table_ref = join->query_block->leaf_tables;
       table_ref != nullptr; table_ref = table_ref->next_leaf) {
    if (table_ref->join_cond() != nullptr &&
        WalkItem(table_ref->join_cond(), enum_walk::PREFIX, extract_keys)) {
      return true;
    }
  }
  return join->generate_derived_keys();
}

using KeyMap = MutableOverflowBitset;

namespace {
/// Result type for GetDerivedKey
struct GetDerivedKeyResult {
  /// The TABLE_SHARE that the key belongs to.
  TABLE_SHARE *share;
  /// The key index.
  int key;
};
}  // namespace

/// Find the derived key used in 'path', if there is one.
static std::optional<GetDerivedKeyResult> GetDerivedKey(
    const AccessPath &path) {
  if (path.type == AccessPath::REF &&
      path.ref().table->pos_in_table_list->is_view_or_derived()) {
    return GetDerivedKeyResult{path.ref().table->s, path.ref().ref->key};
  } else {
    return {};
  }
}

namespace {
/// The set of used keys for a TABLE_SHARE.
struct TableShareInfo {
  /// The TABLE_SHARE.
  TABLE_SHARE *share;
  /// The keys that are in use.
  KeyMap used_keys;
};
}  // namespace

/// Joins of more than 10 tables are rare, so use this when sizing containers.
constexpr size_t kExpectedTableCount{10};

/// The set of used keys, for each derived table.
using TableShareInfoCollection =
    Prealloced_array<TableShareInfo, kExpectedTableCount>;

/**
   Find 'share' in 'collection' if present.
   @param collection The collection to search in.
   @param share The share to search for.
   @returns TableShareInfo for 'share' or nullptr if 'share' not found.
*/
TableShareInfo *FindTableShareInfo(TableShareInfoCollection *collection,
                                   const TABLE_SHARE *share) {
  const auto iter{std::find_if(
      collection->begin(), collection->end(),
      [&](const TableShareInfo &entry) { return entry.share == share; })};

  return iter == collection->end() ? nullptr : std::to_address(iter);
}

/**
   Find the set of keys that are in use in all derived Table_ref objects
   that belong to 'query_block'.
   @param thd The current thread.
   @param query_block The current Query_block.
   @param root_path The root path of 'query_block'.
   @param share_info_collection The set of keys for derived tables that are in
     use.
*/
static void FindUsedDerivedKeys(
    THD *thd, const Query_block &query_block, const AccessPath *root_path,
    TableShareInfoCollection *share_info_collection) {
  // Collect all keys used by AccessPath objects.
  auto examine_path{[&](const AccessPath *path, const JOIN *) {
    const std::optional<GetDerivedKeyResult> path_key{GetDerivedKey(*path)};
    if (path_key.has_value()) {
      TableShareInfo *const share_info{
          FindTableShareInfo(share_info_collection, path_key.value().share)};

      if (share_info == nullptr) {
        KeyMap map{thd->mem_root, path_key.value().share->keys};
        for (uint i = 0; i < path_key.value().share->first_unused_tmp_key;
             i++) {
          map.SetBit(i);
        }
        map.SetBit(path_key.value().key);
        share_info_collection->emplace_back(
            TableShareInfo{path_key.value().share, std::move(map)});
      } else {
        share_info->used_keys.SetBit(path_key.value().key);
      }
    }
    return false;
  }};

  WalkAccessPaths(root_path, query_block.join,
                  WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK, examine_path);
}

/**
   Mark all unique and hash keys as is use.
   @param thd The current thread.
   @param query_block The current Query_block.
   @param share_info_collection The set of keys for derived tables that are in
     use.
*/
static void FindUniqueAndHashKeys(
    THD *thd, const Query_block &query_block,
    TableShareInfoCollection *share_info_collection) {
  for (const Table_ref *table_ref{query_block.leaf_tables};
       table_ref != nullptr; table_ref = table_ref->next_leaf) {
    if (table_ref->uses_materialization()) {
      Derived_refs_iterator it(table_ref);

      while (TABLE *derived_tab = it.get_next()) {
        if (derived_tab->pos_in_table_list->query_block == &query_block) {
          assert(derived_tab->pos_in_table_list->uses_materialization());
          if (!derived_tab->is_created()) {
            KeyMap &used_keys{[&]() -> KeyMap & {
              TableShareInfo *const share_info{
                  FindTableShareInfo(share_info_collection, derived_tab->s)};

              if (share_info == nullptr) {
                share_info_collection->emplace_back(TableShareInfo{
                    derived_tab->s,
                    KeyMap{thd->mem_root, derived_tab->s->keys}});

                return share_info_collection->back().used_keys;
              } else {
                return share_info->used_keys;
              }
            }()};

            // Mark all unique indexes as in use, since they have an effect
            // (deduplication) whether any expression refers to them or not.
            // In particular, they are used if we want to materialize a UNION
            // DISTINCT directly into the derived table.
            for (uint key_idx = 0; key_idx < derived_tab->s->keys; ++key_idx) {
              if (Overlaps(derived_tab->key_info[key_idx].flags, HA_NOSAME)) {
                used_keys.SetBit(key_idx);
              }
            }
            // Same for the hash key used for manual deduplication, if any.
            // (It always has index 0 if it exists.)
            if (derived_tab->hash_field != nullptr) {
              used_keys.SetBit(0);
            }
          }
        }
      }
    }
  }
}

/**
   Remove any unused keys from derived table '*table_ref'.
   @param query_block The current query block.
   @param share_info_collection The set of keys for derived tables that are in
     use.
   @param table_ref The derived table for which we remove the unused keys.
*/
static void RemoveUnusedKeys(const Query_block &query_block,
                             TableShareInfoCollection *share_info_collection,
                             const Table_ref *table_ref) {
  TABLE_SHARE *const share{table_ref->table->s};

  TableShareInfo *const share_info{
      FindTableShareInfo(share_info_collection, share)};

  const int old_key_count = share->keys;

  // Compact the key_info arrays in TABLE/TABLE_SHARE. Re-number bitmaps like
  // TABLE::part_of_key according to the new positions of the remaining keys.
  for (int old_idx = share->first_unused_tmp_key; old_idx < old_key_count;
       old_idx++) {
    if (IsBitSet(old_idx, share_info->used_keys)) {
      if (std::cmp_greater(old_idx, share->first_unused_tmp_key)) {
        Derived_refs_iterator it(table_ref);
        bool modify_share{true};

        while (TABLE *derived_tab = it.get_next()) {
          if (derived_tab->pos_in_table_list->query_block == &query_block) {
            assert(derived_tab->pos_in_table_list->uses_materialization());

            if (!derived_tab->is_created()) {
              assert(share->owner_of_possible_tmp_keys == &query_block);
              derived_tab->move_tmp_key(old_idx, modify_share);
              modify_share = false;
            }
          }
        }
      } else {
        share->first_unused_tmp_key++;
      }
    }
  }

  Derived_refs_iterator it(table_ref);
  bool modify_share{true};

  // Clear tails of key_info arrays and corresponding bitmaps.
  while (TABLE *derived_tab = it.get_next()) {
    if (!derived_tab->is_created()) {
      derived_tab->drop_unused_tmp_keys(modify_share);
      modify_share = false;
    }
  }

  assert(std::cmp_greater_equal(share->keys,
                                PopulationCount(share_info->used_keys)));

  if (share->owner_of_possible_tmp_keys == &query_block) {
    // Unlock TABLE_SHARE.
    share->owner_of_possible_tmp_keys = nullptr;
  }
}

/**
   Print the set of keys defined on derived table 'table_ref' to the
   optimizer trace for.
*/
static void TraceDerivedKeys(THD *thd, const Table_ref *table_ref) {
  assert(table_ref->is_view_or_derived());
  const TABLE_SHARE &share{*table_ref->table->s};

  if (share.keys > 0) {
    if (table_ref->common_table_expr() == nullptr) {
      Trace(thd) << "Keys for derived table '" << table_ref->alias
                 << "' considered during planning':\n";
    } else {
      // For CTEs there is a single TABLE_SHARE common to all aliases. So we
      // use the name of the CTE rather than the first alias.
      Trace(thd) << "Keys for CTE '" << table_ref->table_name
                 << "' considered during planning':\n";
    }

    for (const KEY &key : Bounds_checked_array(share.key_info, share.keys)) {
      Trace(thd) << " - " << key.name << " : {";

      for (uint i = 0; i < key.actual_key_parts; i++) {
        if (i > 0) {
          Trace(thd) << ", ";
        }
        Trace(thd) << "'" << key.key_part[i].field->field_name << "'";
      }

      Trace(thd) << "}\n";
    }
  }
}

void FinalizeDerivedKeys(THD *thd, const Query_block &query_block,
                         AccessPath *root_path) {
  // Find used keys.
  TableShareInfoCollection share_info_collection{PSI_NOT_INSTRUMENTED};
  FindUsedDerivedKeys(thd, query_block, root_path, &share_info_collection);
  FindUniqueAndHashKeys(thd, query_block, &share_info_collection);

  // To keep track of the shares we have processed.
  Prealloced_array<const TABLE_SHARE *, kExpectedTableCount> processed_shares{
      PSI_NOT_INSTRUMENTED};

  // Remove unused keys.
  for (const Table_ref *table_ref{query_block.leaf_tables};
       table_ref != nullptr; table_ref = table_ref->next_leaf) {
    if (table_ref->uses_materialization() && table_ref->is_view_or_derived() &&
        std::find(processed_shares.cbegin(), processed_shares.cend(),
                  table_ref->table->s) == processed_shares.cend()) {
      if (TraceStarted(thd)) {
        TraceDerivedKeys(thd, table_ref);
      }

      RemoveUnusedKeys(query_block, &share_info_collection, table_ref);
      processed_shares.push_back(table_ref->table->s);
    }
  }

  // Change key numbers in 'path' to use the new key number.
  auto translate_keys{[&](AccessPath *path, const JOIN *) {
    const std::optional<GetDerivedKeyResult> key_data{GetDerivedKey(*path)};
    if (key_data.has_value()) {
      TableShareInfo *const share_info{
          FindTableShareInfo(&share_info_collection, key_data.value().share)};

      int new_idx{0};

      for (int i = 0; i < key_data.value().key; i++) {
        if (IsBitSet(i, share_info->used_keys)) {
          new_idx++;
        }
      }

      path->ref().ref->key = new_idx;
    }
    return false;
  }};

  WalkAccessPaths(root_path, query_block.join,
                  WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK, translate_keys);
}
