/*
   Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements
#include "storage/ndb/plugin/ndb_mysql_services.h"

// Uses
#ifndef NULL
// Fix NULL used in my_host_application_signal.h without include
#define NULL nullptr
#endif
#include "mysql/components/services/my_host_application_signal.h"
#include "mysql/service_plugin_registry.h"

Ndb_mysql_services::Ndb_mysql_services()
    : m_registry(mysql_plugin_registry_acquire()) {}

Ndb_mysql_services::~Ndb_mysql_services() {
  mysql_plugin_registry_release(m_registry);
}

bool Ndb_mysql_services::request_mysql_server_shutdown() const {
  return my_host_application_signal_shutdown(m_registry);
}
