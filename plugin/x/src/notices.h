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

#ifndef PLUGIN_X_SRC_NOTICES_H_
#define PLUGIN_X_SRC_NOTICES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"

namespace xpl {

namespace notices {
std::string serialize_warning(const iface::Warning_level level,
                              const uint32_t code, const std::string &msg);

ngs::Error_code send_warnings(iface::Sql_session *da,
                              iface::Protocol_encoder *proto,
                              bool skip_single_error = false);
}  //  namespace notices
}  // namespace xpl

#endif  // PLUGIN_X_SRC_NOTICES_H_
