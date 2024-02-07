/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SERVICE_THD_EGINE_LOCK_INCLUDED
#define MYSQL_SERVICE_THD_EGINE_LOCK_INCLUDED

/**
  @file include/mysql/service_thd_engine_lock.h
  This service provides functions for storage engines to report
  lock related activities.
*/

class THD;

/** @deprecated Please use thd_report_lock_wait(self, wait_for, true) instead.
@see thd_report_lock_wait
Call it just when the engine find a transaction should wait another transaction
to release a row lock.
@param[in]   self      The thd session which is waiting for the lock to release
@param[in]   wait_for  The session which is holding the lock
*/
void thd_report_row_lock_wait(THD *self, THD *wait_for);

/**
Call it just when the engine find a transaction should wait another transaction
to release a lock.
Interface for Engine to report lock conflict.
The caller should guarantee self and thd_wait_for does not be freed,
while it is called.
@param[in]   self      The thd session which is waiting for the lock to release
@param[in]   wait_for  The session which is holding the lock
@param[in]   may_survive_prepare  true:  edge MAY remain even after wait_for
                                         session PREPAREs its transaction,
                                  false: edge CERTAINLY will be removed before
                                         or during PREPARE of transaction run
                                         by the wait_for session.
*/
void thd_report_lock_wait(THD *self, THD *wait_for, bool may_survive_prepare);
#endif
