/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mrs/rest/openapi_object_creator.h"

#include <charconv>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "helper/string/contains.h"  // starts_with
#include "mrs/database/converters/column_datatype_converter.h"
#include "mrs/rest/handler.h"            // parse_json_options
#include "mysql/harness/string_utils.h"  // split_string

namespace mrs {
namespace rest {

namespace {
std::string get_timestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_time_t = system_clock::to_time_t(now);

  // Get the fractional seconds
  auto now_ms = duration_cast<microseconds>(now.time_since_epoch()) % 1'000'000;

  std::tm now_tm = *std::localtime(&now_time_t);

  std::ostringstream oss;
  oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
      << std::setw(6) << now_ms.count();

  return oss.str();
}
}  // anonymous namespace

bool async_enabled(const std::optional<std::string> &options) {
  const auto parsed_options = mrs::rest::parse_json_options(options);

  return (parsed_options.mysql_task.driver ==
              mrs::interface::Options::MysqlTask::DriverType::kDatabase ||
          parsed_options.mysql_task.driver ==
              mrs::interface::Options::MysqlTask::DriverType::kRouter);
}

rapidjson::Value get_header_info(
    std::shared_ptr<DbService> service,
    rapidjson::Document::AllocatorType &allocator) {
  if (!service) return {};

  std::string title;
  if (!service->name.empty()) {
    title = service->name;
  } else {
    title = service->url_context_root.substr(1);  // skip leading '\'
  }
  title += " OpenAPI specification";

  rapidjson::Value result(rapidjson::kObjectType);

  result.AddMember("title", rapidjson::Value(title, allocator), allocator)
      .AddMember("version",
                 rapidjson::Value(std::string(k_schema_version), allocator),
                 allocator);

  if (service->comment.has_value()) {
    result.AddMember("description",
                     rapidjson::Value(*service->comment, allocator), allocator);
  }

  return result;
}

rapidjson::Value get_security_scheme(
    rapidjson::Document::AllocatorType &allocator) {
  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember(
      rapidjson::Value(rest::k_auth_method_name.data(),
                       rest::k_auth_method_name.length(), allocator),
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("type", "http", allocator)
          .AddMember("scheme", "custom", allocator),
      allocator);
  return result;
}

/**
 * Helper class that facilitates generating OpenAPI swagger for the given
 * DBobject entry.
 */
class OpenApiCreator {
 public:
  OpenApiCreator(DbObjectPtr entry,
                 rapidjson::Document::AllocatorType &allocator)
      : allocator_{allocator},
        entry_{std::move(entry)},
        ref_name_{entry_->schema_name + '_' + entry_->name},
        schema_ref_{"#/components/schemas/" + ref_name_} {
    for (auto &c : entry_->object_description->fields) {
      auto column = std::dynamic_pointer_cast<mrs::database::entry::Column>(c);
      if (!column || !column->enabled) continue;

      if (column->is_primary) {
        // Add all Primary Keys, even if there are more than one
        if (!primary_key_)
          primary_key_ = "{" + c->name + "}";
        else {
          if (!primary_key_->empty()) primary_key_.value() += ",";

          primary_key_.value() += "{" + c->name + "}";
        }

        parameters_.PushBack(
            create_parameter(c->name, column_type_to_openapi(column->type)),
            allocator_);
      }
    }
  }

 public:
  /**
   * Create HTTP GET method contents for OpenAPI path.
   */
  rapidjson::Value create_get_method() const;

  /**
   * Create HTTP GET method contents for OpenAPI path. Primary key used as a
   * parameter.
   */
  rapidjson::Value create_get_by_key_method() const;

  /**
   * Create HTTP POST method contents for OpenAPI path.
   */
  rapidjson::Value create_post_method() const;

  /**
   * Create HTTP DELETE method contents for OpenAPI path.
   */
  rapidjson::Value create_delete_method() const;

  /**
   * Create HTTP DELETE method contents for OpenAPI path. Primary key used as a
   * parameter.
   */
  rapidjson::Value create_delete_by_key_method() const;

  /**
   * Create HTTP PUT method contents for OpenAPI path.
   */
  rapidjson::Value create_put_method() const;

  /**
   * Create OpenAPI components for the current entry.
   */
  rapidjson::Value create_components() const;

  /**
   * Information if the current entry contain Primary Key configured.
   */
  bool has_primary_key() const { return primary_key_.has_value(); }

  /**
   * Get Primary Key value.
   */
  std::string primary_key() const { return *primary_key_; }

  /**
   * Add OpenAPI path items for MRS Funcions and Pocedure objects.
   * @param[in] is_async Callable supports async operations
   * @param[in] privileges User privileges
   */
  rapidjson::Value get_procedure_items(const std::optional<uint32_t> privileges,
                                       const bool is_async) const;

  /**
   * Add parameter for entries with Primary Key.
   */
  rapidjson::Value create_parameter(std::string_view name,
                                    std::string_view type) const;

 private:
  /**
   * Add OpenAPI type constraints based on the MySQL datatype for the given
   * column.
   */
  rapidjson::Value add_type_constraints(
      const std::string &type_name,
      const mrs::database::entry::ColumnType type) const;

  /**
   * Add filter parameters to GET and DELETE methods.
   */
  rapidjson::Value get_filter_parameter(const bool is_required) const;

  /**
   * Add 'limit' and 'offset' parameters to GET method.
   */
  rapidjson::Value get_integer_parameter(const std::string &name) const;

