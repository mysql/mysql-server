/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_VALUE_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_VALUE_VALIDATOR_H_

#include <string>

#include "my_dbug.h"

#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/validator/validator.h"
#include "plugin/x/client/visitor/default_visitor.h"

namespace xcl {

/**
  Validator with storage and value casting

  `Validator` uses 'void *context' to pass the information
  where to store the data, this class goes one step further
  it tries to call one of following methods for value-type
  stored in Argument_value:

  * visit_integer
  * visit_bool
  * visit_object
  * visit_array
  * visit_string

  which final class should overwrite and use 'get_ctxt' method
  to store the value. User can overwrite only one of those methods
  in case when he used a type validator for Base_type, for example:

  ```
    struct My_ctxt {
      bool my_is_interactive;
      ...
    };

    class My_client_interactive_validator:
       public Value_validator<My_ctxt, Bool_validator> {
      ...
        void visit_bool(const bool value) override {
          get_ctxt()->my_is_interactive = value;
        }
    }
  ```
*/
template <typename Context, typename Base_type>
class Value_validator : public Base_type, public Default_visitor {
 public:
  Context *get_ctxt() { return m_ctxt; }
  void set_ctxt(void *ctxt) { m_ctxt = reinterpret_cast<Context *>(ctxt); }

 protected:
  void visit_integer(const int64_t /*value*/) override {
    assert(Argument_type::k_integer == Base_type::get_type() &&
           "Derived overwrote wrong type");
  }

  void visit_bool(const bool /*value*/) override {
    assert(Argument_type::k_bool == Base_type::get_type() &&
           "Derived overwrote wrong type");
  }

  void visit_object(const Argument_object & /*value*/) override {
    assert(Argument_type::k_null == Base_type::get_type() &&
           "Derived overwrote wrong type");
  }

  void visit_array(const Argument_array & /*value*/) override {
    assert(Argument_type::k_array == Base_type::get_type() &&
           "Derived overwrote wrong type");
  }

  void visit_string(const std::string & /*value*/) override {
    assert(Argument_type::k_string == Base_type::get_type() &&
           "Derived overwrote wrong type");
  }

 private:
  void store(void *context, const Argument_value &value) override {
    DBUG_TRACE;
    set_ctxt(context);
    value.accept(this);
  }

  Context *m_ctxt;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_VALUE_VALIDATOR_H_
