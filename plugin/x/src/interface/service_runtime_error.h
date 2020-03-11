/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SERVICE_RUNTIME_ERROR_H_
#define PLUGIN_X_SRC_INTERFACE_SERVICE_RUNTIME_ERROR_H_

//! @cond Doxygen_Suppress
#include <stdarg.h>
//! @endcond

namespace xpl {
namespace iface {

/*
  Error reporting interface.

  @class Service_runtime_error
*/
class Service_runtime_error {
 public:
  /*
    Virtual destructor.
  */
  virtual ~Service_runtime_error() = default;

  /*
    Emit error using custom implementation.

    @param[in] error_id Error code.
    @param[in] flags    Error flags.
    @param[in] args     Variadic argument list.
  */
  virtual void emit(int error_id, int flags, va_list args) = 0;

  /*
    Check validity of the object.

    @retval true  Object has been successfully constructed.
    @retval false Object has not been successfully constructed.
  */
  virtual bool is_valid() const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SERVICE_RUNTIME_ERROR_H_
