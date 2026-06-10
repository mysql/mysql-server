/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_RESOLVER_ERROR_CODE_H_
#define MYSQL_HARNESS_RESOLVER_ERROR_CODE_H_

#include <system_error>

#include "harness_export.h"

namespace mysql_harness {
namespace resolver {

/**
 * Enumerates the possible outcomes of a hostname resolution.
 *
 * Transient resolver errors (e.g., timeout, SERVFAIL) are returned as
 * std::error_code and not as ResolveResult::k_temporary_failure.
 */
enum class ErrcResolveResult {
  Success,  ///< Successful resolution with at least one IP address.
  NotFound  ///< Deterministic negative outcome (NXDOMAIN, NODATA).
};

HARNESS_EXPORT
const std::error_category &category() noexcept;

HARNESS_EXPORT
std::error_code make_error_code(ErrcResolveResult) noexcept;

}  // namespace resolver
}  // namespace mysql_harness

namespace std {
template <>
struct is_error_code_enum<mysql_harness::resolver::ErrcResolveResult>
    : true_type {};
}  // namespace std

#endif  // MYSQL_HARNESS_RESOLVER_ERROR_CODE_H_
