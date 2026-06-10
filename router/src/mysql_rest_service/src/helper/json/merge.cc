/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#include "helper/json/merge.h"

#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"

namespace helper {
namespace json {

std::string merge_objects(const std::string &j1, const std::string &j2,
                          const std::set<std::string> &skip_attributes,
                          const std::set<std::string> &overwrite_attributes) {
  rapidjson::Document doc1(text_to_document(j1));
  rapidjson::Document doc2(text_to_document(j2));

  if (!doc1.IsObject()) return j2;
  if (!doc2.IsObject()) return j1;

  auto o1 = doc1.GetObject();
  auto o2 = doc2.GetObject();

  for (const auto &s : overwrite_attributes) {
    auto it = o1.FindMember(s);
    if (it != o1.MemberEnd()) o1.EraseMember(it);
  }

  for (auto it = o2.MemberBegin(); it != o2.MemberEnd(); ++it) {
    auto attr = it->name.GetString();

    // don't overwrite existing fields
    if (!skip_attributes.count(attr) && !o1.HasMember(attr)) {
      o1.AddMember(it->name, it->value, doc1.GetAllocator());
    }
  }

  std::string out;
  rapid_json_to_text(&doc1, out);
  return out;
}

std::optional<std::string> merge_objects(
    std::optional<std::string> j1, std::optional<std::string> j2,
    const std::set<std::string> &skip_attributes,
    const std::set<std::string> &overwrite_attributes) {
  if (!j1.has_value() && !j2.has_value()) return {};

  return {merge_objects(j1.value_or("{}"), j2.value_or("{}"), skip_attributes,
                        overwrite_attributes)};
}

}  // namespace json
}  // namespace helper