  /**
   * Create a reference to OpenAPI components section for methods that are
   * returning multiple records.
   */
  rapidjson::Value get_content_schema_array() const;

  /**
   * Create a reference to OpenAPI components section for methods that are
   * returning one record.
   */
  rapidjson::Value get_content_schema_single() const;

  /**
   * Add security section if applicable.
   */
  void add_security(rapidjson::Value &method) const;

  /**
   *  Tags that are going to be used to distinguish paths.
   */
  rapidjson::Value get_tag() const;

  /**
   * Map column type to OpenAPI supported types.
   */
  std::string column_type_to_openapi(
      mrs::database::entry::ColumnType type) const;

  /**
   * Response for successful DELETE operation.
   */
  rapidjson::Value get_delete_response() const;

  /**
   * Add OpenAPI component items for MRS Pocedure objects.
   */
  rapidjson::Value get_procedure_components() const;

  /**
   * Add OpenAPI component items for MRS Funcion objects.
   */
  rapidjson::Value get_function_components() const;

  /**
   * Get an example of a result set produced by a Procedure call.
   */
  rapidjson::Value get_procedure_result_example() const;

  std::optional<rapidjson::Value> get_type_info(
      const std::string &raw_data_type) const;

 private:
  rapidjson::Document::AllocatorType &allocator_;
  DbObjectPtr entry_;
  const std::string ref_name_;
  const std::string schema_ref_;
  std::optional<std::string> primary_key_;
  rapidjson::Value parameters_{rapidjson::kArrayType};
};

std::optional<rapidjson::Value> OpenApiCreator::get_type_info(
    const std::string &raw_data_type) const {
  database::entry::ColumnType data_type;

  try {
    database::ColumnDatatypeConverter()(&data_type, raw_data_type);
  } catch (const std::exception &) {
    log_warning("Unsupported type when generating OpenAPI specification: %s",
                raw_data_type.c_str());

    return std::nullopt;
  }

  return add_type_constraints(raw_data_type, data_type);
}

rapidjson::Value OpenApiCreator::create_components() const {
  rapidjson::Value schema_properties(rapidjson::kObjectType);
  rapidjson::Value component_info(rapidjson::kObjectType);

  if (entry_->type ==
      mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure) {
    return get_procedure_components();
  } else if (entry_->type ==
             mrs::database::entry::DbObject::ObjectType::k_objectTypeFunction) {
    return get_function_components();
  } else {
    auto obj = entry_->object_description;

    for (auto &c : obj->fields) {
      auto column = std::dynamic_pointer_cast<mrs::database::entry::Column>(c);
      if (!column || !column->enabled) continue;

      auto property_details =
          add_type_constraints(column->datatype, column->type);

      std::string description = column->datatype;
      if (column->is_primary) description += ", Primary Key";

      property_details.AddMember(
          "description", rapidjson::Value(description, allocator_), allocator_);

      schema_properties.AddMember(rapidjson::Value(c->name.c_str(), allocator_),
                                  property_details, allocator_);
    }
  }

  component_info.AddMember(
      rapidjson::Value(ref_name_, allocator_),
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("type", "object", allocator_)
          .AddMember("properties", schema_properties, allocator_),
      allocator_);

  return component_info;
}

rapidjson::Value OpenApiCreator::create_parameter(std::string_view name,
                                                  std::string_view type) const {
  return std::move(
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("in", "path", allocator_)
          .AddMember("name", rapidjson::Value(name.data(), allocator_),
                     allocator_)
          .AddMember("required", true, allocator_)
          .AddMember(
              "schema",
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember("type", rapidjson::Value(type.data(), allocator_),
                             allocator_),
              allocator_));
}

rapidjson::Value OpenApiCreator::get_filter_parameter(
    const bool is_required) const {
  return std::move(rapidjson::Value(rapidjson::kObjectType)
                       .AddMember("in", "query", allocator_)
                       .AddMember("name", "q", allocator_)
                       .AddMember("description", "filter object", allocator_)
                       .AddMember("required", is_required, allocator_)
                       .AddMember("schema",
                                  rapidjson::Value(rapidjson::kObjectType)
                                      .AddMember("type", "string", allocator_),
                                  allocator_));
}

rapidjson::Value OpenApiCreator::get_integer_parameter(
    const std::string &name) const {
  return std::move(
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("in", "query", allocator_)
          .AddMember("name", rapidjson::Value(name, allocator_), allocator_)
          .AddMember("required", false, allocator_)
          .AddMember("schema",
                     rapidjson::Value(rapidjson::kObjectType)
                         .AddMember("type", "integer", allocator_),
                     allocator_));
}

rapidjson::Value OpenApiCreator::get_content_schema_array() const {
  return std::move(
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember(
              "application/json",
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember(
                      "schema",
                      rapidjson::Value(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator_)
                          .AddMember("items",
                                     rapidjson::Value(rapidjson::kObjectType)
                                         .AddMember("$ref",
                                                    rapidjson::Value(
                                                        schema_ref_.c_str(),
                                                        allocator_),
                                                    allocator_),
                                     allocator_),
                      allocator_),
              allocator_));
}

rapidjson::Value OpenApiCreator::get_content_schema_single() const {
  return std::move(
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("application/json",
                     rapidjson::Value(rapidjson::kObjectType)
                         .AddMember("schema",
                                    rapidjson::Value(rapidjson::kObjectType)
                                        .AddMember("$ref",
                                                   rapidjson::Value(
                                                       schema_ref_.c_str(),
                                                       allocator_),
                                                   allocator_),
                                    allocator_),
                     allocator_));
}

void OpenApiCreator::add_security(rapidjson::Value &method) const {
  method.AddMember(
      "security",
      rapidjson::Value(rapidjson::kArrayType)
          .PushBack(
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember(rapidjson::Value(std::string(k_auth_method_name),
                                              allocator_),
                             rapidjson::Value(rapidjson::kArrayType),
                             allocator_),
              allocator_),
      allocator_);
}

rapidjson::Value OpenApiCreator::get_tag() const {
  return std::move(
      rapidjson::Value(rapidjson::kArrayType)
          .PushBack(
              rapidjson::Value(
                  std::string{entry_->schema_name + "/" + entry_->name}.c_str(),
                  allocator_),
              allocator_));
}

static std::optional<int> get_type_size(std::string_view datatype) {
  size_t start_pos = datatype.find('(');
  size_t end_pos = datatype.find(')');
  if (start_pos == std::string_view::npos ||
      end_pos == std::string_view::npos || start_pos + 1 >= end_pos) {
    return std::nullopt;
  }

  const std::string_view num_str =
      datatype.substr(start_pos + 1, end_pos - start_pos - 1);
  int value = 0;
  auto [ptr, ec] =
      std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);

