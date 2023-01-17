/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <memory>
#include <mutex>
#include <string>

#include "mysqlrouter/rest_api_component.h"
#include "rest_api_plugin.h"

BaseRestApiHandler::~BaseRestApiHandler() = default;

//
// HTTP Server's public API
//
bool RestApiComponent::try_process_spec(SpecProcessor processor) {
  std::lock_guard<std::mutex> lock(spec_mu_);

  // if srv_ already points to the rest_api forward the
  // processor directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->process_spec(processor);

    return true;
  } else {
    spec_processors_.emplace_back(processor);
    return false;
  }
}

void RestApiComponent::remove_process_spec(SpecProcessor processor) {
  std::lock_guard<std::mutex> lock(spec_mu_);

  spec_processors_.erase(
      std::remove_if(
          spec_processors_.begin(), spec_processors_.end(),
          [&processor](const auto &value) { return value == processor; }),
      spec_processors_.end());
}

void RestApiComponent::add_path(const std::string &path,
                                std::unique_ptr<BaseRestApiHandler> handler) {
  std::lock_guard<std::mutex> lock(spec_mu_);

  // if srv_ already points to the rest_api forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->add_path(path, std::move(handler));
  } else {
    add_path_backlog_.emplace_back(path, std::move(handler));
  }
}

void RestApiComponent::remove_path(const std::string &path) {
  std::lock_guard<std::mutex> lock(spec_mu_);

  // if srv_ already points to the rest_api remove the
  // route directly, otherwise remove it from the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->remove_path(path);
  } else {
    add_path_backlog_.erase(
        std::remove_if(
            add_path_backlog_.begin(), add_path_backlog_.end(),
            [&path](const auto &value) { return value.first == path; }),
        add_path_backlog_.end());
  }
}

void RestApiComponent::init(std::shared_ptr<RestApi> srv) {
  std::lock_guard<std::mutex> lock(spec_mu_);

  srv_ = srv;

  for (auto &processor : spec_processors_) {
    srv->process_spec(processor);
  }
  spec_processors_.clear();

  for (auto &el : add_path_backlog_) {
    srv->add_path(el.first, std::move(el.second));
  }

  add_path_backlog_.clear();
}

RestApiComponent &RestApiComponent::get_instance() {
  static RestApiComponent instance;

  return instance;
}
