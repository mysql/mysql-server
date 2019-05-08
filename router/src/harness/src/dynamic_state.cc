/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/dynamic_state.h"

#include "common.h"

#include <fstream>
#include <stdexcept>

#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "dynamic_state_schema.h"

namespace {
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonSchemaDocument =
    rapidjson::GenericSchemaDocument<JsonValue, rapidjson::CrtAllocator>;
using JsonSchemaValidator =
    rapidjson::GenericSchemaValidator<JsonSchemaDocument>;

constexpr const char *kVersionFieldName = "version";
constexpr unsigned kVersionMajor = 1;
constexpr unsigned kVersionMinor = 0;
constexpr unsigned kVersionPatch = 0;

std::string to_string(unsigned major, unsigned minor, unsigned patch) {
  return std::to_string(major) + "." + std::to_string(minor) + "." +
         std::to_string(patch);
}

}  // namespace

namespace mysql_harness {

struct DynamicState::Pimpl {
  JsonDocument json_state_doc_;
  std::mutex json_state_doc_lock_;
  std::mutex json_file_lock_;

  /*static*/
  void validate_json_against_schema(const JsonSchemaDocument &schema,
                                    const JsonDocument &json) {
    // verify JSON against the schema
    JsonSchemaValidator validator(schema);
    if (!json.Accept(validator)) {
      // validation failed - throw an error with info of where the problem is
      rapidjson::StringBuffer sb_schema;
      validator.GetInvalidSchemaPointer().StringifyUriFragment(sb_schema);
      rapidjson::StringBuffer sb_json;
      validator.GetInvalidDocumentPointer().StringifyUriFragment(sb_json);
      throw std::runtime_error(
          std::string("Failed schema directive: ") + sb_schema.GetString() +
          "\nFailed schema keyword:   " + validator.GetInvalidSchemaKeyword() +
          "\nFailure location in validated document: " + sb_json.GetString() +
          "\n");
    }
  }
};

DynamicState::DynamicState(const std::string &file_name)
    : file_name_(file_name) {
  pimpl_.reset(new Pimpl());
  pimpl_->json_state_doc_.SetObject();
}

DynamicState::~DynamicState() {}

std::ifstream DynamicState::open_for_read() {
  std::ifstream input_file(file_name_);
  if (input_file.fail()) {
    throw std::runtime_error(
        "Could not open dynamic state file '" + file_name_ +
        "' for reading: " + mysql_harness::get_strerror(errno));
  }

  return input_file;
}

std::ofstream DynamicState::open_for_write() {
  std::ofstream output_file(file_name_);
  if (output_file.fail()) {
    throw std::runtime_error(
        "Could not open dynamic state file '" + file_name_ +
        "' for writing: " + mysql_harness::get_strerror(errno));
  }

  return output_file;
}

bool DynamicState::load() {
  std::unique_lock<std::mutex> lock(pimpl_->json_file_lock_);
  bool result{false};

  auto input_file = open_for_read();
  try {
    result = load_from_stream(input_file);
  } catch (const std::runtime_error &e) {
    throw std::runtime_error("Error parsing file dynamic state file '" +
                             file_name_ + "': " + e.what());
  }

  return result;
}

void DynamicState::ensure_valid_against_schema() {
  // construct schema JSON; throws std::runtime_error on invalid JSON, but note
  JsonDocument schema_json;
  if (schema_json
          .Parse<rapidjson::kParseCommentsFlag>(StateFileJsonSchema::data(),
                                                StateFileJsonSchema::size())
          .HasParseError())
    throw std::runtime_error(
        "Parsing JSON schema failed at offset " +
        std::to_string(schema_json.GetErrorOffset()) + ": " +
        rapidjson::GetParseError_En(schema_json.GetParseError()));
  JsonSchemaDocument schema(schema_json);

  // validate JSON against schema; throws std::runtime_error if validation fails
  try {
    pimpl_->validate_json_against_schema(schema, pimpl_->json_state_doc_);
  } catch (const std::runtime_error &e) {
    throw std::runtime_error(
        std::string("JSON file failed validation against JSON schema: ") +
        e.what());
  }
}

void DynamicState::ensure_version_compatibility() {
  // we do it before validating against the schema so we need to
  // do some initial parsing manually here
  auto &json_doc = pimpl_->json_state_doc_;

  // the whole document has to be an object:
  if (!json_doc.IsObject()) {
    throw std::runtime_error(
        std::string("Invalid json structure: not an object"));
  }

  // it has to have version field
  if (!json_doc.GetObject().HasMember(kVersionFieldName)) {
    throw std::runtime_error(
        std::string("Invalid json structure: missing field: ") +
        kVersionFieldName);
  }

  // this field should be string
  auto &version_field = json_doc.GetObject()[kVersionFieldName];
  if (!version_field.IsString()) {
    throw std::runtime_error(std::string("Invalid json structure: field ") +
                             kVersionFieldName + " should be a string type");
  }

  // the format od the string should be MAJOR.MINOR.PATCH
  std::string version = version_field.GetString();
  unsigned major{0}, minor{0}, patch{0};
  int res = sscanf(version.c_str(), "%u.%u.%u", &major, &minor, &patch);
  if (res != 3) {
    throw std::runtime_error(
        std::string("Invalid version field format, expected MAJOR.MINOR.PATCH, "
                    "found: ") +
        version);
  }

  // the major and minor should match match exactly, different patch is fine
  if (major != kVersionMajor || minor != kVersionMinor) {
    throw std::runtime_error(
        std::string("Unsupported state file version, expected: ") +
        to_string(kVersionMajor, kVersionMinor, kVersionPatch) +
        ", found: " + to_string(major, minor, patch));
  }

  // all good, version matches, go back to the caller with no exception
}

bool DynamicState::load_from_stream(std::istream &input_stream) {
  rapidjson::IStreamWrapper istream(input_stream);

  auto &json_doc = pimpl_->json_state_doc_;

  std::unique_lock<std::mutex> lock(pimpl_->json_state_doc_lock_);

  if (json_doc.ParseStream<rapidjson::kParseCommentsFlag>(istream)
          .HasParseError()) {
    throw std::runtime_error(
        "Parsing JSON failed at offset " +
        std::to_string(json_doc.GetErrorOffset()) + ": " +
        rapidjson::GetParseError_En(json_doc.GetParseError()));
  }

  ensure_version_compatibility();
  ensure_valid_against_schema();
  return true;
}

bool DynamicState::save(bool pretty) {
  std::unique_lock<std::mutex> lock(pimpl_->json_file_lock_);

  auto output_file = open_for_write();

  return save_to_stream(output_file, pretty);
}

bool DynamicState::save_to_stream(std::ostream &output_stream, bool pretty) {
  JsonStringBuffer out_buffer;

  // save/update the version
  std::string ver_str = to_string(kVersionMajor, kVersionMinor, kVersionPatch);
  JsonValue version(rapidjson::kStringType);
  version.SetString(ver_str.c_str(), ver_str.length());
  update_section(kVersionFieldName, std::move(version));

  std::unique_lock<std::mutex> lock(pimpl_->json_state_doc_lock_);
  if (pretty) {
    rapidjson::PrettyWriter<JsonStringBuffer> out_writer{out_buffer};
    pimpl_->json_state_doc_.Accept(out_writer);
  } else {
    rapidjson::Writer<JsonStringBuffer> out_writer{out_buffer};
    pimpl_->json_state_doc_.Accept(out_writer);
  }
  output_stream << out_buffer.GetString();

  return true;
}

std::unique_ptr<JsonValue> DynamicState::get_section(
    const std::string &section_name) {
  std::unique_lock<std::mutex> lock(pimpl_->json_state_doc_lock_);

  auto &json_doc = pimpl_->json_state_doc_;
  if (!json_doc.HasMember(section_name.c_str())) return nullptr;

  auto &allocator = json_doc.GetAllocator();
  auto &section = json_doc[section_name.c_str()];

  return std::unique_ptr<JsonValue>(new JsonValue(section, allocator));
}

bool DynamicState::update_section(const std::string &section_name,
                                  JsonValue &&value) {
  std::unique_lock<std::mutex> lock(pimpl_->json_state_doc_lock_);

  auto &json_doc = pimpl_->json_state_doc_;
  auto &allocator = json_doc.GetAllocator();

  if (!json_doc.HasMember(section_name.c_str())) {
    json_doc.AddMember(JsonValue(section_name.c_str(), allocator), value,
                       allocator);
  } else {
    auto &section = json_doc[section_name.c_str()];
    section = std::move(value);
  }

  return true;
}

}  // namespace mysql_harness
