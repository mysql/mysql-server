/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mrs/database/duality_view/check.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"

namespace mrs {
namespace database {
namespace dv {

using namespace helper::json::sql;

void Check::process(JSONInputObject input) {
  if (invalid_fields_->empty()) {
    for (const auto &m : input.new_object()) {
      if (strcmp(m.name.GetString(), "_metadata") != 0 &&
          strcmp(m.name.GetString(), "links") != 0)
        invalid_fields_->insert(m.name.GetString());
    }
  }

  has_unnested_pk_ = false;
  if (unnested_) {
    // if PK of an unnested object is present, we assume the object is specified
    has_unnested_pk_ = true;
    table_->foreach_field<Column, bool>([this, input](const Column &column) {
      if (column.is_primary) {
        if (!input.find(column.name.c_str()).has_new())
          has_unnested_pk_ = false;
      }
      return false;
    });
  }

  Operation::process(std::move(input));

  if (!invalid_fields_->empty() && !unnested_) {
    throw_invalid_field(table_->table, *invalid_fields_->begin());
  }
}

void Check::on_value(const Column &column,
                     const JSONInputObject::MemberReference &value) {
  invalid_fields_->erase(column.name);

  if (column.is_primary) {
    mysqlrouter::sqlstring key("?");
    key << value.new_value();
    pk_[column.column_name] = std::move(key);
  }
}

void Check::on_no_value(const Column &column,
                        const JSONInputObject::MemberReference &) {
  if (column.is_primary) {
    // PK is optional if this is an unnested 1:1 object
    if (!unnested_) {
      if (!row_ownership_.is_owner_id(*table_, column)) {
        if (for_update_) {
          if (!table_->with_insert()) {
            throw_missing_id(table_->table);
          }
        } else {  // for insert
          if (!column.is_auto_generated_id() || !table_->with_insert()) {
            throw_missing_id(table_->table);
          }
        }
      }
    }
  } else {
    if (!row_ownership_.is_owner_id(*table_, column) &&
        table_->with_check(column) && (!unnested_ || has_unnested_pk_)) {
      throw_missing_field(table_->table, column.name);
    }
  }
}

void Check::process_to_many(const ForeignKeyReference &ref,
                            JSONInputArray input) {
  invalid_fields_->erase(ref.name);

  if (!input.has_new() || input.new_empty()) return;

  std::set<std::string> keys;

  for (size_t i = 0; i < input.size(); i++) {
    Check check(ref.ref_table, row_ownership_, for_update_,
                ref.unnest ? invalid_fields_ : nullptr, ref.unnest);

    auto elem = input.get(i);
    check.process(make_input_object(elem, ref.ref_table->table));

    // check for duplicate keys
    std::string key;
    for (const auto &c : check.primary_key()) {
      key.append(c.second.str()).append(",");
    }
    if (!key.empty()) {
      if (keys.count(key) > 0) throw_duplicate_key(table_->table, ref.name);
      keys.insert(key);
    }

    // ensure PK = ref.FK in joins where FK is included because it's part of PK
    const auto &pk = primary_key();
    const auto &ref_pk = check.primary_key();

    for (const auto &col : ref.column_mapping) {
      auto pki = pk.find(col.first);
      auto ref_pki = ref_pk.find(col.second);

      if (pki != pk.end() && ref_pki != ref_pk.end() &&
          pki->second != ref_pki->second)
        throw_mismatching_id(ref.ref_table->table, col.second);
    }
  }
}

void Check::process_to_one(const ForeignKeyReference &ref,
                           JSONInputObject input) {
  invalid_fields_->erase(ref.name);

  if (!input.has_new() || input.new_empty()) return;

  Check check(ref.ref_table, row_ownership_, true,
              ref.unnest ? invalid_fields_ : nullptr, ref.unnest);
  check.process(input);

  // ensure FK = ref.PK in joins where FK is included because it's part of PK
  const auto &ref_pk = check.primary_key();
  const auto &pk = primary_key();
  for (const auto &col : ref.column_mapping) {
    auto pki = pk.find(col.first);
    auto ref_pki = ref_pk.find(col.second);

    if (pki != pk.end() && ref_pki != ref_pk.end() &&
        pki->second != ref_pki->second)
      throw_mismatching_id(ref.ref_table->table, col.second);
  }
}

}  // namespace dv
}  // namespace database
}  // namespace mrs