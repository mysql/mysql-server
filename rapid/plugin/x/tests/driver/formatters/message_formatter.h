/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _MESSAGE_PRINTER_H_
#define _MESSAGE_PRINTER_H_

#include <cctype>
#include <string>
#include <vector>

#include "plugin/x/client/mysqlxclient/xprotocol.h"


namespace formatter {

std::string message_to_text(
    const xcl::XProtocol::Message &message);

/*
   - field_path possible values:

  * msg1_field1
  * msg1_field1.field1.field2
  * field1[1].field1[0]
  * field1[1].field2
*/
std::string message_to_text(
    const xcl::XProtocol::Message &message,
    const std::string &field_path);

}  // namespace formatter

#endif  // _MESSAGE_PRINTER_H_
