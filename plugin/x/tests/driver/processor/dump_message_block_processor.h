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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_

#include <map>
#include <string>

#include "plugin/x/tests/driver/processor/send_message_block_processor.h"

class Dump_message_block_processor : public Send_message_block_processor {
 public:
  explicit Dump_message_block_processor(Execution_context *context)
      : Send_message_block_processor(context) {}

  Result feed(std::istream &input, const char *linebuf) override;

 private:
  int process(const xcl::XProtocol::Client_message_type_id msg_id,
              const xcl::XProtocol::Message &message) override;

  std::string m_variable_name;
  bool m_is_hex = false;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_
