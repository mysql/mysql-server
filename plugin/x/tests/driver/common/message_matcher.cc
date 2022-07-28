/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/common/message_matcher.h"
#include "plugin/x/tests/driver/parsers/message_parser.h"

#include <vector>

#ifdef GetMessage
#undef GetMessage
#endif

using Message = xcl::XProtocol::Message;
using FieldDescriptor = google::protobuf::FieldDescriptor;
using CppType = FieldDescriptor::CppType;

namespace {

int field_size(const Message &msg, const FieldDescriptor *field) {
  return msg.GetReflection()->FieldSize(msg, field);
}

bool has_field(const Message &msg, const FieldDescriptor *field) {
  return msg.GetReflection()->HasField(msg, field);
}

bool match_field_value(const Message &expected_msg, const Message &msg,
                       const FieldDescriptor *field) {
  if (expected_msg.GetTypeName() == Mysqlx::Notice::Frame().GetTypeName() &&
      field->name() == "payload") {
    auto expected_notice =
        reinterpret_cast<const Mysqlx::Notice::Frame *>(&expected_msg);
    auto notice = reinterpret_cast<const Mysqlx::Notice::Frame *>(&msg);

    if (expected_notice->has_type() && notice->has_type() &&
        expected_notice->type() == notice->type()) {
      std::unique_ptr<Message> expected_notice_payload{
          parser::get_notice_message_from_text(
              static_cast<Mysqlx::Notice::Frame_Type>(expected_notice->type()),
              "", nullptr, true)};
      std::unique_ptr<Message> actual_notice_payload{
          parser::get_notice_message_from_text(
              static_cast<Mysqlx::Notice::Frame_Type>(expected_notice->type()),
              "", nullptr, true)};

      if (nullptr == expected_notice_payload.get()) {
        return nullptr == actual_notice_payload.get();
      }

      if (!actual_notice_payload->ParsePartialFromString(notice->payload()))
        return false;

      if (!expected_notice_payload->ParsePartialFromString(
              expected_notice->payload()))
        return false;

      return message_match_with_expectations(*expected_notice_payload,
                                             *actual_notice_payload);
    }
  }
  switch (field->cpp_type()) {
    case CppType::CPPTYPE_BOOL:
      return expected_msg.GetReflection()->GetBool(expected_msg, field) ==
             msg.GetReflection()->GetBool(msg, field);

    case CppType::CPPTYPE_DOUBLE:
      return expected_msg.GetReflection()->GetDouble(expected_msg, field) ==
             msg.GetReflection()->GetDouble(msg, field);

    case CppType::CPPTYPE_FLOAT:
      return expected_msg.GetReflection()->GetFloat(expected_msg, field) ==
             msg.GetReflection()->GetFloat(msg, field);

    case CppType::CPPTYPE_ENUM:
      return expected_msg.GetReflection()
                 ->GetEnum(expected_msg, field)
                 ->name() == msg.GetReflection()->GetEnum(msg, field)->name();

    case CppType::CPPTYPE_INT32:
      return expected_msg.GetReflection()->GetInt32(expected_msg, field) ==
             msg.GetReflection()->GetInt32(msg, field);

    case CppType::CPPTYPE_INT64:
      return expected_msg.GetReflection()->GetInt64(expected_msg, field) ==
             msg.GetReflection()->GetInt64(msg, field);

    case CppType::CPPTYPE_MESSAGE:
      return message_match_with_expectations(
          expected_msg.GetReflection()->GetMessage(expected_msg, field),
          msg.GetReflection()->GetMessage(msg, field));

    case CppType::CPPTYPE_STRING:
      return expected_msg.GetReflection()->GetString(expected_msg, field) ==
             msg.GetReflection()->GetString(msg, field);

    case CppType::CPPTYPE_UINT32:
      return expected_msg.GetReflection()->GetUInt32(expected_msg, field) ==
             msg.GetReflection()->GetUInt32(msg, field);

    case CppType::CPPTYPE_UINT64:
      return expected_msg.GetReflection()->GetUInt64(expected_msg, field) ==
             msg.GetReflection()->GetUInt64(msg, field);

    default:
      return false;
  }
}

bool match_field_value(const Message &expected_msg, const Message &msg,
                       const FieldDescriptor *field, const int index) {
  switch (field->cpp_type()) {
    case CppType::CPPTYPE_BOOL:
      return expected_msg.GetReflection()->GetRepeatedBool(expected_msg, field,
                                                           index) ==
             msg.GetReflection()->GetRepeatedBool(msg, field, index);

    case CppType::CPPTYPE_DOUBLE:
      return expected_msg.GetReflection()->GetRepeatedDouble(expected_msg,
                                                             field, index) ==
             msg.GetReflection()->GetRepeatedDouble(msg, field, index);

    case CppType::CPPTYPE_FLOAT:
      return expected_msg.GetReflection()->GetRepeatedFloat(expected_msg, field,
                                                            index) ==
             msg.GetReflection()->GetRepeatedFloat(msg, field, index);

    case CppType::CPPTYPE_ENUM:
      return expected_msg.GetReflection()
                 ->GetRepeatedEnum(expected_msg, field, index)
                 ->name() ==
             msg.GetReflection()->GetRepeatedEnum(msg, field, index)->name();

    case CppType::CPPTYPE_INT32:
      return expected_msg.GetReflection()->GetRepeatedInt32(expected_msg, field,
                                                            index) ==
             msg.GetReflection()->GetRepeatedInt32(msg, field, index);

    case CppType::CPPTYPE_INT64:
      return expected_msg.GetReflection()->GetRepeatedInt64(expected_msg, field,
                                                            index) ==
             msg.GetReflection()->GetRepeatedInt64(msg, field, index);

    case CppType::CPPTYPE_MESSAGE:
      return message_match_with_expectations(
          expected_msg.GetReflection()->GetRepeatedMessage(expected_msg, field,
                                                           index),
          msg.GetReflection()->GetRepeatedMessage(msg, field, index));

    case CppType::CPPTYPE_STRING:
      return expected_msg.GetReflection()->GetRepeatedString(expected_msg,
                                                             field, index) ==
             msg.GetReflection()->GetRepeatedString(msg, field, index);

    case CppType::CPPTYPE_UINT32:
      return expected_msg.GetReflection()->GetRepeatedUInt32(expected_msg,
                                                             field, index) ==
             msg.GetReflection()->GetRepeatedUInt32(msg, field, index);

    case CppType::CPPTYPE_UINT64:
      return expected_msg.GetReflection()->GetRepeatedUInt64(expected_msg,
                                                             field, index) ==
             msg.GetReflection()->GetRepeatedUInt64(msg, field, index);

    default:
      return false;
  }
}

}  // namespace

bool message_match_with_expectations(const Message &expected_msg,
                                     const Message &msg) {
  std::vector<const FieldDescriptor *> expected_fields;

  if (expected_msg.GetTypeName() != msg.GetTypeName()) return false;

  expected_msg.GetReflection()->ListFields(expected_msg, &expected_fields);

  for (const auto field : expected_fields) {
    if (!field->is_repeated()) {
      if (has_field(expected_msg, field) && !has_field(msg, field))
        return false;

      if (!match_field_value(expected_msg, msg, field)) return false;
    } else {
      const auto expected_size = field_size(expected_msg, field);
      const auto actual_size = field_size(msg, field);

      if (expected_size != actual_size) return false;

      for (int i = 0; i < expected_size; ++i) {
        if (!match_field_value(expected_msg, msg, field, i)) return false;
      }
    }
  }

  return true;
}
