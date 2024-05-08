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
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mrs/json/json_template_factory.h"
#include "mrs/json/response_json_template.h"
#include "mrs/json/response_sp_json_template_nest.h"
#include "mrs/json/response_sp_json_template_unnest.h"

namespace mrs {
namespace json {

JsonTemplateFactory::~JsonTemplateFactory() = default;

std::shared_ptr<database::JsonTemplate> JsonTemplateFactory::create_template(
    const database::JsonTemplateType type, const bool encode_bigints_as_strings,
    const bool include_links) const {
  switch (type) {
    case database::JsonTemplateType::kObjectNested:
      return std::shared_ptr<database::JsonTemplate>{
          new ResponseSpJsonTemplateNest(encode_bigints_as_strings)};
    case database::JsonTemplateType::kObjectUnnested:
      return std::shared_ptr<database::JsonTemplate>{
          new ResponseSpJsonTemplateUnnest(encode_bigints_as_strings)};
    case database::JsonTemplateType::kStandard:
    default:
      return std::shared_ptr<database::JsonTemplate>{
          new ResponseJsonTemplate(encode_bigints_as_strings, include_links)};
  }
}

}  // namespace json
}  // namespace mrs