  return (ec != std::errc()) ? std::nullopt : std::optional<int>{value};
}

rapidjson::Value OpenApiCreator::add_type_constraints(
    const std::string &datatype,
    const mrs::database::entry::ColumnType type) const {
  rapidjson::Value property_details(rapidjson::kObjectType);

  // zerofill and/or width is not taken into account as it is deprecated

  if (datatype == "date") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "date", allocator_);
  } else if (datatype == "datetime" || datatype == "timestamp") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("example", get_timestamp(), allocator_);
  } else if (datatype == "time") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "time", allocator_);
    property_details.AddMember("example", "00:00:00", allocator_);
  } else if (datatype == "year") {
    property_details.AddMember("type", "integer", allocator_);
    property_details.AddMember("minimum", 1901, allocator_);
    property_details.AddMember("maximum", 2155, allocator_)
        .AddMember("example", 2024, allocator_);

  } else if (helper::starts_with(datatype, "varchar")) {
    property_details.AddMember("type", "string", allocator_);
    const auto max_maybe = get_type_size(datatype);
    if (max_maybe) {
      property_details.AddMember("maxLength", *max_maybe, allocator_);
    }
  } else if (helper::starts_with(datatype, "char")) {
    property_details.AddMember("type", "string", allocator_);
    const auto length_maybe = get_type_size(datatype);
    if (length_maybe) {
      property_details.AddMember("minLength", *length_maybe, allocator_);
      property_details.AddMember("maxLength", *length_maybe, allocator_);
    }
  } else if (datatype == "tinytext") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("maxLength", 255, allocator_);
  } else if (datatype == "text") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("maxLength", 65535, allocator_);
  } else if (datatype == "mediumtext") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("maxLength", 16777215, allocator_);
  } else if (datatype == "longtext") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember<uint64_t>("maxLength", 4294967295, allocator_);

  } else if (datatype == "tinyint unsigned") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 255, allocator_)
        .AddMember("minimum", 0, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "tinyint") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 127, allocator_)
        .AddMember("minimum", -128, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "smallint unsigned") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 65535, allocator_)
        .AddMember("minimum", 0, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "smallint") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 32767, allocator_)
        .AddMember("minimum", -32768, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "mediumint unsigned") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 16777215, allocator_)
        .AddMember("minimum", 0, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "mediumint") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember("maximum", 8388607, allocator_)
        .AddMember("minimum", -8388608, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "int unsigned") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember<uint64_t>("maximum", 4294967295, allocator_)
        .AddMember("minimum", 0, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (datatype == "int") {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int32", allocator_)
        .AddMember<uint64_t>("maximum", 2147483647, allocator_)
        .AddMember<int64_t>("minimum", -2147483648, allocator_)
        .AddMember("example", 0, allocator_);
  } else if (helper::starts_with(datatype, "bigint")) {
    property_details.AddMember("type", "integer", allocator_)
        .AddMember("format", "int64", allocator_)
        .AddMember("example", 0, allocator_);
  } else if (helper::starts_with(datatype, "float")) {
    property_details.AddMember("type", "number", allocator_)
        .AddMember("format", "float", allocator_)
        .AddMember("example", 0.0, allocator_);
  } else if (helper::starts_with(datatype, "double")) {
    property_details.AddMember("type", "number", allocator_)
        .AddMember("format", "double", allocator_)
        .AddMember("example", 0.0, allocator_);
  } else if (helper::starts_with(datatype, "decimal")) {
    property_details.AddMember("type", "number", allocator_)
        .AddMember("format", "decimal", allocator_);
  } else if (helper::starts_with(datatype, "bit")) {
    property_details.AddMember("type", "integer", allocator_);
    const auto shift_maybe = get_type_size(datatype);
    if (shift_maybe) {
      property_details.AddMember(
          "format", *shift_maybe > 32 ? "int64" : "int32", allocator_);
    }
  } else if (datatype == "bool" || datatype == "boolean" ||
             datatype == "tinyint(1)") {
    property_details.AddMember("type", "boolean", allocator_);

  } else if (datatype == "json") {
    property_details.AddMember("type", "object", allocator_);

  } else if (datatype == "binary") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    const auto length_maybe = get_type_size(datatype);
    if (length_maybe) {
      property_details.AddMember("minLength", *length_maybe, allocator_);
      property_details.AddMember("maxLength", *length_maybe, allocator_);
    }
  } else if (datatype == "varbinary") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    const auto length_maybe = get_type_size(datatype);
    if (length_maybe) {
      property_details.AddMember("maxLength", *length_maybe, allocator_);
    }
  } else if (datatype == "tinyblob") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    property_details.AddMember("maxLength", 255, allocator_);
  } else if (datatype == "blob") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    property_details.AddMember("maxLength", 65535, allocator_);
  } else if (datatype == "mediumblob") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    property_details.AddMember("maxLength", 16777215, allocator_);
  } else if (datatype == "longblob") {
    property_details.AddMember("type", "string", allocator_);
    property_details.AddMember("format", "binary", allocator_);
    property_details.AddMember<uint64_t>("maxLength", 4294967295, allocator_);

  } else if (helper::starts_with(datatype, "enum") ||
             helper::starts_with(datatype, "set")) {
    property_details.AddMember("type", "string", allocator_);
    size_t start_pos = datatype.find('(');
    size_t end_pos = datatype.find(')');
    const std::string enum_contents =
        datatype.substr(start_pos + 1, end_pos - start_pos - 1);
    const auto values = mysql_harness::split_string(enum_contents, ',');

    rapidjson::Value values_array(rapidjson::kArrayType);
    for (const auto &value : values) {
      values_array.PushBack(
          rapidjson::Value(value.substr(1, value.length() - 2).c_str(),
                           allocator_),
          allocator_);
    }
    property_details.AddMember("enum", values_array, allocator_);
  } else if (helper::starts_with(datatype, "vector")) {
    property_details.AddMember("type", "array", allocator_);
    property_details.AddMember("items",
                               rapidjson::Value(rapidjson::kObjectType)
                                   .AddMember("type", "number", allocator_)
                                   .AddMember("format", "float", allocator_),
                               allocator_);

    const auto length_maybe = get_type_size(datatype);
    if (length_maybe) {
      property_details.AddMember("minItems", *length_maybe, allocator_);
      property_details.AddMember("maxItems", *length_maybe, allocator_);
    }
  } else if (datatype == "geometry" || datatype == "geomcollection" ||
             datatype == "point" || datatype == "linestring" ||
             datatype == "polygon" || datatype == "multipoint" ||
             datatype == "multilinestring" || datatype == "multipolygon") {
    property_details.AddMember("type", "object", allocator_);
  } else {
    property_details.AddMember(
        "type",
        rapidjson::Value(column_type_to_openapi(type).c_str(), allocator_),
        allocator_);
  }

  return property_details;
}

