/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
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

#ifndef NDBMEMCACHE_CONNQUERYPLANSET_H
#define NDBMEMCACHE_CONNQUERYPLANSET_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif


#include "Configuration.h"
#include "QueryPlan.h"

class ConnQueryPlanSet {
public:
  ConnQueryPlanSet(Ndb_cluster_connection *, int n_plans);
  ~ConnQueryPlanSet();

  void buildSetForConfiguration(const Configuration *, int cluster_id);
  QueryPlan * getPlanForPrefix(const KeyPrefix *) const;
  const Configuration * getConfiguration() const { return config; };

private:
  Ndb *db;
  int nplans;
  QueryPlan **plans;
  const Configuration * config;
};



#endif

