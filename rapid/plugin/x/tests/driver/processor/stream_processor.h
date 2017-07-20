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

#ifndef X_TESTS_DRIVER_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_H_

#include <istream>
#include <vector>

#include "formatters/console.h"
#include "processor/block_processor.h"
#include "processor/execution_context.h"
#include "processor/script_stack.h"


std::vector<Block_processor_ptr> create_macro_block_processors(
    Execution_context *context);

std::vector<Block_processor_ptr> create_block_processors(
    Execution_context *context);

int process_client_input(std::istream &input,
                         std::vector<Block_processor_ptr> *eaters,
                         Script_stack *script_stack, const Console &console);

#endif  // X_TESTS_DRIVER_PROCESSOR_H_
