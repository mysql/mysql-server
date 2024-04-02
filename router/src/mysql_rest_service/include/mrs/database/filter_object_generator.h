/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_FILTER_OBJECT_GENERATOR_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_FILTER_OBJECT_GENERATOR_H_

#include <list>
#include <memory>
#include <string>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include "mrs/database/entry/object.h"

namespace mrs {
namespace database {

class FilterObjectGenerator {
 public:
  using Document = rapidjson::Document;
  using Object = Document::ConstObject;
  using Value = const Document::ValueType;

 public:
  FilterObjectGenerator(std::shared_ptr<database::entry::Object> object = {},
                        bool joins_allowed = false, uint64_t wait_timeout = 0,
                        bool use_wait_in_where = true);

  void reconfigure(uint64_t wait_timeout = 0, bool use_wait_in_where = true);
  void parse(const Document &doc);
  void parse(const std::string &filter_query);
  mysqlrouter::sqlstring get_result() const;
  mysqlrouter::sqlstring get_asof() const;
  bool has_where() const;
  bool has_order() const;
  bool has_asof() const;

  enum Clear { kWhere = 1, kOrder = 2, kAsof = 4, kAll = 7 };
  void reset(const Clear clear = Clear::kAll);

 private:
  void parse_complex_or(Value *value);
  void parse_complex_and(Value *value);
  bool parse_simple_object(Value *value);
  bool parse_complex_object(const char *name, Value *value);
  void parse_orderby_asof_wmember(Object object);
  void parse_order(Object object);
  void parse_asof(Value *value);
  void parse_wmember(const char *name, Value *value);
  void parse_match(Value *value);
  std::shared_ptr<entry::DataField> resolve_field(const char *name);
  mysqlrouter::sqlstring resolve_field_name(
      std::shared_ptr<entry::DataField> &dfield, const char *name,
      bool for_sorting) const;

  std::shared_ptr<database::entry::Object> object_metadata_;
  bool joins_allowed_{false};
  bool empty_{true};
  mysqlrouter::sqlstring where_;
  std::list<std::string> argument_;
  mysqlrouter::sqlstring order_;
  mysqlrouter::sqlstring asof_gtid_{};
  uint64_t wait_timeout_{0};
  bool use_wait_in_where_{true};
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_FILTER_OBJECT_GENERATOR_H_
