/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_SERVICES_SERVICE_AUDIT_API_CONNECTION_H_
#define PLUGIN_X_SRC_SERVICES_SERVICE_AUDIT_API_CONNECTION_H_

#include "mysql/components/services/audit_api_connection_service.h"
#include "plugin/x/src/interface/service_audit_api_connection.h"
#include "plugin/x/src/interface/service_registry.h"

namespace xpl {

/*
  Audit API event generation using the mysql_audit_api_connection service.

  @class Service_audit_api_connection
*/
class Service_audit_api_connection
    : public iface::Service_audit_api_connection {
 public:
  /*
    Construction of the object by acquiring the service from the registry.

    @param[in] registry Service registry service pointer.
  */
  explicit Service_audit_api_connection(iface::Service_registry *registry);
  /*
    Destruction of the object that releases the service handle.
  */
  ~Service_audit_api_connection() override;
  /*
    Generate audit event of the connection class using the service.

    @param[in] thd     THD used for error reporting.
    @param[in] type    Connection event subtype.

    @return Value returned by the Audit API handling mechanism.
  */
  int emit(void *thd, mysql_event_connection_subclass_t type) override;
  /*
    Generate audit event of the connection class using the service.

    @param[in] thd     THD used for error reporting.
    @param[in] type    Connection event subtype.
    @param[in] errcode Error code.

    @return Value returned by the Audit API handling mechanism.
  */
  int emit_with_errorcode(void *thd, mysql_event_connection_subclass_t type,
                          int errcode) override;
  /*
    Check validity of the object.

    @retval true  Object has been successfully constructed.
    @retval false Object has not been successfully constructed.
  */
  bool is_valid() const override;

 private:
  /*
    Service registry service pointer.
  */
  iface::Service_registry *m_registry;
  /*
    Audit API service pointer acquired during the object construction.
  */
  SERVICE_TYPE_NO_CONST(mysql_audit_api_connection) * m_audit_api;
  /*
    Audit API service pointer acquired during the object construction.
  */
  SERVICE_TYPE_NO_CONST(mysql_audit_api_connection_with_error) *
      m_audit_api_error;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICE_AUDIT_API_CONNECTION_H_
