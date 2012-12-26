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
#ifndef NDBMEMCACHE_NDB_WORKER_H
#define NDBMEMCACHE_NDB_WORKER_H

/* There are two public entry points into ndb_worker: 

   1: worker_prepare_operation(), for normal async ops.
   2: ndb_flush_all(), for FLUSH commands, which are run synchronously.
*/
op_status_t worker_prepare_operation(workitem *);
ENGINE_ERROR_CODE ndb_flush_all(ndb_pipeline *);

/* Expiration */
void delete_expired_item(workitem *, NdbTransaction *);

/* An ndb_async_callback is used with NDB Async execution */
typedef void ndb_async_callback(int, NdbTransaction *, void *);

/* workitem.next_step is set to a function of type  worker_step */
typedef void worker_step(NdbTransaction *, workitem *);

#endif
