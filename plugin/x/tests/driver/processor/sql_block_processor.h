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

#ifndef X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_

#include <list>
#include <string>

#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/commands/expected_error.h"
#include "plugin/x/tests/driver/processor/execution_context.h"
#include "plugin/x/tests/driver/processor/script_stack.h"


class Sql_block_processor : public Block_processor {
 public:
  explicit Sql_block_processor(Execution_context *context)
      : m_context(context), m_cm(context->m_connection), m_sql(false) {}

  virtual Result feed(std::istream &input, const char *linebuf);
  virtual bool feed_ended_is_state_ok();

 private:
  int run_sql_batch(xcl::XSession *conn, const std::string &sql_,
                    const bool be_quiet);

  Execution_context  *m_context;
  Connection_manager *m_cm;
  std::string         m_rawbuffer;
  bool                m_sql;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_
