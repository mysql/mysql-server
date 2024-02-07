/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "event_tracking_registry.h"

#include "mysql/components/minimal_chassis.h" /* Minimal chassis */

namespace {
registry_type_t *components_registry = nullptr;
dynamic_loader_type_t *components_dynamic_loader = nullptr;
}  // namespace

registry_type_t *get_service_registry() { return components_registry; }

dynamic_loader_type_t *get_dynamic_loader() {
  return components_dynamic_loader;
}

void init_registry() {
  minimal_chassis_init((&components_registry), nullptr);
  components_registry->acquire(
      "dynamic_loader",
      reinterpret_cast<my_h_service *>(&components_dynamic_loader));
}

void deinit_registry() {
  components_registry->release(
      reinterpret_cast<my_h_service>(components_dynamic_loader));
  minimal_chassis_deinit(components_registry, nullptr);
}
