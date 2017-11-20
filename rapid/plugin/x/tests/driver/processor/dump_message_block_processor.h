/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_

#include <map>
#include <string>

#include "processor/send_message_block_processor.h"


class Dump_message_block_processor : public Send_message_block_processor {
 public:
  explicit Dump_message_block_processor(Execution_context *context)
      : Send_message_block_processor(context) {}

 private:
  std::string get_message_name(const char *linebuf) override;
  int process(const xcl::XProtocol::Client_message_type_id msg_id,
              const xcl::XProtocol::Message &message) override;

  std::string m_variable_name;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_DUMP_MESSAGE_BLOCK_PROCESSOR_H_
