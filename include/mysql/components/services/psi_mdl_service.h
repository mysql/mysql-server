/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_PSI_MDL_SERVICE_H
#define COMPONENTS_SERVICES_PSI_MDL_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/psi_mdl_bits.h>

/*
  Version 1.
  Introduced in MySQL 8.0.3
  Deprecated in MySQL 8.0.24
  Status: Deprecated.
*/
BEGIN_SERVICE_DEFINITION(psi_mdl_v1)
create_metadata_lock_v1_t create_metadata_lock;
set_metadata_lock_status_v1_t set_metadata_lock_status;
destroy_metadata_lock_v1_t destroy_metadata_lock;
start_metadata_wait_v1_t start_metadata_wait;
end_metadata_wait_v1_t end_metadata_wait;
END_SERVICE_DEFINITION(psi_mdl_v1)

/*
  Version 2.
  Introduced in MySQL 8.0.24
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(psi_mdl_v2)
create_metadata_lock_v1_t create_metadata_lock;
set_metadata_lock_status_v1_t set_metadata_lock_status;
/* Added in version 2. */
set_metadata_lock_duration_v2_t set_metadata_lock_duration;
destroy_metadata_lock_v1_t destroy_metadata_lock;
start_metadata_wait_v1_t start_metadata_wait;
end_metadata_wait_v1_t end_metadata_wait;
END_SERVICE_DEFINITION(psi_mdl_v2)

#endif /* COMPONENTS_SERVICES_PSI_MDL_SERVICE_H */
