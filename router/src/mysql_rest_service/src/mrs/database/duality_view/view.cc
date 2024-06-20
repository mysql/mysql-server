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

std::string Table::as_graphql(int depth, bool extended) const {
  std::string r;
  auto indent = std::string(depth * 2, ' ');
  r += "{\n";

  foreach_field<bool>(
      [indent, extended, &r](const mrs::database::Field &field,
                             const mrs::database::Column &column) {
        std::string info;
        std::string extras;
        if (!column.with_check.value_or(true)) extras += " @NOCHECK";
        if (!column.with_update.value_or(true)) extras += " @NOUPDATE";

        if (extended) {
          info += column.datatype;
          if (column.is_primary) {
            info += " pk";
          }
          if (column.is_primary && column.is_auto_generated_id()) {
            if (column.id_generation ==
                mrs::database::IdGenerationType::AUTO_INCREMENT)
              info += " autoinc";
            else if (column.id_generation ==
                     mrs::database::IdGenerationType::REVERSE_UUID)
              info += " uuid";
          }
          if (!info.empty()) info = " <" + info + ">";
        }

        if (field.enabled)
          r += indent + "  " + field.name + ": " + column.name + extras + info +
               ",\n";
        else
          r += indent + "  -: " + column.name + ",\n";
        return false;
      },
      [depth, indent, extended, &r](
          const mrs::database::Field &field,
          const mrs::database::ForeignKeyReference &fk) {
        if (field.enabled) {
          std::string mapping;
          if (fk.to_many)
            for (const auto &c : fk.ref_columns) mapping += "\"" + c + "\",";
          else
            for (const auto &c : fk.columns) mapping += "\"" + c + "\",";
          assert(!mapping.empty());
          if (!mapping.empty()) mapping.pop_back();
          if (fk.to_many) {
            r += indent + "  ";
            if (fk.unnest)
              r += fk.ref_table->table + " @UNNEST";
            else
              r += field.name + ": " + fk.ref_table->table;
            r += fk.ref_table->with_insert() ? " @INSERT"
                                             : (extended ? " @NOINSERT" : "");
            r += fk.ref_table->with_update() ? " @UPDATE"
                                             : (extended ? " @NOUPDATE" : "");
            r += fk.ref_table->with_delete() ? " @DELETE"
                                             : (extended ? " @NODELETE" : "");
            r += " @LINK(to:[" + mapping + "]) ";
            r += "[" + fk.ref_table->as_graphql(depth + 1, extended);
            r.pop_back();  // trim the \n after the }
            r += "],\n";
          } else {
            r += indent + "  ";
            if (fk.unnest)
              r += field.name + ": " + fk.ref_table->table + " @UNNEST ";
            else
              r += field.name + ": " + fk.ref_table->table;
            r += fk.ref_table->with_insert() ? " @INSERT" : "";
            r += fk.ref_table->with_update() ? " @UPDATE"
                                             : (extended ? " @NOUPDATE" : "");
            r += fk.ref_table->with_delete() ? " @DELETE" : "";
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

bool Table::is_updatable(bool update_only) const {
  if (with_update_any_column()) return true;
  if (!update_only && (with_insert() || with_delete())) return true;

  bool updatable = false;

  foreach_reference<bool>(
      [&updatable](const Field &, const ForeignKeyReference &ref) {
        if (ref.ref_table->is_updatable(!ref.to_many)) {
          updatable = true;
          return true;
        }
        return false;
      });

  return updatable;
}

bool DualityView::is_updatable() const { return Table::is_updatable(true); }

void DualityView::validate_definition() const {
  // Checks:
  // - if any table has no PK -> error
  // - if duplicate field name -> error
  // - if duplicate column -> error
  // - if any nested join is missing a PK column -> read-only
  // - if any object has the FK for a 1:1 nested join as a field -> read-only
  // - if any nested 1:n object has the FK to the parent as a field -> read-only
  // - if any nested 1:n join is unnested -> read-only
}

std::string DualityView::as_graphql(bool extended) const {
  std::string flags;
  flags += with_insert() ? " @INSERT " : (extended ? " @NOINSERT" : "");
  flags += with_update() ? " @UPDATE " : (extended ? " @NOUPDATE" : "");
  flags += with_delete() ? " @DELETE " : (extended ? " @NODELETE" : "");

  return table + flags + " " + Table::as_graphql(0, extended);
}

}  // namespace database
}  // namespace mrs
