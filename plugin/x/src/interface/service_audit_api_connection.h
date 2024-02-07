/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SERVICE_AUDIT_API_CONNECTION_H_
#define PLUGIN_X_SRC_INTERFACE_SERVICE_AUDIT_API_CONNECTION_H_

#include "mysql/components/services/bits/plugin_audit_connection_types.h"

namespace xpl {
namespace iface {

/*
  Audit API events generating interface.

  @class Service_runtime_error
*/
class Service_audit_api_connection {
 public:
  /*
    Virtual destructor.
  */
  virtual ~Service_audit_api_connection() = default;

  /*
    Generate audit event of the connection class.

    @param[in] thd  THD used for error reporting.
    @param[in] type Connection event subtype.

    @return Value returned by the Audit API handling mechanism.
  */
  virtual int emit(void *thd, mysql_event_connection_subclass_t type) = 0;

  /*
    Generate audit event of the connection class.

    @param[in] thd     THD used for error reporting.
    @param[in] type    Connection event subtype.
    @param[in] errcode Error code that replaces Diagnostic Area result code.

    @return Value returned by the Audit API handling mechanism.
  */
  virtual int emit_with_errorcode(void *thd,
                                  mysql_event_connection_subclass_t type,
                                  int errcode) = 0;

  /*
    Check validity of the object.

    @retval true  Object has been successfully constructed.
    @retval false Object has not been successfully constructed.
  */
  virtual bool is_valid() const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SERVICE_AUDIT_API_CONNECTION_H_