rapidjson::Value OpenApiCreator::get_delete_response() const {
  rapidjson::Value result{rapidjson::kObjectType};
  result.AddMember("description", "Deleted item(s) count", allocator_)
      .AddMember(
          "content",
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember(
                  "application/json",
                  rapidjson::Value(rapidjson::kObjectType)
                      .AddMember(
                          "schema",
                          rapidjson::Value(rapidjson::kObjectType)
                              .AddMember("type", "object", allocator_)
                              .AddMember(
                                  "properties",
                                  rapidjson::Value(rapidjson::kObjectType)
                                      .AddMember(
                                          "itemsDeleted",
                                          rapidjson::Value(
                                              rapidjson::kObjectType)
                                              .AddMember("type", "integer",
                                                         allocator_),
                                          allocator_),
                                  allocator_),
                          allocator_),
                  allocator_),
          allocator_);

  return result;
}

std::string OpenApiCreator::column_type_to_openapi(
    mrs::database::entry::ColumnType type) const {
  using Column_type = mrs::database::entry::ColumnType;
  switch (type) {
    case Column_type::INTEGER:
    case Column_type::BINARY:
      return "integer";
    case Column_type::DOUBLE:
      return "number";
    case Column_type::JSON:
      return "object";
    case Column_type::BOOLEAN:
      return "boolean";
    default:
      return "string";
  }
}

rapidjson::Value get_route_openapi_component(
    DbObjectPtr entry, rapidjson::Document::AllocatorType &allocator) {
  OpenApiCreator api_creator{entry, allocator};
  return api_creator.create_components();
}

void get_procedure_metadata_component(
    rapidjson::Value &schema_properties,
    rapidjson::Document::AllocatorType &allocator) {
  rapidjson::Value metadata_items(rapidjson::kObjectType);

  metadata_items.AddMember("type", "object", allocator)
      .AddMember("properties",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("name",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "string", allocator)
                                    .AddMember("description", "Column name",
                                               allocator),
                                allocator)
                     .AddMember("type",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "string", allocator)
                                    .AddMember("description", "Column type",
                                               allocator),
                                allocator),
                 allocator);

  rapidjson::Value metadata_def(rapidjson::kObjectType);
  metadata_def.AddMember("type", "object", allocator)
      .AddMember(
          "properties",
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("columns",
                         rapidjson::Value(rapidjson::kObjectType)
                             .AddMember("type", "array", allocator)
                             .AddMember("items", metadata_items, allocator),
                         allocator),
          allocator);

  schema_properties.AddMember("procedure_metadata_def", metadata_def,
                              allocator);
}

