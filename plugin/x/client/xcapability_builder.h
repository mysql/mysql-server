/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_XCAPABILITY_BUILDER_H_
#define PLUGIN_X_CLIENT_XCAPABILITY_BUILDER_H_

#include <string>

#include "plugin/x/client/visitor/any_filler.h"

namespace xcl {

class Capabilities_builder {
 public:
  using CapabilitiesSet = ::Mysqlx::Connection::CapabilitiesSet;

 public:
  Capabilities_builder &clear() {
    m_cap_set.Clear();
    return *this;
  }

  Capabilities_builder &add_capability(const std::string &name,
                                       const xcl::Argument_value &argument) {
    auto capabilities = m_cap_set.mutable_capabilities();
    auto capability = capabilities->add_capabilities();
    capability->set_name(name);
    Any_filler capability_filler(capability->mutable_value());
    argument.accept(&capability_filler);

    return *this;
  }

  Capabilities_builder &add_capabilities_from_object(
      const Argument_object &obj) {
    for (const auto &cap : obj) {
      add_capability(cap.first, cap.second);
    }

    return *this;
  }

  CapabilitiesSet &get_result() { return m_cap_set; }

 private:
  CapabilitiesSet m_cap_set;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XCAPABILITY_BUILDER_H_
