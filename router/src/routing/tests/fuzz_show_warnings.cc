/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "router/src/routing/src/show_warnings_parser.h"

#include <iostream>
#include <string_view>

#include "router/src/routing/src/sql_parser_state.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  SqlParserState sql_parser_state;

  sql_parser_state.statement(
      std::string_view(reinterpret_cast<const char *>(Data), Size));

  auto lexer = sql_parser_state.lexer();

  // it shouldn't crash, but otherwise the return-value doesn't matter.
  (void)ShowWarningsParser(lexer.begin(), lexer.end()).parse();

  return 0;
}
