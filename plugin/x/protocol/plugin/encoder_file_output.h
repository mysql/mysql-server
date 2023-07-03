/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_ENCODER_FILE_OUTPUT_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_ENCODER_FILE_OUTPUT_H_

#include <google/protobuf/descriptor.h>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include "plugin/x/generated/protobuf/mysqlx.pb.h"
MY_COMPILER_DIAGNOSTIC_POP()
#include "plugin/x/protocol/plugin/file_output.h"

class Encoder_file_output : public File_output {
 public:
  using Descriptor = google::protobuf::Descriptor;

 public:
  explicit Encoder_file_output(const std::string &name) : File_output(name) {}

  void write_header(Context *context) override {
    write_to_context(context,
                     "#ifndef PLUGIN_X_GENERATED_ENCODING_DESCRIPTORS_H_");
    write_to_context(context,
                     "#define PLUGIN_X_GENERATED_ENCODING_DESCRIPTORS_H_");
    write_to_context(context, "");
    write_to_context(context, "#include <cstdint>");
    write_to_context(context, "");
    write_to_context(context, "namespace protocol {");
    write_to_context(context, "");
    write_to_context(context, "namespace tags {");
  }

  void write_footer(Context *context) override {
    write_unused_ids(context);
    write_to_context(context, "");
    write_to_context(context, "}  // namespace tags");
    write_to_context(context, "");
    write_to_context(context, "}  // namespace protocol");
    write_to_context(context, "");
    write_to_context(context,
                     "#endif  // PLUGIN_X_GENERATED_ENCODING_DESCRIPTORS_H_");
  }

  void append_message(Context *context, const Descriptor *message) {
    // Skip client only messages
    if (message->options().HasExtension(Mysqlx::client_message_id) &&
        !message->options().HasExtension(Mysqlx::server_message_id))
      return;

    write_to_context(context, "");
    write_to_context(context, "struct ", message->name(), " {");

    if (message->options().HasExtension(Mysqlx::server_message_id)) {
      const auto server_id_numeric = static_cast<int>(
          message->options().GetExtension(Mysqlx::server_message_id));
      const auto server_id = std::to_string(server_id_numeric);

      m_used_message_ids.insert(server_id_numeric);

      write_to_context(
          context, "  static constexpr uint32_t server_id = ", server_id, ";");
      write_to_context(context, "");
    }

    for (int i = 0; i < message->field_count(); ++i) {
      bool is_reserved = false;
      const auto field = message->field(i);
      const auto field_tag = std::to_string(field->number());
      const auto field_name = get_cpp_field_name(field->name(), &is_reserved);

      if (is_reserved) {
        write_to_context(context,
                         "  // The field name is reserved in keyword in C++, "
                         "it was modified");
        write_to_context(
            context,
            "  // to fix potential compilation issues and improve readability");
      }
      write_to_context(context, "  static constexpr uint32_t ", field_name,
                       " = ", field_tag, ";");
    }

    write_to_context(context, "};");
  }

 private:
  /*
   The new encoder might also look for those missing IDs in
   mysqlx.pb.h, still because some subset of IDs is already in tags
   namespace, thus to be consistent lets put all IDs to the generated
   file.
  */
  void write_unused_ids(Context *context) {
    auto descriptor = Mysqlx::ServerMessages_Type_descriptor();
    std::vector<std::string> values;
    for (int i = 0; i < descriptor->value_count(); ++i) {
      const auto enum_value = descriptor->value(i);

      // Check if we there is a message containing Server message ID
      if (0 == m_used_message_ids.count(enum_value->number())) {
        std::string value = "  " + enum_value->name() + " = " +
                            std::to_string(enum_value->number());

        values.push_back(value);
      }
    }

    if (!values.empty()) {
      write_to_context(context, "");
      write_to_context(context, "enum Raw_payload_ids {");
      for (size_t i = 0; i < values.size(); ++i) {
        const bool is_last = i == values.size() - 1;
        write_to_context(context, values[i], is_last ? "" : ",");
      }
      write_to_context(context, "};");
    }
  }

  std::string get_cpp_field_name(std::string name, bool *is_reserved) {
    for (auto &c : name) {
      c = ::tolower(c);
    }

    // Check for reserved words
    if (name == "namespace" || name == "operator") {
      if (is_reserved) *is_reserved = true;
      name = name + "_";
    }

    return name;
  }

  std::set<int> m_used_message_ids;
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_ENCODER_FILE_OUTPUT_H_
