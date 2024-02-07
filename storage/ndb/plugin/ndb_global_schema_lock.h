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

#ifndef NDB_GLOBAL_SCHEMA_LOCK_H
#define NDB_GLOBAL_SCHEMA_LOCK_H

class THD;

/**
  Locks or unlock the GSL, thus preventing concurrent
  modification to any other object in the cluster

  @param thd                Thread context.
  @param lock               Indicates whether GSL should be locked or unlocked
  @param record_gsl         Record and track the GSL so that it can be used to
                            detect any possible deadlocks
  @param victimized         'true' if locking failed as we were chosen
                             as a victim in order to avoid possible deadlocks.

  @return false ok
  @return true  error
*/

bool ndb_gsl_lock(THD *thd, bool lock, bool record_gsl, bool *victimized);

#endif
