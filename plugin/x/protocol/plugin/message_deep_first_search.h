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

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_DEEP_FIRST_SEARCH_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_DEEP_FIRST_SEARCH_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <memory>
#include <set>
#include <string>

class Message_deep_first_search {
 public:
  using Descriptor = google::protobuf::Descriptor;
  using FieldDescriptor = google::protobuf::FieldDescriptor;

  Message_deep_first_search() = default;
  virtual ~Message_deep_first_search() = default;

  virtual bool begin_validate_field(const FieldDescriptor *field,
                                    const Descriptor *message_descriptor) = 0;

  virtual void end_validate_field(const FieldDescriptor *field,
                                  const Descriptor *message_descriptor) = 0;

  virtual void indeep_search(const Descriptor *message_descriptor) {
    if (!begin_validate_field(nullptr, message_descriptor)) return;

    for (int i = 0; i < message_descriptor->field_count(); ++i) {
      auto subfield = message_descriptor->field(i);

      if (nullptr == subfield) continue;

      indeep_search_children(subfield);
    }

    end_validate_field(nullptr, message_descriptor);
  }

  virtual void indeep_search_children(const FieldDescriptor *field) {
    const Descriptor *message_descriptor = nullptr;

    if (FieldDescriptor::TYPE_MESSAGE == field->type() ||
        FieldDescriptor::TYPE_GROUP == field->type())
      message_descriptor = field->message_type();

    if (!begin_validate_field(field, message_descriptor)) return;

    if (message_descriptor) {
      for (int i = 0; i < message_descriptor->field_count(); ++i) {
        auto subfield = message_descriptor->field(i);

        if (nullptr == subfield) continue;

        indeep_search_children(subfield);
      }
    }

    end_validate_field(field, message_descriptor);
  }
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_DEEP_FIRST_SEARCH_H_
