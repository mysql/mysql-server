/*
 Copyright (c) 2015, Oracle and/or its affiliates. All rights
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

#include <memcached/types.h>
#include <memcached/extension_loggers.h>

#include <NdbApi.hpp>

#include "atomics.h"
#include "Configuration.h"
#include "QueryPlan.h"
#include "SchedulerConfigManager.h"

SchedulerConfigManager::SchedulerConfigManager(int _thread, int _cluster) :
  thread(_thread),
  cluster(_cluster),
  current_plans(0),
  old_plans(0),
  nstatreq(0)
{
  DEBUG_ENTER_DETAIL();
}


SchedulerConfigManager::~SchedulerConfigManager() {
  DEBUG_ENTER_DETAIL();
  if(current_plans) delete current_plans;
  if(old_plans) delete old_plans;
}


/* This is a partial implementation of online reconfiguration.
   It can replace KeyPrefix mappings, but not add a cluster at runtime 
*/
void SchedulerConfigManager::configure(Configuration *conf) {
  DEBUG_ENTER();

  /* Get my Ndb Cluster Connection */
  ndb_connection = conf->getConnectionPoolById(cluster)->getPooledConnection(thread);

  /* Get a set of QueryPlans for the new configuration */
  ConnQueryPlanSet * plans = new ConnQueryPlanSet(ndb_connection, conf->nprefixes);
  plans->buildSetForConfiguration(conf, cluster);

  /* Garbage Collect old old plans */
  if(old_plans) {
    delete old_plans;
  }
  /* Save current plans as old ones */
  old_plans = (ConnQueryPlanSet*) current_plans;

  /* Swap new plans into place */
  atomic_set_ptr((void * volatile *)& current_plans, plans);
}


const KeyPrefix * SchedulerConfigManager::setQueryPlanInWorkitem(workitem * item) {
  const ConnQueryPlanSet * plans = current_plans;
  const KeyPrefix * prefix =
    plans->getConfiguration()->getPrefixByInfo(item->prefix_info);

  /* Set length in workitem */
  item->base.nsuffix = item->base.nkey - prefix->prefix_len;

  /* Set QueryPlan in workitem */
  item->plan = plans->getPlanForPrefix(prefix);

  return prefix;
}


void SchedulerConfigManager::add_stats(const char *stat_key,
                                       ADD_STAT add_stat,
                                       const void *cookie) {
  if(strncasecmp(stat_key, "reconf", 6) == 0) {
    const char * key = "Running";
    char buffer[16];
    int gen = current_plans->getConfiguration()->generation;
    int value_len = snprintf(buffer, 16, "%d", gen);
    add_stat(key, strlen(key), buffer, value_len, cookie);
    DEBUG_PRINT("stats reconf [req %d]: running %d", ++nstatreq, gen);
  }
}

