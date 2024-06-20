/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  along with this program; if not, write to the Free Software
*/

#include "mrs/database/entry/object.h"

namespace mrs {
namespace database {
namespace entry {

std::string Table::as_graphql(int depth, bool extended) const {
  std::string r;
  auto indent = std::string(depth * 2, ' ');
  r += "{\n";

  foreach_field<bool>(
      [indent, extended, &r](const Column &column) {
        std::string info;
        std::string extras;
        if (!column.with_check.value_or(true)) extras += " @NOCHECK";
        if (!column.with_update.value_or(true)) extras += " @NOUPDATE";
        if (column.is_row_owner) extras += " @ROWOWNER";

        if (extended) {
          info += column.datatype;
          if (column.is_primary) {
            info += " pk";
          }
          if (column.is_primary && column.is_auto_generated_id()) {
            if (column.id_generation == IdGenerationType::AUTO_INCREMENT)
              info += " autoinc";
            else if (column.id_generation == IdGenerationType::REVERSE_UUID)
              info += " uuid";
          }
          if (!info.empty()) info = " <" + info + ">";
        }

        if (column.enabled)
          r += indent + "  " + column.name + ": " + column.column_name +
               extras + info + ",\n";
        else
          r += indent + "  -: " + column.column_name + ",\n";
        return false;
      },
      [depth, indent, extended, &r](const ForeignKeyReference &fk) {
        if (fk.enabled) {
          std::string mapping;
          if (fk.to_many)
            for (const auto &c : fk.column_mapping)
              mapping += "\"" + c.second + "\",";
          else
            for (const auto &c : fk.column_mapping)
              mapping += "\"" + c.first + "\",";
          assert(!mapping.empty());
          if (!mapping.empty()) mapping.pop_back();
          if (fk.to_many) {
            r += indent + "  ";
            if (fk.unnest)
              r += fk.name + ": " + fk.ref_table->table + " @UNNEST";
            else
              r += fk.name + ": " + fk.ref_table->table;
            r += fk.ref_table->with_insert() ? " @INSERT"
                                             : (extended ? " @NOINSERT" : "");
            r += fk.ref_table->with_update() ? " @UPDATE"
                                             : (extended ? " @NOUPDATE" : "");
            r += fk.ref_table->with_delete() ? " @DELETE"
                                             : (extended ? " @NODELETE" : "");
            r += fk.ref_table->with_check_ ? (extended ? " @CHECK" : "")
                                           : " @NOCHECK";
            r += " @LINK(to:[" + mapping + "]) ";
            r += "[" + fk.ref_table->as_graphql(depth + 1, extended);
            r.pop_back();  // trim the \n after the }
            r += "],\n";
          } else {
            r += indent + "  ";
            if (fk.unnest)
              r += fk.name + ": " + fk.ref_table->table + " @UNNEST ";
            else
              r += fk.name + ": " + fk.ref_table->table;
            r += fk.ref_table->with_insert() ? " @INSERT" : "";
            r += fk.ref_table->with_update() ? " @UPDATE"
                                             : (extended ? " @NOUPDATE" : "");
            r += fk.ref_table->with_delete() ? " @DELETE" : "";
            r += fk.ref_table->with_check_ ? (extended ? " @CHECK" : "")
                                           : " @NOCHECK";
            r += " @LINK(from:[" + mapping + "]) ";
            r += fk.ref_table->as_graphql(depth + 1, extended);
            r.pop_back();  // trim the \n after the }
            r += ",\n";
          }
        } else {
          r += indent + "  -: " + fk.ref_table->table + ",\n";
        }
        return false;
      });
  // trim ending ,
  r.pop_back();
  r.pop_back();
  r.push_back('\n');
  r += indent + "}\n";

  return r;
}

bool Table::with_check_recursive() const {
  // recursively check if any enabled field is marked for check
  if (with_check_) return true;

  return foreach_field<bool>(
      [](const Column &column) {
        if (column.enabled && column.with_check.value_or(false)) return true;
        return false;
      },
      [](const ForeignKeyReference &ref) {
        if (ref.enabled && ref.ref_table->with_check_recursive()) return true;
        return false;
      });
}

bool Table::is_editable(bool &has_unnested_1n) const {
  // view is read-write iff:
  // - there's at least one table/field that is updatable/insertable/deletable
  // - there are no unnested 1:n tables
  // TODO(alfredo): add support updating views with unnested 1:n, keeping only
  // these parts read-only

  bool editable = false;

  foreach_field<ForeignKeyReference, bool>([&](const ForeignKeyReference &ref) {
    if (ref.to_many && ref.unnest) {
      has_unnested_1n = true;
      return true;
    }
    if (ref.ref_table->is_editable(has_unnested_1n)) {
      editable = true;
    }
    return false;
  });

  if (editable || with_update_any_column() || with_insert() || with_delete())
    return true;
  return false;
}

bool Object::is_read_only() const {
  bool has_unnested_1n = false;
  if (!is_editable(has_unnested_1n)) return true;
  if (has_unnested_1n) return true;
  return false;
}

std::string DualityView::as_graphql(bool extended) const {
  std::string flags;
  flags += with_insert() ? " @INSERT " : (extended ? " @NOINSERT" : "");
  flags += with_update() ? " @UPDATE " : (extended ? " @NOUPDATE" : "");
  flags += with_delete() ? " @DELETE " : (extended ? " @NODELETE" : "");
  flags += with_check_ ? (extended ? " @CHECK" : "") : " @NOCHECK";

  return table + flags + " " + Table::as_graphql(0, extended);
}

}  // namespace entry
}  // namespace database
}  // namespace mrs