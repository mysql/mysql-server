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

#include "plugin/x/tests/driver/processor/stream_processor.h"

#include <memory>

#include "plugin/x/tests/driver/processor/command_multiline_processor.h"
#include "plugin/x/tests/driver/processor/command_processor.h"
#include "plugin/x/tests/driver/processor/comment_processor.h"
#include "plugin/x/tests/driver/processor/dump_message_block_processor.h"
#include "plugin/x/tests/driver/processor/indigestion_processor.h"
#include "plugin/x/tests/driver/processor/macro_block_processor.h"
#include "plugin/x/tests/driver/processor/sql_block_processor.h"

std::vector<Block_processor_ptr> create_macro_block_processors(
    Execution_context *context) {
  std::vector<Block_processor_ptr> result;

  result.push_back(std::make_shared<Sql_block_processor>(context));
  result.push_back(std::make_shared<Dump_message_block_processor>(context));
  result.push_back(std::make_shared<Command_processor>(context));
  result.push_back(std::make_shared<Command_multiline_processor>(context));
  result.push_back(std::make_shared<Send_message_block_processor>(context));
  result.push_back(std::make_shared<Comment_processor>());
  result.push_back(std::make_shared<Indigestion_processor>(context));

  return result;
}

std::vector<Block_processor_ptr> create_block_processors(
    Execution_context *context) {
  std::vector<Block_processor_ptr> result;

  result.push_back(std::make_shared<Sql_block_processor>(context));
  result.push_back(std::make_shared<Macro_block_processor>(context));
  result.push_back(std::make_shared<Dump_message_block_processor>(context));
  result.push_back(std::make_shared<Command_processor>(context));
  result.push_back(std::make_shared<Command_multiline_processor>(context));
  result.push_back(std::make_shared<Send_message_block_processor>(context));
  result.push_back(std::make_shared<Comment_processor>());
  result.push_back(std::make_shared<Indigestion_processor>(context));

  return result;
}

int process_client_input(std::istream &input,
                         std::vector<Block_processor_ptr> *eaters,
                         Script_stack *script_stack, const Console &console) {
  std::string linebuf;

  if (!input.good()) {
    console.print_error("Input stream isn't valid\n");

    return 1;
  }

  Block_processor_ptr hungry_block_reader;

  while (std::getline(input, linebuf)) {
    Block_processor::Result result = Block_processor::Result::Not_hungry;

    script_stack->front().m_line_number++;

    if (!hungry_block_reader) {
      std::vector<Block_processor_ptr>::iterator i = eaters->begin();

      while (i != eaters->end() &&
             Block_processor::Result::Not_hungry == result) {
        result = (*i)->feed(input, linebuf.c_str());
        if (Block_processor::Result::Indigestion == result) return 1;
        if (Block_processor::Result::Feed_more == result)
          hungry_block_reader = (*i);
        ++i;
      }
      if (Block_processor::Result::Everyone_not_hungry == result) break;
      continue;
    }

    result = hungry_block_reader->feed(input, linebuf.c_str());

    if (Block_processor::Result::Indigestion == result) return 1;

    if (Block_processor::Result::Feed_more != result)
      hungry_block_reader.reset();

    if (Block_processor::Result::Everyone_not_hungry == result) break;
  }

  std::vector<Block_processor_ptr>::iterator i = eaters->begin();

  while (i != eaters->end()) {
    if (!(*i)->feed_ended_is_state_ok()) return 1;

    ++i;
  }

  return 0;
}
