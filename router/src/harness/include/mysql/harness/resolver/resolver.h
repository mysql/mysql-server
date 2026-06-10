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

#ifndef MYSQL_HARNESS_RESOLVER_RESOLVER_H_
#define MYSQL_HARNESS_RESOLVER_RESOLVER_H_

#include <string>

#include "harness_export.h"

#include "mysql/harness/resolver/common.h"

namespace mysql_harness {
namespace resolver {

/**
 * Resolve a hostname to a list of IP addresses.
 *
 * @param hostname the hostname to resolve
 * @param cache_policy a hint on whether to use the cache or not.
 *                     If the cache is not configured, this parameter is
 * ignored. The cache is configured through ResolverRegistry.
 * @return the result of the hostname resolution
 */
HARNESS_EXPORT
ResolveHostResult resolve_host(
    const std::string &hostname,
    CachePolicy cache_policy = CachePolicy::UseIfPresent);

}  // namespace resolver
}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_RESOLVER_RESOLVER_H_
