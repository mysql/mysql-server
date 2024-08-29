/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mysql/components/library_mysys/my_hex_tools.h"

#include "json_writer.h"

namespace keyring_common::json_data {

/**
  Constructor

  @param [in] data        JSON data
  @param [in] version     Version information
  @param [in] version_key Writer version key
  @param [in] array_key   Key to array that stores all elements
*/
Json_writer::Json_writer(const std::string &data /* = {} */,
                         const std::string &version /* = "1.0" */,
                         const std::string &version_key /* = "version" */,
                         const std::string &array_key /* = "elements" */)
    : version_key_(version_key),
      array_key_(array_key),
      valid_(version.length() > 0 && version_key.length() > 0 &&
             array_key.length() > 0) {
  if (valid_ && data.length() == 0) {
    document_.SetObject();
    rapidjson::Document::AllocatorType &allocator = document_.GetAllocator();
    rapidjson::Value version_value(rapidjson::kObjectType);
    version_value.SetString(version.c_str(),
                            static_cast<rapidjson::SizeType>(version.length()),
                            allocator);
    document_.AddMember(rapidjson::StringRef(version_key_.c_str()),
                        version_value, allocator);
    rapidjson::Value elements(rapidjson::kArrayType);
    document_.AddMember(rapidjson::StringRef(array_key_.c_str()), elements,
                        allocator);
  } else {
    valid_ &= !document_.Parse(data).HasParseError();
  }
}

bool Json_writer::set_data(const std::string &data) {
  valid_ = !document_.Parse(data).HasParseError();
  return valid_;
}

/** Get string representation of the JSON document */
std::string Json_writer::to_string() const {
  if (!valid_) return std::string{};
  rapidjson::StringBuffer string_buffer;
  string_buffer.Clear();
  rapidjson::Writer<rapidjson::StringBuffer> string_writer(string_buffer);
  document_.Accept(string_writer);
  return {string_buffer.GetString(), string_buffer.GetSize()};
}

bool Json_writer::add_element(const meta::Metadata &metadata,
                              const data::Data &data, Json_data_extension &) {
  rapidjson::Document::AllocatorType &allocator = document_.GetAllocator();
  rapidjson::Value array_element(rapidjson::kObjectType);
  rapidjson::Value element_member(rapidjson::kObjectType);

  /* Add user */
  element_member.SetString(
      metadata.owner_id().c_str(),
      static_cast<rapidjson::SizeType>(metadata.owner_id().length()),
      allocator);
  array_element.AddMember("user", element_member, allocator);

  /* Add data id */
  element_member.SetString(
      metadata.key_id().c_str(),
      static_cast<rapidjson::SizeType>(metadata.key_id().length()), allocator);
  array_element.AddMember("data_id", element_member, allocator);

  /* Add data type */
  element_member.SetString(
      data.type().c_str(),
      static_cast<rapidjson::SizeType>(data.type().length()), allocator);
  array_element.AddMember("data_type", element_member, allocator);

  /* Add data */
  std::string hex_data(data.data().size() * 2, '\0');
  (void)hex_string(hex_data.data(), data.data().c_str(), data.data().size());
  hex_data.shrink_to_fit();
  element_member.SetString(hex_data.c_str(),
                           static_cast<rapidjson::SizeType>(hex_data.size()),
                           allocator);
  array_element.AddMember("data", element_member, allocator);

  /* Add extension */
  rapidjson::Value extension_array(rapidjson::kArrayType);
  array_element.AddMember("extension", extension_array, allocator);

  /* Now add the newly constructed object to the array */
  document_[array_key_].PushBack(array_element, allocator);

  return document_.HasParseError();
}

bool Json_writer::remove_element(const meta::Metadata &metadata,
                                 const Json_data_extension &) {
  bool retval = true;
  if (!valid_) return retval;

  rapidjson::Value &elements = document_[array_key_.c_str()];
  if (!elements.IsArray()) return retval;

  for (rapidjson::Value::ConstValueIterator it = elements.Begin();
       it != elements.End();) {
    const meta::Metadata current_metadata((*it)["data_id"].Get<std::string>(),
                                          (*it)["user"].Get<std::string>());
    if (metadata == current_metadata) {
      it = elements.Erase(it);  // Erase will move iterator to next position
      retval = false;
    } else
      ++it;
  }
  return retval;
}

/**
  Get number of elements in the document

  @returns number elements in the document
*/
size_t Json_writer::num_elements() const {
  if (!valid_) return 0;
  return static_cast<size_t>(document_[array_key_.c_str()].Size());
}

bool Json_writer::set_property(const std::string &property_key,
                               const std::string &property) {
  if (!valid_) return true;
  rapidjson::Document::AllocatorType &allocator = document_.GetAllocator();
  rapidjson::Value property_value(rapidjson::kObjectType);
  property_value.SetString(property.c_str(),
                           static_cast<rapidjson::SizeType>(property.length()),
                           allocator);
  document_.AddMember(rapidjson::StringRef(property_key.c_str()),
                      property_value, allocator);
  return false;
}

}  // namespace keyring_common::json_data
