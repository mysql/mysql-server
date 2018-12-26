/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_TESTS_DRIVER_FORMATTERS_MESSAGE_FORMATTER_H_
#define PLUGIN_X_TESTS_DRIVER_FORMATTERS_MESSAGE_FORMATTER_H_

#include <cctype>
#include <string>
#include <vector>

#include "plugin/x/client/mysqlxclient/xprotocol.h"

namespace formatter {

std::string message_to_text(const xcl::XProtocol::Message &message);

/*
   - field_path possible values:

  * msg1_field1
  * msg1_field1.field1.field2
  * field1[1].field1[0]
  * field1[1].field2
*/
std::string message_to_text(const xcl::XProtocol::Message &message,
                            const std::string &field_path,
                            const bool show_message_name = true);

}  // namespace formatter

#endif  // PLUGIN_X_TESTS_DRIVER_FORMATTERS_MESSAGE_FORMATTER_H_
