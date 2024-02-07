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

#ifndef ROUTING_CLASSIC_QUERY_PARAM_INCLUDED
#define ROUTING_CLASSIC_QUERY_PARAM_INCLUDED

#include <string>
#include <system_error>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_message.h"

/**
 * convert any param-type into a human readable string.
 */
stdx::expected<std::string, std::error_code> param_to_string(
    const classic_protocol::borrowed::message::client::Query::Param &param);

/**
 * convert a numeric type into an unsigned integer.
 */
stdx::expected<uint64_t, std::error_code> param_to_number(
    const classic_protocol::borrowed::message::client::Query::Param &param);
/**
 * convert a string-typed query param to a std::string.
 *
 * - BLOB
 * - TEXT
 * - STRING
 * - VARCHAR
 * - VARSTRING
 *
 * are string-types.
 *
 * - returns std::errc::bad_message if a non-string type is provided
 * - returns a codec_errc if decoding the parameter fails.
 *
 * @param param parameter of a query attribute
 * @returns a std::string on success, a std::error_code on error.
 */
stdx::expected<std::string, std::error_code> param_as_string(
    const classic_protocol::borrowed::message::client::Query::Param &param);

#endif
