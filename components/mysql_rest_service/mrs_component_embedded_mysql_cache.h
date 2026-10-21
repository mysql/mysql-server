/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MRS_COMPONENT_EMBEDDED_MYSQL_CACHE_H
#define MRS_COMPONENT_EMBEDDED_MYSQL_CACHE_H

#include <cstdint>
#include <memory>
#include <string>

#include "mrs/configuration.h"

namespace collector {
class MysqlCacheManager;
class MysqlFixedPoolManager;
}  // namespace collector

namespace mrs_component {

// Creates the component-mode MysqlCacheManager implementation. It preserves the
// shared MRS cache-manager interface but allocates fresh embedded MySQL
// sessions that use internal security-context execution instead of network
// password authentication.
std::unique_ptr<collector::MysqlCacheManager>
create_embedded_mysql_cache_manager(const mrs::Configuration &configuration,
                                    const std::string &local_address,
                                    const std::string &metadata_auth_id,
                                    const std::string &data_auth_id);

std::shared_ptr<collector::MysqlFixedPoolManager>
create_embedded_mysql_user_session_pool(uint32_t passthrough_pool_size,
                                        const std::string &local_address,
                                        const std::string &auth_id);

}  // namespace mrs_component

#endif  // MRS_COMPONENT_EMBEDDED_MYSQL_CACHE_H
