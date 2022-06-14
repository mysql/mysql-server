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

#ifndef COMPONENTS_SERVICES_PSI_TLS_CHANNEL_H
#define COMPONENTS_SERVICES_PSI_TLS_CHANNEL_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/psi_tls_channel_service.h>

#define REQUIRES_PSI_TLS_CHANNEL_SERVICE REQUIRES_SERVICE(psi_tls_channel_v1)
#define REQUIRES_PSI_TLS_CHANNEL_SERVICE_PLACEHOLDER \
  REQUIRES_SERVICE_PLACEHOLDER(psi_tls_channel_v1)

extern REQUIRES_PSI_TLS_CHANNEL_SERVICE_PLACEHOLDER;

#define PSI_TLS_CHANNEL_CALL(M) mysql_service_psi_tls_channel_v1->M

#endif  // !COMPONENTS_SERVICES_PSI_TLS_CHANNEL_H
