/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_NOTICES_H_
#define _XPL_NOTICES_H_

#include <stdint.h>
#include "ngs/error_code.h"

namespace ngs {
class Protocol_encoder;
}

namespace xpl {
class Sql_data_context;

namespace notices {
ngs::Error_code send_warnings(Sql_data_context &da,
                              ngs::Protocol_encoder &proto,
                              bool skip_single_error = false);
ngs::Error_code send_client_id(ngs::Protocol_encoder &proto,
                               uint64_t client_id);
ngs::Error_code send_account_expired(ngs::Protocol_encoder &proto);
ngs::Error_code send_generated_insert_id(ngs::Protocol_encoder &proto,
                                         uint64_t i);
ngs::Error_code send_rows_affected(ngs::Protocol_encoder &proto, uint64_t i);
ngs::Error_code send_message(ngs::Protocol_encoder &proto,
                             const std::string &message);
}  //  namespace notices
}  // namespace xpl

#endif  // _XPL_NOTICES_H_
