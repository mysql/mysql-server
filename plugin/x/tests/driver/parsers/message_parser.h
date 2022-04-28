/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_MESSAGE_PARSER_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_MESSAGE_PARSER_H_

#include <memory>
#include <string>

#include "plugin/x/client/mysqlxclient/xprotocol.h"

namespace parser {

bool get_name_and_body_from_text(const std::string &text_message,
                                 std::string *out_full_message_name,
                                 std::string *out_message_body,
                                 const bool is_body_full = false);

xcl::XProtocol::Message *get_notice_message_from_text(
    const Mysqlx::Notice::Frame_Type type, const std::string &text_payload,
    std::string *out_error, const bool allow_partial_messaged = false);

xcl::XProtocol::Message *get_client_message_from_text(
    const std::string &name, const std::string &data,
    xcl::XProtocol::Client_message_type_id *msg_id, std::string *out_error,
    const bool allow_partial_messaged = false);

xcl::XProtocol::Message *get_server_message_from_text(
    const std::string &name, const std::string &data,
    xcl::XProtocol::Server_message_type_id *msg_id, std::string *out_error,
    const bool allow_partial_messaged = false);

}  // namespace parser

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_MESSAGE_PARSER_H_
