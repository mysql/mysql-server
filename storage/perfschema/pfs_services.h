/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_SERVICES_H
#define PFS_SERVICES_H

#include "mysql/components/services/registry.h"

// pfs services
#include <mysql/components/services/psi_cond_service.h>
#include <mysql/components/services/psi_error_service.h>
#include <mysql/components/services/psi_file_service.h>
#include <mysql/components/services/psi_idle_service.h>
#include <mysql/components/services/psi_mdl_service.h>
#include <mysql/components/services/psi_memory_service.h>
#include <mysql/components/services/psi_mutex_service.h>
#include <mysql/components/services/psi_rwlock_service.h>
#include <mysql/components/services/psi_socket_service.h>
#include <mysql/components/services/psi_stage_service.h>
#include <mysql/components/services/psi_statement_service.h>
#include <mysql/components/services/psi_system_service.h>
#include <mysql/components/services/psi_table_service.h>
#include <mysql/components/services/psi_thread_service.h>
#include <mysql/components/services/psi_tls_channel_service.h>
#include <mysql/components/services/psi_transaction_service.h>

bool pfs_init_services(SERVICE_TYPE(registry_registration) * reg);
bool pfs_deinit_services(SERVICE_TYPE(registry_registration) * reg);

extern SERVICE_TYPE(psi_cond_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_cond_v1);
extern SERVICE_TYPE(psi_error_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_error_v1);
extern SERVICE_TYPE(psi_file_v2)
    SERVICE_IMPLEMENTATION(performance_schema, psi_file_v2);
extern SERVICE_TYPE(psi_idle_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_idle_v1);
extern SERVICE_TYPE(psi_mdl_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_mdl_v1);
extern SERVICE_TYPE(psi_mdl_v2)
    SERVICE_IMPLEMENTATION(performance_schema, psi_mdl_v2);
extern SERVICE_TYPE(psi_memory_v2)
    SERVICE_IMPLEMENTATION(performance_schema, psi_memory_v2);
extern SERVICE_TYPE(psi_mutex_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_mutex_v1);
extern SERVICE_TYPE(psi_rwlock_v2)
    SERVICE_IMPLEMENTATION(performance_schema, psi_rwlock_v2);
extern SERVICE_TYPE(psi_socket_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_socket_v1);
extern SERVICE_TYPE(psi_stage_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_stage_v1);
extern SERVICE_TYPE(psi_statement_v4)
    SERVICE_IMPLEMENTATION(performance_schema, psi_statement_v4);
extern SERVICE_TYPE(psi_system_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_system_v1);
extern SERVICE_TYPE(psi_table_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_table_v1);
extern SERVICE_TYPE(psi_thread_v4)
    SERVICE_IMPLEMENTATION(performance_schema, psi_thread_v4);
extern SERVICE_TYPE(psi_thread_v5)
    SERVICE_IMPLEMENTATION(performance_schema, psi_thread_v5);
extern SERVICE_TYPE(psi_thread_v6)
    SERVICE_IMPLEMENTATION(performance_schema, psi_thread_v6);
extern SERVICE_TYPE(psi_transaction_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_transaction_v1);
extern SERVICE_TYPE(psi_tls_channel_v1)
    SERVICE_IMPLEMENTATION(performance_schema, psi_tls_channel_v1);

#endif /* PFS_SERVICES_H */
