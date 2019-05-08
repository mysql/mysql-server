/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"

#include <assert.h>
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"

/* Reasonable initial cache limit */
#define DEFAULT_CACHE_LIMIT 1000000000ULL

cfg_app_xcom_st *the_app_xcom_cfg = NULL;

void init_cfg_app_xcom() {
  if (!the_app_xcom_cfg)
    the_app_xcom_cfg = (cfg_app_xcom_st *)malloc(sizeof(cfg_app_xcom_st));

  the_app_xcom_cfg->m_poll_spin_loops = 0;
  the_app_xcom_cfg->m_cache_limit = DEFAULT_CACHE_LIMIT;
  the_app_xcom_cfg->identity = NULL;
}

void deinit_cfg_app_xcom() {
  /* Delete the identity because we own it. */
  if (the_app_xcom_cfg != NULL && the_app_xcom_cfg->identity != NULL) {
    delete_node_address(1, the_app_xcom_cfg->identity);
  }
  free(the_app_xcom_cfg);
  the_app_xcom_cfg = NULL;
}

node_address *cfg_app_xcom_get_identity() {
  node_address *identity = NULL;
  if (the_app_xcom_cfg != NULL) identity = the_app_xcom_cfg->identity;
  return identity;
}

void cfg_app_xcom_set_identity(node_address *identity) {
  /* Validate preconditions. */
  assert(identity != NULL);

  /*
   If the configuration structure was setup, store the identity.
   If not, delete the identity because we own it.
  */
  if (the_app_xcom_cfg != NULL) {
    if (the_app_xcom_cfg->identity != NULL) {
      delete_node_address(1, the_app_xcom_cfg->identity);
    }
    the_app_xcom_cfg->identity = identity;
  } else {
    delete_node_address(1, identity);
  }
}
