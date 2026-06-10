/*
 * Copyright (c) 2019, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MYSQLSHDK_LIBS_UTILS_UTILS_ENCODING_H_
#define MYSQLSHDK_LIBS_UTILS_UTILS_ENCODING_H_

#include <string>
#include <string_view>

namespace shcore {

/**
 * Decodes a base64 encoded string.
 *
 * @param source the base64 string to be decoded
 * @param target a string pointer where the decoded string will be stored.
 *
 * @returns true on success decode
 */
bool decode_base64(std::string_view source, std::string *target);

bool encode_base64(std::string_view source, std::string *encoded);

bool encode_base64(const unsigned char *source, std::size_t source_length,
                   std::string *encoded);

bool decode_base64url(std::string_view source, std::string *target);

bool encode_base64url(std::string_view source, std::string *encoded);

/**
 * Replaces '+' with '-' and '/' with '_', trims any '=' (RFC 7515, Section 2
 * and RFC 4648, Section 5).
 */
bool encode_base64url(const unsigned char *source, std::size_t source_length,
                      std::string *encoded);

}  // namespace shcore

#endif  // MYSQLSHDK_LIBS_UTILS_UTILS_ENCODING_H_
