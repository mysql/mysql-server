/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
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
