/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/formatters/message_formatter.h"

#include <algorithm>
#include <vector>

#include "plugin/x/src/helper/to_string.h"

#ifdef GetMessage
#undef GetMessage
#endif

namespace formatter {

using FieldDescriptor = google::protobuf::FieldDescriptor;
using FieldDescriptors = std::vector<const FieldDescriptor *>;
using Reflection = google::protobuf::Reflection;
using Message = xcl::XProtocol::Message;

namespace details {

/**
  Extract fields from a path (path of fields).

  The patch represents fields (objects) that have nested other objects.
  Using a path, user can choose concrete field inside a large message.
  Characters that can be used in fields name are following:

  * '[a-z]'
  * '[A-Z]'
  * '[0-9]'
  * '_'

  Fields inside a path are separated by dot ("."). When a field points to
  an array then user must use by zero-based-index specified inside square
  brackets.


  Path example
  ------------

  * msg1_field1
  * msg1_field1.field1.field2
  * field1[1].field1[0]
  * field1[1].field2
 */
class Field_path_extractor {
 public:
  explicit Field_path_extractor(const std::string &path);

  std::string get_next_fields() const;
  bool get_current_field(std::string *out_result) const;
  bool get_index(bool *out_has_index, int *out_index) const;

 private:
  std::string m_path;
  std::string m_field_full_name;
  std::size_t m_index;
};

Field_path_extractor::Field_path_extractor(const std::string &path)
    : m_path(path),
      m_field_full_name(path.substr(0, path.find('.'))),
      m_index(m_field_full_name.find('[')) {}

std::string Field_path_extractor::get_next_fields() const {
  const std::size_t position = m_field_full_name.length() + 1;
  if (m_path.length() <= position) return "";

  return m_path.substr(position);
}

bool Field_path_extractor::get_current_field(std::string *out_result) const {
  *out_result = m_field_full_name.substr(0, m_index);

  for (std::size_t i = 0; i < out_result->length(); ++i) {
    const char c = (*out_result)[i];

    if (!std::isalnum(c) && '_' != c) return false;
  }

  return true;
}

bool Field_path_extractor::get_index(bool *out_has_index,
                                     int *out_index) const {
  const bool has_index = std::string::npos != m_index;
  if (has_index) {
    if (m_field_full_name.find(']') != m_field_full_name.length() - 1) {
      return false;
    }

    std::string index_str = m_field_full_name.substr(
        m_index + 1, m_field_full_name.length() - 1 - m_index - 1);
    bool valid_index = !index_str.empty();

    for (std::size_t i = 0; i < index_str.length(); ++i)
      valid_index = valid_index && std::isdigit(index_str[i]);

    if (!valid_index) {
      return false;
    }

    *out_index = std::stoi(index_str);
  }

  *out_has_index = has_index;

  return true;
}

class Field {
 public:
  std::string m_name;
  bool m_has_index;
  int m_index;

