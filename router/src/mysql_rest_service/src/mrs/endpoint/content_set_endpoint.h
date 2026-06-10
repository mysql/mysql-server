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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_CONTENT_SET_ENDPOINT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_CONTENT_SET_ENDPOINT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mrs/database/entry/content_set.h"
#include "mrs/endpoint/handler/persistent/persistent_data_content_file.h"
#include "mrs/endpoint/option_endpoint.h"
#include "mrs/interface/handler_factory.h"

namespace mrs {
namespace endpoint {

class DbServiceEndpoint;

class ContentSetEndpoint : public OptionEndpoint {
 public:
  using Parent = OptionEndpoint;
  using ContentSet = mrs::database::entry::ContentSet;
  using ContentSetPtr = std::shared_ptr<ContentSet>;
  using HandlerFactoryPtr = std::shared_ptr<mrs::interface::HandlerFactory>;
  using DataType = ContentSet;

 public:
  ContentSetEndpoint(const ContentSet &entry,
                     EndpointConfigurationPtr configuration,
                     HandlerFactoryPtr factory);

  UniversalId get_id() const override;
  UniversalId get_parent_id() const override;
  std::optional<std::string> get_options() const override;

  const ContentSetPtr get() const;
  void set(const ContentSet &schema, EndpointBasePtr parent);

  void get_content_set_data(std::vector<std::string> *out_scripts,
                            std::vector<std::string> *out_module_classes);

  void disable_handler(std::string_view handler_path);
  void child_updated(
      std::shared_ptr<mrs::interface::EndpointBase> content_file_endpoint,
      std::shared_ptr<handler::PersistentDataContentFile> file_contents);

  void update() override;

 private:
  EnabledType get_this_node_enabled_level() const override;
  bool does_this_node_require_authentication() const override;
  std::string get_my_url_path_part() const override;
  std::string get_my_url_part() const override;

  std::optional<uint16_t> directory_pos_;
  ContentSetPtr entry_;
};

}  // namespace endpoint

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_CONTENT_SET_ENDPOINT_H_
