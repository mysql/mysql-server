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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MRS_COMPONENT_RUNTIME_SERVICES_H
#define MRS_COMPONENT_RUNTIME_SERVICES_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "mrs/embedded_runtime_services.h"

namespace mrs_component {

/**
  Adapts embedded MRS runtime operations to Server component services.
*/
class ComponentEmbeddedRuntimeServices : public mrs::EmbeddedRuntimeServices {
 public:
  ComponentEmbeddedRuntimeServices(std::string local_address,
                                   std::string metadata_user,
                                   std::string data_user,
                                   std::string jwt_secret);
  ~ComponentEmbeddedRuntimeServices() override;

  std::unique_ptr<collector::MysqlCacheManager> create_mysql_cache_manager(
      const mrs::Configuration &configuration) override;

  std::optional<std::string> authenticate_mysql_user(
      const std::string &user,
      const mysql_harness::SecureString &password) override;

  std::shared_ptr<collector::MysqlFixedPoolManager>
  create_mysql_user_session_pool(uint32_t passthrough_pool_size,
                                 const std::string &auth_id) override;

  const std::string &jwt_secret() const override { return jwt_secret_; }

 private:
  std::string local_address_;
  std::string metadata_auth_id_;
  std::string data_auth_id_;
  std::string jwt_secret_;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_RUNTIME_SERVICES_H
