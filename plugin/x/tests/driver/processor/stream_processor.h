/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_STREAM_PROCESSOR_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_STREAM_PROCESSOR_H_

#include <istream>
#include <vector>

#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/execution_context.h"
#include "plugin/x/tests/driver/processor/script_stack.h"

std::vector<Block_processor_ptr> create_macro_block_processors(
    Execution_context *context);

std::vector<Block_processor_ptr> create_block_processors(
    Execution_context *context);

int process_client_input(std::istream &input,
                         std::vector<Block_processor_ptr> *eaters,
                         Script_stack *script_stack, const Console &console);

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_STREAM_PROCESSOR_H_