rapidjson::Value OpenApiCreator::create_get_method() const {
  rapidjson::Value get_method(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);

  rapidjson::Value response(rapidjson::kObjectType);
  response
      .AddMember(
          "description",
          rapidjson::Value(std::string(entry_->name + " contents").c_str(),
                           allocator_),
          allocator_)
      .AddMember("content", get_content_schema_array(), allocator_);
  responses.AddMember("200", response, allocator_);

  std::string summary{"Get " + entry_->name + " contents"};
  get_method
      .AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                 allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_)
      .AddMember("parameters",
                 rapidjson::Value(rapidjson::kArrayType)
                     .PushBack(get_integer_parameter("limit"), allocator_)
                     .PushBack(get_integer_parameter("offset"), allocator_)
                     .PushBack(get_filter_parameter(/*is_required*/ false),
                               allocator_),
                 allocator_);

  if (entry_->requires_authentication) add_security(get_method);

  return get_method;
}

rapidjson::Value OpenApiCreator::create_get_by_key_method() const {
  rapidjson::Value get(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);

  rapidjson::Value result(rapidjson::kObjectType);

  result
      .AddMember(
          "description",
          rapidjson::Value(std::string(entry_->name + " contents").c_str(),
                           allocator_),
          allocator_)
      .AddMember("content", get_content_schema_single(), allocator_);

  responses.AddMember("200", result, allocator_);
  responses.AddMember("404",
                      rapidjson::Value(rapidjson::kObjectType)
                          .AddMember("description", "Not found", allocator_),
                      allocator_);

  rapidjson::Value get_parameters{parameters_, allocator_};
  std::string summary{"Get " + entry_->name + " contents"};
  get.AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                allocator_)
      .AddMember("parameters",
                 get_parameters.PushBack(
                     get_filter_parameter(/*is_required*/ false), allocator_),
                 allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_);

  if (entry_->requires_authentication) add_security(get);

  return get;
}

rapidjson::Value OpenApiCreator::create_post_method() const {
  rapidjson::Value post_method(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);
  rapidjson::Value request_body(rapidjson::kObjectType);

  request_body.AddMember("description", "Item to create", allocator_)
      .AddMember("required", true, allocator_)
      .AddMember("content", get_content_schema_single(), allocator_);

  responses
      .AddMember("400",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("description", "Invalid input", allocator_),
                 allocator_)
      .AddMember(
          "200",
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("description", "Item successfully created", allocator_)
              .AddMember("content", get_content_schema_single(), allocator_),
          allocator_);

  std::string summary{"Create " + entry_->name + " entry"};

  post_method
      .AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                 allocator_)
      .AddMember("requestBody", request_body, allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_);

  if (entry_->requires_authentication) add_security(post_method);

  return post_method;
}

rapidjson::Value OpenApiCreator::create_delete_method() const {
  rapidjson::Value delete_method(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);

  responses
      .AddMember("404",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("description", "Not found", allocator_),
                 allocator_)
      .AddMember("200", get_delete_response(), allocator_);

  std::string summary{"Delete " + entry_->name + " entry"};
  rapidjson::Value delete_parameters{rapidjson::kArrayType};
  delete_parameters.PushBack(get_filter_parameter(/*is_required*/ true),
                             allocator_);

  delete_method
      .AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                 allocator_)
      .AddMember("parameters", delete_parameters, allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_);

  if (entry_->requires_authentication) add_security(delete_method);

  return delete_method;
}

rapidjson::Value OpenApiCreator::create_delete_by_key_method() const {
  rapidjson::Value delete_method(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);

  responses
      .AddMember("404",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("description", "Not found", allocator_),
                 allocator_)
      .AddMember("200", get_delete_response(), allocator_);

  std::string summary{"Delete " + entry_->name + " entry"};
  rapidjson::Value delete_parameters{parameters_, allocator_};

  delete_method
      .AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                 allocator_)
      .AddMember("parameters", delete_parameters, allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_);

  if (entry_->requires_authentication) add_security(delete_method);

  return delete_method;
}

rapidjson::Value OpenApiCreator::create_put_method() const {
  rapidjson::Value put_method(rapidjson::kObjectType);
  rapidjson::Value responses(rapidjson::kObjectType);
  rapidjson::Value request_body(rapidjson::kObjectType);

  request_body.AddMember("description", "Item to create or update", allocator_)
      .AddMember("required", true, allocator_)
      .AddMember("content", get_content_schema_single(), allocator_);

  responses
      .AddMember("400",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("description", "Invalid input", allocator_),
                 allocator_)
      .AddMember(
          "200",
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("description", "Item successfully created or updated",
                         allocator_)
              .AddMember("content", get_content_schema_single(), allocator_),
          allocator_);

  std::string summary{"Update or create " + entry_->name + " entry"};
  rapidjson::Value put_parameters{parameters_, allocator_};

  put_method
      .AddMember("summary", rapidjson::Value(summary.c_str(), allocator_),
                 allocator_)
      .AddMember("parameters", put_parameters, allocator_)
      .AddMember("requestBody", request_body, allocator_)
      .AddMember("tags", get_tag(), allocator_)
      .AddMember("responses", responses, allocator_);

  if (entry_->requires_authentication) add_security(put_method);

  return put_method;
}

static rapidjson::Value get_http_500_schema(
    rapidjson::Document::AllocatorType &allocator) {
  rapidjson::Value result{rapidjson::kObjectType};
  result.AddMember("type", "object", allocator)
      .AddMember("properties",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("message",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "string", allocator),
                                allocator)
                     .AddMember("status",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator),
                 allocator);
  return result;
}

