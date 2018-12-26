/*
 Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 
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

#include <memcached/types.h>
#include <memcached/extension_loggers.h>

#include <NdbApi.hpp>

#include "atomics.h"
#include "Configuration.h"
#include "GlobalConfigManager.h"

class SchedulerConfigManager;  // forward declaration


GlobalConfigManager::GlobalConfigManager(int _nthreads) :
  nthreads(_nthreads),
  conf(& get_Configuration()),
  generation(0)
{
  DEBUG_ENTER();
  conf->generation = 0;
  nclusters = conf->nclusters;

  /* Initialize the list that will hold SchedulerConfigManagers */
  schedulerConfigManagers = (SchedulerConfigManager **)
    calloc(sizeof(void *), nthreads * nclusters);

  /* We do not create and configure the SchedulerConfigManagers,
     but require a user or derived class to do so.
  */
}


GlobalConfigManager::~GlobalConfigManager() {
  if(schedulerConfigManagers)
    free(schedulerConfigManagers);
}


void GlobalConfigManager::configureSchedulers() {
  for(int i = 0; i < nclusters ; i++) {
    for(int j = 0; j < nthreads; j++) {
      SchedulerConfigManager *wc = * (getSchedulerConfigManagerPtr(j, i));
      wc->configure(conf);
    }
  }
}


bool GlobalConfigManager::reconfigure(Configuration * new_cf) {
  conf = new_cf;
  conf->generation = ++generation;
  DEBUG_PRINT("SchedulerGlobal::reconfigure generation %d", generation);
  configureSchedulers();
  atomic_barrier();
  return true;
}

