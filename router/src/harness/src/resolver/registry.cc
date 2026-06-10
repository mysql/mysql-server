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
#include "mysql/harness/resolver/registry.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include "resolver/resolver_net_ts.h"

namespace mysql_harness {
namespace resolver {

class ResolverForwarder : public ResolverInterface {
 public:
  ResolveHostResult resolve_host(
      const std::string &hostname,
      [[maybe_unused]] CachePolicy cache_policy) override {
    return Registry::get_instance()
        .get(CachePolicy::Bypass)
        ->resolve_host(hostname, CachePolicy::Bypass);
  }
};

Registry::Registry()
    : default_resolver_(std::make_shared<ResolverNetTs>()),
      default_forwarder_(std::make_shared<ResolverForwarder>()) {
  clear();
}

Registry &Registry::get_instance() {
  static Registry s_instance;
  return s_instance;
}

void Registry::set(CachePolicy policy,
                   std::shared_ptr<ResolverInterface> resolver) {
  assert(static_cast<size_t>(policy) < resolvers_.size());
  assert(nullptr != resolver.get());
  std::unique_lock lock(mutex_);
  resolvers_[static_cast<int>(policy)] = std::move(resolver);
}

std::shared_ptr<ResolverInterface> Registry::get(CachePolicy policy) const {
  std::shared_lock lock(mutex_);
  return resolvers_[static_cast<int>(policy)];
}

void Registry::clear() {
  std::unique_lock lock(mutex_);

  resolvers_[static_cast<int>(CachePolicy::FillOnSuccess)] = default_forwarder_;
  resolvers_[static_cast<int>(CachePolicy::UseIfPresent)] = default_forwarder_;
  resolvers_[static_cast<int>(CachePolicy::Bypass)] = default_resolver_;
}

void Registry::clear(CachePolicy policy) {
  assert(static_cast<size_t>(policy) < resolvers_.size());
  std::unique_lock lock(mutex_);

  resolvers_[static_cast<int>(policy)] =
      (policy == CachePolicy::Bypass) ? default_resolver_ : default_forwarder_;
}

}  // namespace resolver
}  // namespace mysql_harness
