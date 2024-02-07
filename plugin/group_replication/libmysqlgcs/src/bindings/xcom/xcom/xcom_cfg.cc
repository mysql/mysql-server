/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include <assert.h>

#include "xcom/xcom_cfg.h"

#include "xcom/node_list.h"
#include "xcom/statistics/include/statistics_storage_interface_default_impl.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"

cfg_app_xcom_st *the_app_xcom_cfg = nullptr;

static Xcom_statistics_storage_interface *default_stats_storage_impl = nullptr;

void init_cfg_app_xcom() {
  if (!the_app_xcom_cfg)
    the_app_xcom_cfg = (cfg_app_xcom_st *)xcom_malloc(sizeof(cfg_app_xcom_st));

  the_app_xcom_cfg->m_poll_spin_loops = 0;
  the_app_xcom_cfg->m_cache_limit = DEFAULT_CACHE_LIMIT;
  the_app_xcom_cfg->identity = nullptr;
  the_app_xcom_cfg->network_ns_manager = nullptr;
  the_app_xcom_cfg->statistics_storage = nullptr;
}

void deinit_cfg_app_xcom() {
  /* Delete the identity because we own it. */
  if (the_app_xcom_cfg != nullptr && the_app_xcom_cfg->identity != nullptr) {
    delete_node_address(1, the_app_xcom_cfg->identity);
  }

  if (default_stats_storage_impl != nullptr) {
    /* purecov: begin inspected */
    delete default_stats_storage_impl;
    default_stats_storage_impl = nullptr;
    /* purecov: end */
  }

  free(the_app_xcom_cfg);
  the_app_xcom_cfg = nullptr;
}

Network_namespace_manager *cfg_app_get_network_namespace_manager() {
  Network_namespace_manager *mgr = nullptr;
  if (the_app_xcom_cfg != nullptr) mgr = the_app_xcom_cfg->network_ns_manager;
  return mgr;
}

Xcom_statistics_storage_interface *cfg_app_get_storage_statistics() {
  Xcom_statistics_storage_interface *mgr = nullptr;
  if (the_app_xcom_cfg != nullptr &&
      the_app_xcom_cfg->statistics_storage != nullptr) {
    mgr = the_app_xcom_cfg->statistics_storage;
  } else {
    /* purecov: begin inspected */
    if (default_stats_storage_impl == nullptr) {
      default_stats_storage_impl =
          new Xcom_statistics_storage_interface_default_impl();
    }

    mgr = default_stats_storage_impl;
    /* purecov: end */
  }

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
