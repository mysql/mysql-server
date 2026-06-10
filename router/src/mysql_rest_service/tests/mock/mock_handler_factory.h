/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_HANDLER_FACTORY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_HANDLER_FACTORY_H_

#include "mrs/database/entry/universal_id.h"
#include "mrs/interface/handler_factory.h"

class MockHandlerFactory : public mrs::interface::HandlerFactory {
 public:
  using UniversalId = mrs::database::entry::UniversalId;

  MOCK_METHOD(
      std::shared_ptr<mrs::endpoint::handler::PersistentDataContentFile>,
      create_persistent_content_file,
      (EndpointBasePtr, const OptionalIndexNames &index_names), (override));

  MOCK_METHOD(std::shared_ptr<Handler>, create_db_service_debug_handler,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_service_metadata_handler,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>,
              create_db_schema_metadata_catalog_handler,
              (EndpointBasePtr db_schema_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_schema_metadata_handler,
              (EndpointBasePtr db_schema_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_object_handler,
              (EndpointBasePtr db_object_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_object_metadata_handler,
              (EndpointBasePtr db_object_endpoint), (override));

  MOCK_METHOD(std::shared_ptr<Handler>, create_db_schema_openapi_handler,
              (EndpointBasePtr db_cshema_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_service_openapi_handler,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_db_object_openapi_handler,
              (EndpointBasePtr db_object_endpoint), (override));

  MOCK_METHOD(
      std::shared_ptr<Handler>, create_content_file,
      (EndpointBasePtr content_file_endpoint,
       std::shared_ptr<mrs::endpoint::handler::PersistentDataContentFile>,
       const bool),
      (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_string_handler,
              (EndpointBasePtr endpoint, const UniversalId &service_id,
               bool requires_authentication, const Uri &url,
               const std::string &path, const std::string &file_name,
               const std::string &file_content, bool is_index),
              (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_redirection_handler,
              (EndpointBasePtr endpoint, const UniversalId &service_id,
               bool requires_authentication, const Uri &url,
               const std::string &path, const std::string &file_name,
               const std::string &redirection_path, const bool pernament),
              (override));

  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_login,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_logout,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_completed,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_user,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_auth_apps,
              (EndpointBasePtr db_service_endpoint), (override));
  MOCK_METHOD(std::shared_ptr<Handler>, create_authentication_status,
              (EndpointBasePtr db_service_endpoint), (override));
};

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_HANDLER_FACTORY_H_ */
