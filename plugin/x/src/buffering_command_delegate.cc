/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

#include <functional>

#include "plugin/x/src/buffering_command_delegate.h"

namespace xpl {

Buffering_command_delegate::Buffering_command_delegate()
    : Callback_command_delegate(
          std::bind(&Buffering_command_delegate::begin_row_cb, this),
          std::bind(&Buffering_command_delegate::end_row_cb, this,
                    std::placeholders::_1)) {}

void Buffering_command_delegate::reset() {
  m_resultset.clear();
  Command_delegate::reset();
}

Callback_command_delegate::Row_data *
Buffering_command_delegate::begin_row_cb() {
  m_resultset.push_back(Row_data());
  return &m_resultset.back();
}

bool Buffering_command_delegate::end_row_cb(Row_data *) { return true; }

}  // namespace xpl
