/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef X_CLIENT_CAPABILITY_BUILDER_H_
#define X_CLIENT_CAPABILITY_BUILDER_H_

#include <memory>
#include <string>

#include "my_compiler.h"
#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/mysqlxclient/xmessage.h"

namespace xcl {

class Capabilities_builder {
 private:
  class Capability_visitor : public Argument_value::Argument_visitor {
   public:
    using Capability = ::Mysqlx::Connection::Capability;
    using Scalar = ::Mysqlx::Datatypes::Scalar;
    using Capability_ptr = std::unique_ptr<Capability>;

   public:
    explicit Capability_visitor(Capability *capability)
        : m_capability(capability) {}

   private:
    void visit(const int64_t value) override {
      auto scalr = get_scalar();
      scalr->set_type(Mysqlx::Datatypes::Scalar_Type_V_SINT);
      scalr->set_v_signed_int(value);
    }

    void visit(const std::string &value, const Argument_value::String_type st
                                             MY_ATTRIBUTE((unused))) override {
      auto scalr = get_scalar();
      scalr->set_type(Mysqlx::Datatypes::Scalar_Type_V_STRING);
      scalr->mutable_v_string()->set_value(value);
    }

    void visit(const bool value) override {
      auto scalr = get_scalar();
      scalr->set_type(Mysqlx::Datatypes::Scalar_Type_V_BOOL);
      scalr->set_v_bool(value);
    }

    // Methods below shouldn't be called
    void visit() override {}

    void visit(const uint64_t value MY_ATTRIBUTE((unused))) override {}

    void visit(const double value MY_ATTRIBUTE((unused))) override {}

    void visit(const float value MY_ATTRIBUTE((unused))) override {}

    void visit(const Object &value MY_ATTRIBUTE((unused))) override {}

    void visit(const Arguments &value MY_ATTRIBUTE((unused))) override {}

    Scalar *get_scalar() {
      auto any = m_capability->mutable_value();
      any->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
      return any->mutable_scalar();
    }

    Capability *m_capability;
  };

 public:
  using CapabilitiesSet = ::Mysqlx::Connection::CapabilitiesSet;

 public:
  Capabilities_builder &add_capability(const std::string &name,
                                       const xcl::Argument_value &argument) {
    auto capabilities = m_cap_set.mutable_capabilities();
    auto capability = capabilities->add_capabilities();
    Capability_visitor capability_scalar_vallue_filler(capability);

    capability->set_name(name);
    argument.accept(&capability_scalar_vallue_filler);

    return *this;
  }

  Capabilities_builder &add_capabilities_from_object(const Object &obj) {
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

#endif  // X_CLIENT_CAPABILITY_BUILDER_H_
