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

#include "mrs/endpoint/content_file_endpoint.h"

#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/json/parse_file_sharing_options.h"
#include "mrs/observability/entity.h"
#include "mrs/router_observation_entities.h"

namespace mrs {
namespace endpoint {

using ContentFile = ContentFileEndpoint::ContentFile;
using ContentFilePtr = ContentFileEndpoint::ContentFilePtr;
using UniversalId = ContentFileEndpoint::UniversalId;
using EnabledType = ContentFileEndpoint::EnabledType;

static json::ParseFileSharingOptions::Result parse_content_file_options(
    const std::string &options) {
  auto result =
      helper::json::text_to_handler<json::ParseFileSharingOptions>(options);

  if (!result) {
    log_error(
        "Failed to parse 'ContentFileEndpoint' options from db_objects JSON "
        "configuration");
  }

  return result.value_or(json::ParseFileSharingOptions::Result{});
}

ContentFileEndpoint::ContentFileEndpoint(const ContentFile &entry,
                                         EndpointConfigurationPtr configuration,
                                         HandlerFactoryPtr factory)
    : Parent(configuration),
      entry_{std::make_shared<ContentFile>(entry)},
      factory_{factory} {}

ContentFileEndpoint::~ContentFileEndpoint() {
  // Content File is getting deleted, if it affected the Content Set handlers
  // lets update them so that defaultStaticContent handler is restored
  if (altered_parent_) {
    auto parent = mrs::endpoint::handler::lock_parent(this);
    if (parent) {
      parent->update();
    }
  }
}

UniversalId ContentFileEndpoint::get_id() const { return entry_->id; }

UniversalId ContentFileEndpoint::get_parent_id() const {
  return entry_->content_set_id;
}

const ContentFilePtr ContentFileEndpoint::get() const { return entry_; }

void ContentFileEndpoint::set(const ContentFile &entry,
                              EndpointBasePtr parent) {
  auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);
  entry_ = std::make_shared<ContentFile>(entry);
  change_parent(parent);
  changed();
}

void ContentFileEndpoint::update() {
  Parent::update();
  observability::EntityCounter<kEntityCounterUpdatesFiles>::increment();

  auto parent = mrs::endpoint::handler::lock_parent(this);
  assert(parent && "parent must be valid");
  parent->child_updated(shared_from_this(), persistent_data_);
}

void ContentFileEndpoint::activate_common() {
  persistent_data_.reset();
  persistent_data_ = factory_->create_persistent_content_file(
      shared_from_this(), get_index_files());
}

void ContentFileEndpoint::activate_private() { activate_common(); }

void ContentFileEndpoint::activate_public() {
  activate_common();

  const bool k_redirect_pernament = true;
  is_index_ = false;
  auto parent = mrs::endpoint::handler::lock_parent(this);
  assert(parent && "parent must be valid");
  const auto &possible_indexes = parent->get_index_files();

  if (possible_indexes.has_value()) {
    auto entry_name =
        handler::remove_leading_slash_from_path(entry_->request_path);
    for (const auto &index : possible_indexes.value()) {
      if (entry_name == index) {
        is_index_ = true;
        break;
      }
    }
  }

  // .reset() has to be done as a separate step to avoid overwriting the
  // handlers in the map. As a result for a small window there is no handler
  // (can potentially yield 404).
  // handle_index is set to false as setting directoryIndex handler is relegated
  // to the parent
  handler_.reset();
  handler_ = factory_->create_content_file(shared_from_this(), persistent_data_,
                                           /*handle_index*/ false);

  if (is_index_) {
    // see the comment for a handler_ a few lines above
    handler_redirection_.reset();
    handler_redirection_ = factory_->create_redirection_handler(
        shared_from_this(), parent->get()->service_id,
        parent->required_authentication(), parent->get_url(),
        parent->get_url_path(), "", parent->get_url_path() + "/",
        k_redirect_pernament);
  }

  // ContentSet options
  const auto &options = parent.get()->get_options();

  if (options.has_value()) {
    auto fs = parse_content_file_options(options.value());
    for (const auto &[k, _] : fs.default_static_content_) {
      if (k ==
          handler::remove_leading_slash_from_path(get_my_url_path_part())) {
        // ContentSet has a handler for defaultStaticContent option that
        // collides with this ContentFile, we should disable it
        parent->disable_handler(get_url_path());
        altered_parent_ = true;
      }
    }
  }
}

void ContentFileEndpoint::deactivate() {
  handler_.reset();
  handler_redirection_.reset();
  is_index_ = false;
}

EnabledType ContentFileEndpoint::get_this_node_enabled_level() const {
  return entry_->enabled;
}

bool ContentFileEndpoint::does_this_node_require_authentication() const {
  return entry_->requires_authentication;
}

std::string ContentFileEndpoint::get_my_url_path_part() const {
  return entry_->request_path;
}

std::string ContentFileEndpoint::get_my_url_part() const {
  return entry_->request_path;
}

std::optional<std::string> ContentFileEndpoint::get_options() const {
  return {};
}

bool ContentFileEndpoint::is_index() const { return is_index_; }

}  // namespace endpoint
}  // namespace mrs
