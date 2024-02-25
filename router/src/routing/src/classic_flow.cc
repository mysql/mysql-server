/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_flow.h"

#include "classic_command.h"
#include "classic_connection_base.h"
#include "classic_greeting_receiver.h"

stdx::expected<Processor::Result, std::error_code> FlowProcessor::process() {
  switch (stage()) {
    case Stage::Greeting:
      return greeting();
    case Stage::Command:
      return command();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

/**
 * the handshake part.
 */
stdx::expected<Processor::Result, std::error_code> FlowProcessor::greeting() {
  stage(Stage::Command);

  connection()->push_processor(std::make_unique<ClientGreetor>(connection()));
  return Result::Again;
}

/**
 * the command part.
 */
stdx::expected<Processor::Result, std::error_code> FlowProcessor::command() {
  // if the greeting phase finished with auth::success, start the command
  // phase. Otherwise just leave.
  if (!connection()->authenticated()) {
    stage(Stage::Done);
    return Result::Again;
  }

  connection()->connected();

  stage(Stage::Done);

  connection()->push_processor(
      std::make_unique<CommandProcessor>(connection()));

  return Result::Again;
}