rapidjson::Value OpenApiCreator::get_procedure_items(
    const std::optional<uint32_t> privileges, const bool is_async) const {
  rapidjson::Value input_properties(rapidjson::kObjectType);
  for (const auto &p : entry_->fields.parameters.fields) {
    if (p.mode == mrs::database::entry::Field::Mode::modeOut) continue;

    auto property_details_res = get_type_info(p.raw_data_type);
    if (property_details_res) {
      input_properties.AddMember(rapidjson::Value(p.name, allocator_),
                                 *property_details_res, allocator_);
    }
  }

  rapidjson::Value call_result(rapidjson::kObjectType);
  if (is_async) {
    call_result.AddMember(
        "202",
        rapidjson::Value(rapidjson::kObjectType)
            .AddMember("description", entry_->name + " async task started",
                       allocator_)
            .AddMember(
                "content",
                rapidjson::Value(rapidjson::kObjectType)
                    .AddMember(
                        "application/json",
                        rapidjson::Value(rapidjson::kObjectType)
                            .AddMember(
                                "schema",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "object", allocator_)
                                    .AddMember(
                                        "properties",
                                        rapidjson::Value(rapidjson::kObjectType)
                                            .AddMember(
                                                "message",
                                                rapidjson::Value(
                                                    rapidjson::kObjectType)
                                                    .AddMember("type", "string",
                                                               allocator_),
                                                allocator_)
                                            .AddMember(
                                                "statusUrl",
                                                rapidjson::Value(
                                                    rapidjson::kObjectType)
                                                    .AddMember("type", "string",
                                                               allocator_),
                                                allocator_)
                                            .AddMember(
                                                "taskId",
                                                rapidjson::Value(
                                                    rapidjson::kObjectType)
                                                    .AddMember("type",
                                                               "integer",
                                                               allocator_),
                                                allocator_),
                                        allocator_),
                                allocator_),
                        allocator_),
                allocator_),
        allocator_);
  } else {
    call_result.AddMember(
        "200",
        rapidjson::Value(rapidjson::kObjectType)
            .AddMember("description", entry_->name + " results", allocator_)
            .AddMember("content", get_content_schema_single(), allocator_),
        allocator_);
  }
  call_result.AddMember(
      "500",
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("description", "Internal Server Error", allocator_)
          .AddMember(
              "content",
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember(
                      "application/json",
                      rapidjson::Value(rapidjson::kObjectType)
                          .AddMember("schema", get_http_500_schema(allocator_),
                                     allocator_),
                      allocator_),
              allocator_),
      allocator_);

  rapidjson::Value request_body(rapidjson::kObjectType);
  request_body.AddMember(
      "content",
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember(
              "application/json",
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember("schema",
                             rapidjson::Value(rapidjson::kObjectType)
                                 .AddMember("description", "Input parameters",
                                            allocator_)
                                 .AddMember("type", "object", allocator_)
                                 .AddMember("properties", input_properties,
                                            allocator_),
                             allocator_),
              allocator_),
      allocator_);

  const std::string type_str =
      entry_->type ==
              mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure
          ? "procedure"
          : "function";

  rapidjson::Value function_detail(rapidjson::kObjectType);
  function_detail
      .AddMember(
          rapidjson::Value("summary", allocator_),
          rapidjson::Value(std::string("Call ") + entry_->name + " " + type_str,
                           allocator_),
          allocator_)
      .AddMember(
          "tags",
          rapidjson::Value(rapidjson::kArrayType)
              .PushBack(rapidjson::Value(std::string{entry_->schema_name + " " +
                                                     type_str + "s"}
                                             .c_str(),
                                         allocator_),
                        allocator_),
          allocator_)
      .AddMember("requestBody", request_body, allocator_)
      .AddMember("responses", call_result, allocator_);

  rapidjson::Value function_item(rapidjson::kObjectType);
  rapidjson::Value post_detail(function_detail, allocator_);

  if (privileges.value_or(mrs::database::entry::Operation::valueUpdate) &
      mrs::database::entry::Operation::valueUpdate) {
    function_item.AddMember("put", function_detail, allocator_);
  }

  if (privileges.value_or(mrs::database::entry::Operation::valueCreate) &
      mrs::database::entry::Operation::valueCreate) {
    function_item.AddMember("post", post_detail, allocator_);
  }

  return function_item;
}

rapidjson::Value OpenApiCreator::get_function_components() const {
  rapidjson::Value result_info(rapidjson::kObjectType);

  if (!entry_->fields.results.empty()) {
    const auto result_detail = entry_->fields.results.at(0);
    if (result_detail.fields.size() == 1) {
      auto item_details_res =
          get_type_info(result_detail.fields[0].raw_data_type);
      if (item_details_res) {
        result_info.AddMember(
            rapidjson::Value(result_detail.fields[0].bind_name, allocator_),
            *item_details_res, allocator_);
      }
    } else {
      log_warning("Wrong result format for %s", entry_->name.c_str());
    }
  }

  rapidjson::Value component_info(rapidjson::kObjectType);
  component_info.AddMember(
      rapidjson::Value(ref_name_, allocator_),
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("type", "object", allocator_)
          .AddMember("properties", result_info, allocator_),
      allocator_);
  return component_info;
}

