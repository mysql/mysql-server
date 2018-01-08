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
  int get_server_role_id(NdbTransaction *);
  bool get_connections(NdbTransaction *);
  bool get_policies(NdbTransaction *);
  bool get_prefixes(int role_id, NdbTransaction *);
  bool store_prefix(const char *, TableSpec *, int, char *);
  TableSpec * get_container(char *name, NdbTransaction *);
  virtual TableSpec * get_container_record(char *name, NdbTransaction *);
  void log_signon(NdbTransaction *);
  void set_initial_cas();
  
  /* Private instance variables */
  Ndb db;
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
  virtual TableSpec * get_container_record(char *name, NdbTransaction *);
  virtual void minor_version_config();
};


#endif
