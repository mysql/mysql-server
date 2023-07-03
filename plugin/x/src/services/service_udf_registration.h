/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_SERVICES_SERVICE_UDF_REGISTRATION_H_
#define PLUGIN_X_SRC_SERVICES_SERVICE_UDF_REGISTRATION_H_

#include "mysql/components/services/udf_registration.h"
#include "plugin/x/src/interface/service_registry.h"
#include "plugin/x/src/interface/service_udf_registration.h"

namespace xpl {

class Service_udf_registration : public iface::Service_udf_registration {
 public:
  explicit Service_udf_registration(iface::Service_registry *registry);
  ~Service_udf_registration() override;

  bool udf_register(const char *func_name, enum Item_result return_type,
                    Udf_func_any func, Udf_func_init init_func,
                    Udf_func_deinit deinit_func) override;

  bool udf_unregister(const char *name, int *was_present) override;
  bool is_valid() override;

 private:
  iface::Service_registry *m_registry;
  SERVICE_TYPE_NO_CONST(udf_registration) * m_udf_registration;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICE_UDF_REGISTRATION_H_
