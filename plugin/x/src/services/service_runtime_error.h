/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_SRC_SERVICES_SERVICE_RUNTIME_ERROR_H_
#define PLUGIN_X_SRC_SERVICES_SERVICE_RUNTIME_ERROR_H_

#include "plugin/x/src/interface/service_runtime_error.h"

#include "mysql/components/services/mysql_runtime_error.h"
#include "plugin/x/src/interface/service_registry.h"

namespace xpl {

/*
  Error reporting implementation using the mysql_runtime_error service.

  @class Service_runtime_error
*/
class Service_runtime_error : public iface::Service_runtime_error {
 public:
  /*
    Construction of the object by acquiring the service from the registry.

    @param[in] registry Service registry service pointer.
  */
  explicit Service_runtime_error(iface::Service_registry *registry);
  /*
    Destruction of the object that releases the service handle.
  */
  ~Service_runtime_error() override;

  /*
    Emit error code using the error service.

    @param[in] error_id Error code.
    @param[in] flags    Error flags.
    @param[in] args     Variadic argument list.
  */
  void emit(int error_id, int flags, va_list args) override;

  /*
    Check, whether the object has benn correctly created.
  */
  bool is_valid() const override;

 private:
  /*
    Service registry service pointer.
  */
  iface::Service_registry *m_registry;
  /*
    Runtime error service pointer acquired during the object construction.
  */
  SERVICE_TYPE_NO_CONST(mysql_runtime_error) * m_runtime_error;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICE_RUNTIME_ERROR_H_
