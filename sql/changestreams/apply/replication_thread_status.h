/*
   Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CHANGE_STREAM_REPLICATION_THREAD_STATUS_H_
#define CHANGE_STREAM_REPLICATION_THREAD_STATUS_H_

#include "sql/rpl_mi.h"

/**
  This method locks both (in this order)
    mi->run_lock
    rli->run_lock

  @param mi The associated master info object

  @note this method shall be invoked while locking mi->m_channel_lock
  for writes. This is due to the mixed order in which these locks are released
  and acquired in such method as the slave threads start and stop methods.
*/
void lock_slave_threads(Master_info *mi);

/**
  Unlock replica master and relay log info locks.

  @param mi the repo info object that contains the lock
*/
void unlock_slave_threads(Master_info *mi);

/**
  Find out which replications threads are running

  @param mask                   Return value here
  @param mi                     master_info for slave
  @param inverse                If set, returns which threads are not running
  @param ignore_monitor_thread  If set, ignores monitor io thread

  IMPLEMENTATION
    Get a bit mask for which threads are running.
*/
void init_thread_mask(int *mask, Master_info *mi, bool inverse,
                      bool ignore_monitor_thread = false);

#endif /* CHANGE_STREAM_REPLICATION_THREAD_STATUS_H_ */