rapidjson::Value OpenApiCreator::get_procedure_result_example() const {
  rapidjson::Value result(rapidjson::kArrayType);
  for (const auto &p : entry_->fields.results) {
    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("type", p.name, allocator_);

    rapidjson::Value item_details(rapidjson::kObjectType);
    rapidjson::Value metadata_columns(rapidjson::kArrayType);
    for (const auto &field : p.fields) {
      metadata_columns.PushBack(
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("name", field.bind_name, allocator_)
              .AddMember("type", field.raw_data_type, allocator_),
          allocator_);

      auto out_param_details_res = get_type_info(field.raw_data_type);
      if (!out_param_details_res) continue;

      rapidjson::Value example{};
      if (out_param_details_res->HasMember("example")) {
        example = (*out_param_details_res)["example"];
      } else {
        example = rapidjson::Value("", allocator_);
      }

      item_details.AddMember(rapidjson::Value(field.bind_name, allocator_),
                             example, allocator_);
    }

    item.AddMember("items", item_details, allocator_)
        .AddMember("_metadata",
                   rapidjson::Value(rapidjson::kObjectType)
                       .AddMember("columns", metadata_columns, allocator_),
                   allocator_);

    result.PushBack(item, allocator_);
  }
  return result;
}

rapidjson::Value OpenApiCreator::get_procedure_components() const {
  rapidjson::Value out_params(rapidjson::kObjectType);
  for (const auto &p : entry_->fields.parameters.fields) {
    if (p.mode == mrs::database::entry::Field::Mode::modeOut ||
        p.mode == mrs::database::entry::Field::Mode::modeInOut) {
      auto out_param_details_res = get_type_info(p.raw_data_type);
      if (!out_param_details_res) continue;

      rapidjson::Value out_param_details =
          std::move(out_param_details_res.value());

      std::string mode = p.mode == mrs::database::entry::Field::Mode::modeOut
                             ? "OUT"
                             : "INOUT";
      out_param_details.AddMember("description", mode + " parameter",
                                  allocator_);

      out_params.AddMember(rapidjson::Value(p.name, allocator_),
                           out_param_details, allocator_);
    }
  }

  rapidjson::Value items(rapidjson::kArrayType);
  rapidjson::Value item_obj(rapidjson::kObjectType);

  for (const auto &r : entry_->fields.results) {
    rapidjson::Value types(rapidjson::kObjectType);
    for (const auto &p : r.fields) {
      auto item_details_res = get_type_info(p.raw_data_type);

      if (item_details_res) {
        types.AddMember(rapidjson::Value(p.name, allocator_), *item_details_res,
                        allocator_);
      }
    }
    items.PushBack(rapidjson::Value(rapidjson::kObjectType)
                       .AddMember("type", "object", allocator_)
                       .AddMember("description", r.name, allocator_)
                       .AddMember("properties", types, allocator_),
                   allocator_);
  }

  if (entry_->fields.results.size() > 1) {
    item_obj.AddMember("oneOf", items, allocator_);
  } else if (entry_->fields.results.size() == 1) {
    item_obj = items[0].Move();
  }

  rapidjson::Value property(rapidjson::kObjectType);
  property.AddMember(
      "resultSets",
      rapidjson::Value(rapidjson::kObjectType)
          .AddMember("type", "object", allocator_)
          .AddMember("example", get_procedure_result_example(), allocator_)
          .AddMember(
              "properties",
              rapidjson::Value(rapidjson::kObjectType)
                  .AddMember("type",
                             rapidjson::Value(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator_),
                             allocator_)
                  .AddMember("items",
                             rapidjson::Value(rapidjson::kObjectType)
                                 .AddMember("type", "array", allocator_)
                                 .AddMember("items", item_obj, allocator_),
                             allocator_)
                  .AddMember(
                      "_metadata",
                      rapidjson::Value(rapidjson::kObjectType)
                          .AddMember(
                              "$ref",
                              "#/components/schemas/procedure_metadata_def",
                              allocator_),
                      allocator_),
              allocator_),
      allocator_);

  if (!out_params.ObjectEmpty()) {
    property.AddMember("outParams",
                       rapidjson::Value(rapidjson::kObjectType)
                           .AddMember("type", "object", allocator_)
                           .AddMember("properties", out_params, allocator_),
                       allocator_);
  }

  rapidjson::Value component_info(rapidjson::kObjectType);
  component_info.AddMember(rapidjson::Value(ref_name_, allocator_),
                           rapidjson::Value(rapidjson::kObjectType)
                               .AddMember("type", "object", allocator_)
                               .AddMember("properties", property, allocator_),
                           allocator_);
  return component_info;
}

