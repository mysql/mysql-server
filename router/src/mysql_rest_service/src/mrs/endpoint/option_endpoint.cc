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

#include "mrs/endpoint/option_endpoint.h"

#include "mysql/harness/utility/container/generic.h"

#include "helper/json/text_to.h"
#include "mrs/json/parse_file_sharing_options.h"

namespace mrs {
namespace endpoint {

using OptionalIndexNames = OptionEndpoint::OptionalIndexNames;

static json::ParseFileSharingOptions::Result parse_file_sharing_options(
    const std::string &options) {
  auto result =
      helper::json::text_to_handler<mrs::json::ParseFileSharingOptions>(
          options);

  if (!result) {
    log_error(
        "Failed to parse 'OptionEndpoint' options from db_objects JSON "
        "configuration");
  }

  return result.value_or(mrs::json::ParseFileSharingOptions::Result{});
}

OptionEndpoint::OptionEndpoint(UniversalId service_id,
                               EndpointConfigurationPtr configuration,
                               HandlerFactoryPtr factory)
    : EndpointBase(configuration), service_id_{service_id}, factory_{factory} {}

OptionalIndexNames OptionEndpoint::get_index_files() {
  if (directory_indexes_.has_value()) return directory_indexes_;

  auto parent = get_parent_ptr();
  if (parent) {
    EndpointBase *eb = parent.get();
    return eb->get_index_files();
  }

  return {};
}

void OptionEndpoint::update() {
  const bool k_redirect_pernament = true;
  const bool k_redirect_temporary = false;
  EndpointBase::update();

  handlers_.clear();

  const auto &opt = get_options();

  if (EnabledType::EnabledType_public == get_enabled_level() &&
      opt.has_value()) {
    using namespace helper::json;
    using namespace mrs::json;

    auto fs = parse_file_sharing_options(opt.value());

    directory_indexes_ = fs.directory_index_directive_;

    auto directory_indexes =
        get_index_files().value_or(OptionalIndexNames::value_type{});

    for (const auto &[k, v] : fs.default_redirects_) {
      handlers_.push_back(factory_->create_redirection_handler(
          shared_from_this(), service_id_, required_authentication(), get_url(),
          get_url_path(), k, v, k_redirect_temporary));
    }

    for (const auto &[k, v] : fs.default_static_content_) {
      const bool is_index =
          mysql_harness::utility::container::has(directory_indexes, k);
      // When the url path is empty, its root path, which
      // http plugin processes as "", instead "/".
      // In case of root path and index, we do not need
      // to redirect because the path is proper but optimized
      // by http server.
      if (!get_url_path().empty() && is_index) {
        handlers_.push_back(factory_->create_redirection_handler(
            shared_from_this(), service_id_, required_authentication(),
            get_url(), get_url_path(), "", get_url_path() + "/",
            k_redirect_pernament));
      }

      handlers_.push_back(factory_->create_string_handler(
          shared_from_this(), service_id_, required_authentication(), get_url(),
          get_url_path(), k, v, is_index));
    }
  }
}

}  // namespace endpoint
}  // namespace mrs
