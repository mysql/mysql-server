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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_MULTIPLE_COMPRESS_BLOCK_PROCESSOR_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_MULTIPLE_COMPRESS_BLOCK_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/commands/expected_error.h"
#include "plugin/x/tests/driver/processor/execution_context.h"
#include "plugin/x/tests/driver/processor/script_stack.h"
#include "plugin/x/tests/driver/processor/send_message_block_processor.h"

class Multiple_compress_block_processor : public Block_processor {
 public:
  explicit Multiple_compress_block_processor(Execution_context *context)
      : m_context(context), m_cm(context->m_connection) {}

  Result feed(std::istream &input, const char *linebuf) override;
  bool feed_ended_is_state_ok() override;

 protected:
  using Client_message_id = xcl::XProtocol::Client_message_type_id;
  using Message_ptr = std::unique_ptr<xcl::XProtocol::Message>;

  class Compress_message_block_processor : public Send_message_block_processor {
   public:
    explicit Compress_message_block_processor(
        Execution_context *context, Multiple_compress_block_processor *parent)
        : Send_message_block_processor(context), m_parent(parent) {}

    int process(const xcl::XProtocol::Client_message_type_id msg_id,
                const xcl::XProtocol::Message &message) override {
      return m_parent->process(msg_id, message);
    }

    Multiple_compress_block_processor *m_parent;
  };

  int process(const xcl::XProtocol::Client_message_type_id msg_id,
              const xcl::XProtocol::Message &message);

  Execution_context *m_context;
  Connection_manager *m_cm;
  bool m_processing = false;
  Compress_message_block_processor m_message_processor{m_context, this};
  std::vector<Client_message_id> m_msg_id;
  std::vector<Message_ptr> m_messages;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_MULTIPLE_COMPRESS_BLOCK_PROCESSOR_H_
