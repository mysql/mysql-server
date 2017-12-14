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

#ifndef X_TESTS_DRIVER_PROCESSOR_MACRO_BLOCK_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_MACRO_BLOCK_PROCESSOR_H_

#include <memory>
#include <string>

#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/commands/macro.h"
#include "plugin/x/tests/driver/processor/execution_context.h"


class Macro_block_processor : public Block_processor {
 public:
  explicit Macro_block_processor(Execution_context *context)
      : m_context(context) {}

  Result feed(std::istream &input, const char *linebuf) override;
  bool feed_ended_is_state_ok() override;

 private:
  Execution_context     *m_context;
  std::shared_ptr<Macro> m_macro;
  std::string            m_rawbuffer;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_MACRO_BLOCK_PROCESSOR_H_
