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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_

#include <string>
#include <vector>

#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/commands/expected_error.h"
#include "plugin/x/tests/driver/processor/execution_context.h"
#include "plugin/x/tests/driver/processor/script_stack.h"

class Sql_block_processor : public Block_processor {
 public:
  explicit Sql_block_processor(Execution_context *context)
      : m_context(context), m_cm(context->m_connection), m_sql(false) {}

  Result feed(std::istream &input, const char *linebuf) override;
  bool feed_ended_is_state_ok() override;

 protected:
  int run_sql_batch(xcl::XSession *conn, const std::string &sql_,
                    const bool be_quiet);
  void handle_warnings(Result_fetcher *fetcher,
                       std::vector<Warning> *out_warnings_aggregation);
  Execution_context *m_context;
  Connection_manager *m_cm;
  std::string m_rawbuffer;
  bool m_sql;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_SQL_BLOCK_PROCESSOR_H_