rapidjson::Value get_route_openapi_schema_path(
    const std::optional<uint32_t> privileges, DbObjectPtr entry,
    const std::string &url, const bool is_async,
    rapidjson::Document::AllocatorType &allocator) {
  OpenApiCreator api_creator{entry, allocator};

  rapidjson::Value items(rapidjson::kObjectType);
  rapidjson::Value path_methods(rapidjson::kObjectType);
  rapidjson::Value path_pk_methods(rapidjson::kObjectType);

  if (entry->type ==
          mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure ||
      entry->type ==
          mrs::database::entry::DbObject::ObjectType::k_objectTypeFunction) {
    if (privileges.value_or(mrs::database::entry::Operation::valueUpdate) &
            mrs::database::entry::Operation::valueUpdate ||
        privileges.value_or(mrs::database::entry::Operation::valueCreate) &
            mrs::database::entry::Operation::valueCreate) {
      auto function_item =
          api_creator.get_procedure_items(privileges, is_async);
      items.AddMember(rapidjson::Value(url, allocator), function_item,
                      allocator);
    }

    return items;
  }

  if (privileges.value_or(mrs::database::entry::Operation::valueRead) &
      entry->crud_operation & mrs::database::entry::Operation::valueRead) {
    path_methods.AddMember("get", api_creator.create_get_method(), allocator);
  }
  if (privileges.value_or(mrs::database::entry::Operation::valueCreate) &
      entry->crud_operation & mrs::database::entry::Operation::valueCreate) {
    path_methods.AddMember("post", api_creator.create_post_method(), allocator);
  }
  if (privileges.value_or(mrs::database::entry::Operation::valueDelete) &
      entry->crud_operation & mrs::database::entry::Operation::valueDelete) {
    path_methods.AddMember("delete", api_creator.create_delete_method(),
                           allocator);
  }

  items.AddMember(rapidjson::Value(url, allocator), path_methods, allocator);

  if (api_creator.has_primary_key()) {
    if (privileges.value_or(mrs::database::entry::Operation::valueRead) &
        entry->crud_operation & mrs::database::entry::Operation::valueRead) {
      path_pk_methods.AddMember("get", api_creator.create_get_by_key_method(),
                                allocator);
    }
    if (privileges.value_or(mrs::database::entry::Operation::valueDelete) &
        entry->crud_operation & mrs::database::entry::Operation::valueDelete) {
      path_pk_methods.AddMember(
          "delete", api_creator.create_delete_by_key_method(), allocator);
    }
    if (privileges.value_or(mrs::database::entry::Operation::valueUpdate) &
        entry->crud_operation & mrs::database::entry::Operation::valueUpdate) {
      path_pk_methods.AddMember("put", api_creator.create_put_method(),
                                allocator);
    }

    std::string member_path =
        std::string{url} + "/" + api_creator.primary_key();
    items.AddMember(rapidjson::Value(member_path.c_str(), allocator),
                    rapidjson::Value(path_pk_methods, allocator), allocator);
  }

  return items;
}

bool is_supported(
    const std::shared_ptr<mrs::database::entry::DbObject> &db_obj,
    const std::shared_ptr<mrs::database::entry::DbSchema> &db_schema) {
  namespace entry_ns = mrs::database::entry;

  if (db_schema->enabled != entry_ns::EnabledType::EnabledType_public ||
      db_obj->enabled != entry_ns::EnabledType::EnabledType_public) {
    return false;
  }

  return true;
}

rapidjson::Value add_task_id_endpoint(
    const std::optional<uint32_t> privileges, DbObjectPtr entry,
    rapidjson::Document::AllocatorType &allocator) {
  rapidjson::Value result(rapidjson::kObjectType);
  OpenApiCreator api_creator{entry, allocator};

  rapidjson::Value tags{rapidjson::kArrayType};
  tags.PushBack(rapidjson::Value(
                    std::string{entry->schema_name + "/" + entry->name}.c_str(),
                    allocator),
                allocator);

  rapidjson::Value parameters{rapidjson::kArrayType};
  parameters.PushBack(api_creator.create_parameter("taskId", "integer"),
                      allocator);

  if (privileges.value_or(mrs::database::entry::Operation::valueDelete) &
      mrs::database::entry::Operation::valueDelete) {
    result.AddMember(
        "delete",
        rapidjson::Value(rapidjson::kObjectType)
            .AddMember("summary", "Delete async task", allocator)
            .AddMember("parameters", rapidjson::Value(parameters, allocator),
                       allocator)
            .AddMember("tags", rapidjson::Value(tags, allocator), allocator)
            .AddMember(
                "responses",
                rapidjson::Value(rapidjson::kObjectType)
                    .AddMember(
                        "200",
                        rapidjson::Value(rapidjson::kObjectType)
                            .AddMember("description",
                                       "Task deleted successfully", allocator),
                        allocator),
                allocator),
        allocator);
  }

  rapidjson::Value get_schema{rapidjson::kObjectType};
  get_schema.AddMember("type", "object", allocator)
      .AddMember("properties",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("data",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "object", allocator)
                                    .AddMember("nullable", true, allocator)
                                    .AddMember("additionalProperties", true,
                                               allocator),
                                allocator)
                     .AddMember("status",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "string", allocator),
                                allocator)
                     .AddMember("message",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "string", allocator),
                                allocator)
                     .AddMember("progress",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator),
                 allocator);

  if (privileges.value_or(mrs::database::entry::Operation::valueRead) &
      mrs::database::entry::Operation::valueRead) {
    result.AddMember(
        "get",
        rapidjson::Value(rapidjson::kObjectType)
            .AddMember("summary", "Check async task status", allocator)
            .AddMember("parameters", parameters, allocator)
            .AddMember("tags", tags, allocator)
            .AddMember(
                "responses",
                rapidjson::Value(rapidjson::kObjectType)
                    .AddMember(
                        "200",
                        rapidjson::Value(rapidjson::kObjectType)
                            .AddMember("description", "Async task status",
                                       allocator)
                            .AddMember(
                                "content",
                                rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember(
                                        "application/json",
                                        rapidjson::Value(rapidjson::kObjectType)
                                            .AddMember("schema", get_schema,
                                                       allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }

  return result;
}

}  // namespace rest
}  // namespace mrs
