/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_CONFIG_V1_H
#define NDBMEMCACHE_CONFIG_V1_H

#include "Configuration.h" 
#include "LookupTable.h"
#include "KeyPrefix.h"
#include "TableSpec.h"

class config_v1 {
public:
  /* Public Methods */
  config_v1(Configuration * cf);
  virtual ~config_v1();
  bool read_configuration();
  virtual void minor_version_config() = 0;
  
protected:
  void define_tablespecs();
  int get_server_role_id();
  bool get_connections();
  bool get_policies();
  bool get_prefixes(int role_id);
  bool store_prefix(const char *, TableSpec *, int, char *);
  TableSpec * get_container(char *name);
  virtual TableSpec * get_container_record(char *name);
  void log_signon();
  void set_initial_cas();
  
  /* Private instance variables */
  Configuration &conf;
  int server_role_id;
  Uint64 signon_gci;  
  int nclusters;
  int cluster_ids[MAX_CLUSTERS];
  LookupTable<prefix_info_t> * policies_map;
  LookupTable<TableSpec> * containers_map;
};


class config_v1_0 : public config_v1 {
public:
  config_v1_0(Configuration * cf) : config_v1(cf) {};
  virtual void minor_version_config();
};


class config_v1_1 : public config_v1 {
public:
  config_v1_1(Configuration * cf) : config_v1(cf) {};
  virtual void minor_version_config();  
};


class config_v1_2 : public config_v1 { 
public:
  config_v1_2(Configuration * cf) : config_v1(cf) {};
  virtual TableSpec * get_container_record(char *name);
  virtual void minor_version_config();
};


#endif
