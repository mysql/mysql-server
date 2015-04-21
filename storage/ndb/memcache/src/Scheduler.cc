/*
 Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licensein
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

#include "Scheduler.h"
#include "workitem.h"
#include "ndb_pipeline.h"


void Scheduler::execute(NdbTransaction * tx, NdbTransaction::ExecType ex, 
                        NdbAsynchCallback cb, struct workitem * item, 
                        prepare_flags flags)
{
  item->pipeline->scheduler->prepare(tx, ex, cb, item, flags);
}

