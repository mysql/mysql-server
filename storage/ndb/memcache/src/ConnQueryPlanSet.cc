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

#include "debug.h"
#include "ConnQueryPlanSet.h"

ConnQueryPlanSet::ConnQueryPlanSet(Ndb_cluster_connection *conn, int n) :
  nplans(n),
  plans(new QueryPlan *[n])
{
  memset(plans, 0, (nplans * sizeof(QueryPlan *)));  
  db = new Ndb(conn);
  db->init();
}


ConnQueryPlanSet::~ConnQueryPlanSet() 
{
  delete db;
  delete plans;
}


void ConnQueryPlanSet:: buildSetForConfiguration(const Configuration *cf,
                                                 int cluster_id)
{
  const KeyPrefix *k = cf->getNextPrefixForCluster(cluster_id, NULL);
  while(k) {
    getPlanForPrefix(k);
    k = cf->getNextPrefixForCluster(cluster_id, k);
  }
}


QueryPlan * ConnQueryPlanSet::getPlanForPrefix(const KeyPrefix *prefix) { 
  int plan_id = prefix->info.prefix_id;
  
  if(plans[plan_id] == 0) {
    plans[plan_id] = new QueryPlan(db, prefix->table);
  }
  
  if(plans[plan_id]->initialized)
    return plans[plan_id];
  else
    return 0;
}