  bool operator()(const FieldDescriptor *fd) const {
    return m_name == fd->name();
  }
};

typedef std::vector<Field> Fields;

static Fields get_fields_array_from_path(std::string path) {
  Fields result;

  while (!path.empty()) {
    Field_path_extractor path_extractor(path);
    Field field;

    if (!path_extractor.get_current_field(&field.m_name))
      throw std::logic_error("Elements name contains not allowed characters");

    if (!path_extractor.get_index(&field.m_has_index, &field.m_index))
      throw std::logic_error("Wrong filter format, around elements index");

    path = path_extractor.get_next_fields();
    result.push_back(field);
  }

  return result;
}

}  // namespace details

template <typename Message_type>
static std::string message_to_text(const std::string &binary_message) {
  std::string result;
  Message_type message;

  message.ParseFromString(binary_message);
  google::protobuf::TextFormat::PrintToString(message, &result);

  return message.GetDescriptor()->full_name() + " { " + result + " }";
}

static std::string messages_field_to_text(const Message &message,
                                          const FieldDescriptor *fd) {
  const Reflection *reflection = message.GetReflection();

  switch (fd->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return xpl::to_string(reflection->GetInt32(message, fd));

    case FieldDescriptor::CPPTYPE_UINT32:
      return xpl::to_string(reflection->GetUInt32(message, fd));

    case FieldDescriptor::CPPTYPE_INT64:
      return xpl::to_string(reflection->GetInt64(message, fd));

    case FieldDescriptor::CPPTYPE_UINT64:
      return xpl::to_string(reflection->GetUInt64(message, fd));

    case FieldDescriptor::CPPTYPE_DOUBLE:
      return xpl::to_string(reflection->GetDouble(message, fd));

    case FieldDescriptor::CPPTYPE_FLOAT:
      return xpl::to_string(reflection->GetFloat(message, fd));

    case FieldDescriptor::CPPTYPE_BOOL:
      return xpl::to_string(reflection->GetBool(message, fd));

    case FieldDescriptor::CPPTYPE_ENUM:
      return reflection->GetEnum(message, fd)->name();

    case FieldDescriptor::CPPTYPE_STRING:
      return reflection->GetString(message, fd);

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return message_to_text(reflection->GetMessage(message, fd));

    default:
      throw std::logic_error("Unknown protobuf message type");
  }
}

static std::string messages_repeated_field_to_text(const Message &message,
                                                   const FieldDescriptor *fd,
                                                   const int index) {
  const Reflection *reflection = message.GetReflection();

  switch (fd->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return xpl::to_string(reflection->GetRepeatedInt32(message, fd, index));

    case FieldDescriptor::CPPTYPE_UINT32:
      return xpl::to_string(reflection->GetRepeatedUInt32(message, fd, index));

    case FieldDescriptor::CPPTYPE_INT64:
      return xpl::to_string(reflection->GetRepeatedInt64(message, fd, index));

    case FieldDescriptor::CPPTYPE_UINT64:
      return xpl::to_string(reflection->GetRepeatedUInt64(message, fd, index));

    case FieldDescriptor::CPPTYPE_DOUBLE:
      return xpl::to_string(reflection->GetRepeatedDouble(message, fd, index));

    case FieldDescriptor::CPPTYPE_FLOAT:
      return xpl::to_string(reflection->GetRepeatedFloat(message, fd, index));

    case FieldDescriptor::CPPTYPE_BOOL:
      return xpl::to_string(reflection->GetRepeatedBool(message, fd, index));

    case FieldDescriptor::CPPTYPE_ENUM:
      return reflection->GetRepeatedEnum(message, fd, index)->name();

    case FieldDescriptor::CPPTYPE_STRING:
      return reflection->GetRepeatedString(message, fd, index);

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return message_to_text(
          reflection->GetRepeatedMessage(message, fd, index));

    default:
      throw std::logic_error("Unknown protobuf message type");
  }
}

std::string message_to_text(const Message &message) {
  std::string output;
  std::string name;
  google::protobuf::TextFormat::Printer printer;

  printer.SetInitialIndentLevel(1);

  // special handling for nested messages (at least for Notices)
  if (message.GetDescriptor()->full_name() == "Mysqlx.Notice.Frame") {
    Mysqlx::Notice::Frame frame =
        *static_cast<const Mysqlx::Notice::Frame *>(&message);

    switch (frame.type()) {
      case ::Mysqlx::Notice::Frame_Type_WARNING: {
        const auto payload_as_text =
            message_to_text<Mysqlx::Notice::Warning>(frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
      case ::Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED: {
        const auto payload_as_text =
            message_to_text<Mysqlx::Notice::SessionVariableChanged>(
                frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
      case ::Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED: {
        const auto payload_as_text =
            message_to_text<Mysqlx::Notice::SessionStateChanged>(
                frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }

      case ::Mysqlx::Notice::Frame_Type_GROUP_REPLICATION_STATE_CHANGED: {
        const auto payload_as_text =
            message_to_text<Mysqlx::Notice::GroupReplicationStateChanged>(
                frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
    }

    printer.PrintToString(frame, &output);
  } else {
    printer.PrintToString(message, &output);
  }

  return message.GetDescriptor()->full_name() + " {\n" + output + "}\n";
}

/**
  Message in text format.

  The message_path must be constructed according to format described
  by Field_path_extractor, with following limitation:

  * printing of field which is array (a message or scalar needs to be
    selected)
*/
std::string message_to_text(const Message &message,
                            const std::string &field_path,
                            const bool show_message_name) {
  if (field_path.empty()) return message_to_text(message);

  const FieldDescriptor *field_descriptor = nullptr;
  const Message *msg = &message;
  details::Fields fields = details::get_fields_array_from_path(field_path);
  std::size_t index_of_last_element = fields.size() - 1;

  for (std::size_t field_index = 0; field_index < fields.size();
       ++field_index) {
    const bool is_last_element = index_of_last_element == field_index;
    FieldDescriptors output;
    const details::Field &expected_field = fields[field_index];
    const Reflection *reflection = msg->GetReflection();

    reflection->ListFields(*msg, &output);

    FieldDescriptors::iterator i =
        std::find_if(output.begin(), output.end(), expected_field);

    if (output.end() == i) {
      throw std::logic_error("Message '" + msg->GetDescriptor()->full_name() +
                             "' doesn't contains field '" +
                             expected_field.m_name +
                             "'"
                             " or the field isn't set");
    }

    field_descriptor = *i;

    if (field_descriptor->is_repeated() != expected_field.m_has_index) {
      throw std::logic_error(expected_field.m_has_index
                                 ? "Element '" + expected_field.m_name +
                                       "' isn't an array"
                                 : "Element '" + expected_field.m_name +
                                       "' is an array and requires "
                                       "an index");
    }

    if (!is_last_element) {
      if (FieldDescriptor::CPPTYPE_MESSAGE != field_descriptor->cpp_type()) {
        throw std::logic_error(
            "Path must point to a message for "
            "all elements except last");
      }

      /*
        Move the msg pointer to the selected field
      */
      if (expected_field.m_has_index) {
        const int field_array_elements =
            reflection->FieldSize(*msg, field_descriptor);

        if (expected_field.m_index >= field_array_elements) {
          throw std::logic_error("Elements '" + expected_field.m_name +
                                 "' index out of boundary "
                                 "(size of the array is " +
                                 xpl::to_string(field_array_elements) + ")");
        }

        msg = &reflection->GetRepeatedMessage(*msg, field_descriptor,
                                              expected_field.m_index);
      } else {
        msg = &reflection->GetMessage(*msg, field_descriptor);
      }
    }
  }

  if (!field_descriptor)
    throw std::logic_error("Elements descriptor is missing");

  if (field_descriptor->is_repeated() &&
      !fields[index_of_last_element].m_has_index)
    throw std::logic_error("Last selected element is a repeated field");

  std::string prefix = "";

  if (show_message_name)
    prefix = message.GetDescriptor()->full_name() + "(" + field_path + ") = ";

  if (!field_descriptor->is_repeated())
    return prefix + messages_field_to_text(*msg, field_descriptor);

  return prefix +
         messages_repeated_field_to_text(*msg, field_descriptor,
                                         fields[index_of_last_element].m_index);
}

}  // namespace formatter
