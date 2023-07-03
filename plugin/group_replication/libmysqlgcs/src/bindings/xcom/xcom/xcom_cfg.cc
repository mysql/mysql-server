/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <assert.h>

#include "xcom/xcom_cfg.h"

#include "xcom/node_list.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"

cfg_app_xcom_st *the_app_xcom_cfg = nullptr;

void init_cfg_app_xcom() {
  if (!the_app_xcom_cfg)
    the_app_xcom_cfg = (cfg_app_xcom_st *)xcom_malloc(sizeof(cfg_app_xcom_st));

  the_app_xcom_cfg->m_poll_spin_loops = 0;
  the_app_xcom_cfg->m_cache_limit = DEFAULT_CACHE_LIMIT;
  the_app_xcom_cfg->identity = nullptr;
}

void deinit_cfg_app_xcom() {
  /* Delete the identity because we own it. */
  if (the_app_xcom_cfg != nullptr && the_app_xcom_cfg->identity != nullptr) {
    delete_node_address(1, the_app_xcom_cfg->identity);
  }
  free(the_app_xcom_cfg);
  the_app_xcom_cfg = nullptr;
}

Network_namespace_manager *cfg_app_get_network_namespace_manager() {
  Network_namespace_manager *mgr = nullptr;
  if (the_app_xcom_cfg != nullptr) mgr = the_app_xcom_cfg->network_ns_manager;
  return mgr;
}

node_address *cfg_app_xcom_get_identity() {
  node_address *identity = nullptr;
  if (the_app_xcom_cfg != nullptr) identity = the_app_xcom_cfg->identity;
  return identity;
}

void cfg_app_xcom_set_identity(node_address *identity) {
  /* Validate preconditions. */
  assert(identity != nullptr);

  /*
   If the configuration structure was setup, store the identity.
   If not, delete the identity because we own it.
  */
  if (the_app_xcom_cfg != nullptr) {
    if (the_app_xcom_cfg->identity != nullptr) {
      delete_node_address(1, the_app_xcom_cfg->identity);
    }
    the_app_xcom_cfg->identity = identity;
  } else {
    delete_node_address(1, identity);
  }
}
