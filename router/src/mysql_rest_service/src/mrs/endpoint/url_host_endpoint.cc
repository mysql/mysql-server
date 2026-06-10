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

#include "mrs/endpoint/url_host_endpoint.h"

#include <mutex>
#include <string>

#include "mrs/endpoint/url_host_endpoint.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/utility/container/generic.h"

#ifdef HAVE_JIT_EXECUTOR_PLUGIN
#include "mysqlrouter/jit_executor_component.h"
#endif

namespace mrs {
namespace endpoint {

using UrlHost = UrlHostEndpoint::UrlHost;
using UrlHostPtr = UrlHostEndpoint::UrlHostPtr;
using UniversalId = UrlHostEndpoint::UniversalId;
using EnabledType = UrlHostEndpoint::EnabledType;

UrlHostEndpoint::UrlHostEndpoint(const UrlHost &entry,
                                 EndpointConfigurationPtr configuration,
                                 HandlerFactoryPtr factory)
    : OptionEndpoint({}, configuration, factory),
      entry_{std::make_shared<UrlHost>(entry)} {}

void UrlHostEndpoint::update() {
  Parent::update();

#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  jit_executor::JitExecutorComponent::get_instance().update_global_config(
      get_options().value_or(""));
#endif

  observability::EntityCounter<kEntityCounterUpdatesHosts>::increment();
}
UniversalId UrlHostEndpoint::get_id() const { return entry_->id; }

UniversalId UrlHostEndpoint::get_parent_id() const { return {}; }

const UrlHostPtr UrlHostEndpoint::get() const { return entry_; }

void UrlHostEndpoint::set(const UrlHost &entry, EndpointBasePtr) {
  entry_ = std::make_shared<UrlHost>(entry);
  changed();
}

UrlHostEndpoint::Uri UrlHostEndpoint::get_url() const {
  // Let use Uri/constructor. It parses the input text, which
  // may contain host/ip address/port number, thus it may
  // initialize multiple fields of the URL in single call.
  //
  // This approach allows to parse the port which might be
  // returned with host by `get_my_url_part`. Possible values
  // that might be returned by that function:
  //
  // * HOST1
  // * HOST1:PORT
  // * IPv4
  // * IPv4:PORT
  //
  using namespace std::string_literals;
  Uri result("//"s + get_my_url_part());
  return result;
}

EnabledType UrlHostEndpoint::get_this_node_enabled_level() const {
  return EnabledType::EnabledType_public;
}

std::string UrlHostEndpoint::get_my_url_path_part() const { return ""; }

std::string UrlHostEndpoint::get_my_url_part() const { return entry_->name; }

EnabledType UrlHostEndpoint::get_enabled_level() const {
  return EnabledType::EnabledType_public;
}

bool UrlHostEndpoint::does_this_node_require_authentication() const {
  return false;
}

std::optional<std::string> UrlHostEndpoint::get_options() const {
  return entry_->options;
}

}  // namespace endpoint
}  // namespace mrs
