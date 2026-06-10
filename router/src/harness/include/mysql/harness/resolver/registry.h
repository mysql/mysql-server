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

#ifndef MYSQL_HARNESS_RESOLVER_REGISTRY_H_
#define MYSQL_HARNESS_RESOLVER_REGISTRY_H_

#include <array>
#include <memory>
#include <shared_mutex>

#include "harness_export.h"
#include "mysql/harness/resolver/interface.h"

namespace mysql_harness {
namespace resolver {

class HARNESS_EXPORT Registry final {
 public:
  static Registry &get_instance();

  void set(CachePolicy policy, std::shared_ptr<ResolverInterface> resolver);
  std::shared_ptr<ResolverInterface> get(CachePolicy policy) const;

  void clear();
  void clear(CachePolicy policy);

 private:
  Registry();
  ~Registry() = default;

  mutable std::shared_mutex mutex_;
  std::array<std::shared_ptr<ResolverInterface>, 3> resolvers_{};
  std::shared_ptr<ResolverInterface> default_resolver_;
  std::shared_ptr<ResolverInterface> default_forwarder_;
};

}  // namespace resolver
}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_RESOLVER_REGISTRY_H_
