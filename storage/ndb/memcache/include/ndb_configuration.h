/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_NDBCONFIGURATION_H
#define NDBMEMCACHE_NDBCONFIGURATION_H

/* 
  Configuration is shared between the engine thread (which is C) and the 
  back-end NDB code (which is C++). 

  The functions here have C linkage, and serve as a wrapper around the C++ 
  Configuration methods.
*/

#include "atomics.h"
#include "KeyPrefix.h"
#include "ndbmemcache_global.h"

DECLARE_FUNCTIONS_WITH_C_LINKAGE

bool connect_to_primary_cluster(const char *connectstring, const char *role);
bool get_config();
bool open_connections_to_all_clusters();
bool prefetch_dictionary_objects();
prefix_info_t get_prefix_info_for_key(int nkey, const char *key);
void disconnect_all();
void set_initial_cas_ids(unsigned int *hi, atomic_int32_t *lo);
void print_debug_startup_info();
void start_reconfig_listener(void *scheduler);

END_FUNCTIONS_WITH_C_LINKAGE

#endif
