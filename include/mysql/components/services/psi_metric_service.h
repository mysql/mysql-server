/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMPONENTS_SERVICES_PSI_METRIC_SERVICE_H
#define COMPONENTS_SERVICES_PSI_METRIC_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/psi_metric_bits.h>

BEGIN_SERVICE_DEFINITION(psi_metric_v1)
/** @sa register_meters_v1_t. */
register_meters_v1_t register_meters;
/** @sa unregister_meters_v1_t. */
unregister_meters_v1_t unregister_meters;
/** @sa register_change_notification_v1_t. */
register_change_notification_v1_t register_change_notification;
/** @sa unregister_change_notification_v1_t. */
unregister_change_notification_v1_t unregister_change_notification;
/** @sa send_change_notification_v1_t. */
send_change_notification_v1_t send_change_notification;
END_SERVICE_DEFINITION(psi_metric_v1)

#endif /* COMPONENTS_SERVICES_PSI_METRIC_SERVICE_H */
