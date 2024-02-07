/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef DBLQH_STATE_HPP
#define DBLQH_STATE_HPP

/*
 * The state values are published in
 * ndbinfo.ndb$dblqh_tcconnect_state.state_int_value and
 * ndbinfo.ndb$operations.state and should not be changed as long as any
 * supported version of data nodes expose them anywhere such in
 * ndbinfo.ndb$operations.state.
 *
 * The values of dblqh_tcconnect_state should match values in
 * Dblqh::TcConnectionrec::TransactionState.
 */
struct dblqh_tcconnect_state {
  enum {
    IDLE = 0,
    WAIT_ACC = 1,
    WAIT_TUP = 4,
    LOG_QUEUED = 6,
    PREPARED = 7,
    LOG_COMMIT_WRITTEN_WAIT_SIGNAL = 8,
    LOG_COMMIT_QUEUED_WAIT_SIGNAL = 9,
    LOG_COMMIT_QUEUED = 11,
    COMMITTED = 13,
    WAIT_TUP_COMMIT = 35,
    WAIT_ACC_ABORT = 14,
    LOG_ABORT_QUEUED = 18,
    WAIT_TUP_TO_ABORT = 19,
    SCAN_STATE_USED = 21,
    SCAN_TUPKEY = 30,
    COPY_TUPKEY = 31,
    TC_NOT_CONNECTED = 32,
    PREPARED_RECEIVED_COMMIT = 33,
    LOG_COMMIT_WRITTEN = 34
  };
};

#endif
