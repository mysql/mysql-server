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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_VALIDATOR_H_

#include <string>

#include "my_dbug.h"

#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/visitor/default_visitor.h"

namespace xcl {

/**
  Class which defines an interfaces for type-check, value-check, and value
  storing.

  Final class should define how the valid should be stored, still the problem
  is that storing of value is rather context related and its hard to generalize
  the it. Because of it it passes "void *context" which final implementer
  should cast to some storage object and copy there value. Example:

  ```
     void store(void *context, const Argument_value &v) override {
       auto storage = (My_storage_class*)context;
       storage->my_object_timeout = v;
     }
  ```
*/
class Validator {
 public:
  using Type = Argument_value::Type;

 public:
  virtual ~Validator() = default;

  virtual Type get_type() const = 0;

  /**
      Checks if 'Argument_value' matches the type define by get_type().

      User may override this method to do more advanced type testing,
    */
  virtual bool valid_type(const Argument_value &value) {
    DBUG_TRACE;
    return get_type() == value.type();
  }

  /**
      Checks if 'Argument_value' matches on expected values.

      The default implementation of this function accepts all values.

      @param value      'variant' value to be checked

      @retval true   value is correct
      @retval false  value is incorrect
    */
  virtual bool valid_value(const Argument_value &value) { return true; }

  virtual void store(void *context, const Argument_value &value) {}
};

/**
  Checks that 'Argument_value' contains boolean.

  This validator check only if the type is valid. Value validation
  and value storing is omitted.
*/
class Bool_validator : public Validator {
 public:
  using Type = Argument_value::Type;

 public:
  Type get_type() const override {
    DBUG_TRACE;
    return Type::k_bool;
  }
};

/**
  Checks that 'Argument_value' contains string.
*/
class String_validator : public Validator {
 public:
  Type get_type() const override {
    DBUG_TRACE;
    return Type::k_string;
  }
};

/**
  Checks that 'Argument_value' contains integer.
*/
class Integer_validator : public Validator {
 public:
  Type get_type() const override {
    DBUG_TRACE;
    return Type::k_integer;
  }
};

/**
  Checks that 'Argument_value' contains Argument_object (map of
  Argument_values).
*/
class Object_validator : public Validator {
 public:
  Type get_type() const override {
    DBUG_TRACE;
    return Type::k_object;
  }
};

/**
  Checks that 'Argument_value' contains an array of strings.
*/
class Array_of_strings_validator : public Validator {
 private:
  class Is_valid_array_visitor : public Default_visitor {
   public:
    void visit_string(const std::string &value) override {
      DBUG_TRACE;
      m_valid = true;
    }

    void visit_array(const Argument_array &values) override {
      DBUG_TRACE;
      m_valid = true;
      for (const auto &value : values) {
        m_valid = m_valid && (value.type() == Argument_value::Type::k_string);
      }
    }

    bool m_valid = false;
  };

 public:
  Type get_type() const override { return Type::k_array; }

  bool valid_type(const Argument_value &value) override {
    DBUG_TRACE;
    Is_valid_array_visitor check;
    value.accept(&check);

    return check.m_valid;
  }
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_VALIDATOR_H_
