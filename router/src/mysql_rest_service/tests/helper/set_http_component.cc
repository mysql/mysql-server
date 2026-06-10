/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "helper/set_http_component.h"

#include <memory>
#include <utility>

#include "http/base/uri_path_matcher.h"

namespace helper {

SetHttpComponent::HttpWrapperHttpServerComponent::
    HttpWrapperHttpServerComponent(HttpServerComponent *other)
    : other_{other} {}

void *SetHttpComponent::HttpWrapperHttpServerComponent::add_regex_route(
    const std::string &url_host, const std::string &url_regex,
    std::unique_ptr<http::base::RequestHandler> cb) {
  return other_->add_regex_route(url_host, url_regex, std::move(cb));
}

void *SetHttpComponent::HttpWrapperHttpServerComponent::add_direct_match_route(
    const std::string &url_host, const ::http::base::UriPathMatcher &url_path,
    std::unique_ptr<http::base::RequestHandler> cb) {
  return other_->add_direct_match_route(url_host, url_path, std::move(cb));
}

void SetHttpComponent::HttpWrapperHttpServerComponent::remove_route(
    const void *handler) {
  other_->remove_route(handler);
}

void SetHttpComponent::HttpWrapperHttpServerComponent::init(
    HttpServerCtxtPtr srv) {
  other_->init(srv);
}

bool SetHttpComponent::HttpWrapperHttpServerComponent::is_ssl_configured() {
  return other_->is_ssl_configured();
}

SetHttpComponent::SetHttpComponent(HttpServerComponent *component) {
  auto wrapper = std::make_unique<HttpWrapperHttpServerComponent>(component);
  HttpServerComponent::set_instance(std::move(wrapper));
}

SetHttpComponent::~SetHttpComponent() { HttpServerComponent::set_instance({}); }

}  // namespace helper
