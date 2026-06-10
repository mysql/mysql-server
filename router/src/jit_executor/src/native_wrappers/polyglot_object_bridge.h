/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_OBJECT_BRIDGE_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_OBJECT_BRIDGE_H_

#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_value.h"
#include "utils/utils_json.h"

namespace shcore {
namespace polyglot {

class Object_bridge {
 public:
  virtual ~Object_bridge() = default;

  virtual std::string class_name() const = 0;

  virtual std::string &append_descr(std::string &s_out, int indent = -1,
                                    int quote_strings = 0) const;

  virtual std::string &append_repr(std::string &s_out) const;

  virtual void append_json(shcore::JSON_dumper &dumper) const;

  //! Returns the list of members that this object has
  virtual std::vector<std::string> get_members() const;

  //! Verifies if the object has a member
  virtual bool has_member(const std::string &prop) const;

  //! Sets the value of a member
  virtual void set_member(const std::string & /*prop*/, Value /*value*/) {}

  //! Returns the value of a member
  virtual bool is_indexed() const { return false; }

  //! Returns the value of a member
  virtual Value get_member(size_t /*index*/) const { return {}; }

  //! Sets the value of a member
  virtual void set_member(size_t /*index*/, Value /*value*/) {}

  //! Returns the number of indexable members
  virtual size_t length() const { return 0; }

  //! Returns true if a method with the given name exists.
  bool has_method(const std::string &name) const;

  //! Returns the value of a member
  virtual Value get_member(const std::string & /*prop*/) const { return {}; }

  //! Calls the named method with the given args
  virtual Value call(const std::string & /*name*/,
                     const Argument_list & /*args*/) {
    return {};
  }

 private:
  virtual const std::vector<std::string> *properties() const { return nullptr; }
  virtual const std::vector<std::string> *methods() const { return nullptr; }
};

typedef std::shared_ptr<Object_bridge> Object_bridge_t;

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_OBJECT_BRIDGE_H_
