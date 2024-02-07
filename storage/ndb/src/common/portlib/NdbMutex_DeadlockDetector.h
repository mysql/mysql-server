/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MUTEX_DEADLOCK_DETECTOR_H
#define NDB_MUTEX_DEADLOCK_DETECTOR_H

void NdbMutex_DeadlockDetectorInit();
void NdbMutex_DeadlockDetectorEnd();

void ndb_mutex_created(struct ndb_mutex_state *&);
void ndb_mutex_destroyed(struct ndb_mutex_state *&);
void ndb_mutex_locked(struct ndb_mutex_state *, bool is_blocking = true);
void ndb_mutex_unlocked(struct ndb_mutex_state *);

void ndb_mutex_thread_init(struct ndb_mutex_thr_state *&);
void ndb_mutex_thread_exit(struct ndb_mutex_thr_state *&);

#endif
