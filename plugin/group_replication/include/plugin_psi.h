/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PLUGIN_PSI_INCLUDED
#define PLUGIN_PSI_INCLUDED


#include "plugin_server_include.h"


/*
  Register the psi keys for mutexes and conditions

  @param[in]  mutexes        PSI mutex info
  @param[in]  mutex_count    The number of elements in mutexes
  @param[in]  conditions     PSI condition info
  @param[in]  cond_count     The number of elements in conditions
*/
void register_group_replication_psi_keys(PSI_mutex_info mutexes[],
                                         int mutex_count,
                                         PSI_cond_info conditions[],
                                         int cond_count);


#endif /* PLUGIN_PSI_INCLUDED */
