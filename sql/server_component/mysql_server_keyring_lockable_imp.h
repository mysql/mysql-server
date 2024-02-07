/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_KEYRING_LOCKABLE_IMP_INCLUDED
#define MYSQL_SERVER_KEYRING_LOCKABLE_IMP_INCLUDED

#include <atomic>
#include <string>

#include <mysql/components/my_service.h>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_aes.h>
#include <mysql/components/services/keyring_generator.h>
#include <mysql/components/services/keyring_keys_metadata_iterator.h>
#include <mysql/components/services/keyring_load.h>
#include <mysql/components/services/keyring_metadata_query.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/components/services/keyring_writer.h>

extern SERVICE_TYPE(keyring_aes) * srv_keyring_aes;
extern SERVICE_TYPE(keyring_generator) * srv_keyring_generator;
extern SERVICE_TYPE(keyring_keys_metadata_iterator) *
    srv_keyring_keys_metadata_iterator;
extern SERVICE_TYPE(keyring_component_status) * srv_keyring_component_status;
extern SERVICE_TYPE(keyring_component_metadata_query) *
    srv_keyring_component_metadata_query;
extern SERVICE_TYPE(keyring_reader_with_status) * srv_keyring_reader;
extern SERVICE_TYPE(keyring_load) * srv_keyring_load;
extern SERVICE_TYPE(keyring_writer) * srv_keyring_writer;

void init_srv_event_tracking_handles();
void deinit_srv_event_tracking_handles();

namespace keyring_lockable {

/* Keyring_encryption_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_encryption_service_definition.h>
/* Keyring_generator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_generator_service_definition.h>
/* Keyring_keys_metadata_iterator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_keys_metadata_iterator_service_definition.h>
/* Keyring_metadata_query_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_metadata_query_service_definition.h>
/* Keyring_reader_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_reader_service_definition.h>
/* Keyring_load_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_load_service_definition.h>
/* Keyring_writer_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_writer_service_definition.h>

}  // namespace keyring_lockable

void keyring_lockable_init();
void keyring_lockable_deinit();
void set_srv_keyring_implementation_as_default();
void release_keyring_handles();
bool keyring_status_no_error();

#endif /* MYSQL_SERVER_KEYRING_LOCKABLE_IMP_INCLUDED */
