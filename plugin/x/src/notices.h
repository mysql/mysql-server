/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_NOTICES_H_
#define PLUGIN_X_SRC_NOTICES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/error_code.h"

namespace ngs {

class Sql_session_interface;
class Protocol_encoder_interface;

}  // namespace ngs

namespace xpl {

namespace notices {
ngs::Error_code send_warnings(ngs::Sql_session_interface &da,
                              ngs::Protocol_encoder_interface &proto,
                              bool skip_single_error = false);
ngs::Error_code send_client_id(ngs::Protocol_encoder_interface &proto,
                               uint64_t client_id);
ngs::Error_code send_account_expired(ngs::Protocol_encoder_interface &proto);
ngs::Error_code send_generated_insert_id(ngs::Protocol_encoder_interface &proto,
                                         uint64_t i);
ngs::Error_code send_rows_affected(ngs::Protocol_encoder_interface &proto,
                                   uint64_t i);
ngs::Error_code send_message(ngs::Protocol_encoder_interface &proto,
                             const std::string &message);
ngs::Error_code send_generated_document_ids(
    ngs::Protocol_encoder_interface &proto,
    const std::vector<std::string> &ids);

}  //  namespace notices
}  // namespace xpl

#endif  // PLUGIN_X_SRC_NOTICES_H_
