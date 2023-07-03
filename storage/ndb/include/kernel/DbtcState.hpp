/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef DBTC_STATE_HPP
#define DBTC_STATE_HPP

/*
 * The state values are published in
 * ndbinfo.ndb$dbtc_apiconnect_state.state_int_value and
 * ndbinfo.ndb$transactions.state and should not be changed as long as any
 * supported version of data nodes expose them anywhere such in
 * ndbinfo.ndb$transactions.state.
 *
 * The values of dbtc_apiconnect_state should match values in
 * Dbtc::ConnectionState.
 */
struct dbtc_apiconnect_state
{
  enum
  {
    CS_CONNECTED = 0,
    CS_DISCONNECTED = 1,
    CS_STARTED = 2,
    CS_RECEIVING = 3,
    CS_RESTART = 7,
    CS_ABORTING = 8,
    CS_COMPLETING = 9,
    CS_COMPLETE_SENT = 10,
    CS_PREPARE_TO_COMMIT = 11,
    CS_COMMIT_SENT = 12,
    CS_START_COMMITTING = 13,
    CS_COMMITTING = 14,
    CS_REC_COMMITTING = 15,
    CS_WAIT_ABORT_CONF = 16,
    CS_WAIT_COMPLETE_CONF = 17,
    CS_WAIT_COMMIT_CONF = 18,
    CS_FAIL_ABORTING = 19,
    CS_FAIL_ABORTED = 20,
    CS_FAIL_PREPARED = 21,
    CS_FAIL_COMMITTING = 22,
    CS_FAIL_COMMITTED = 23,
    CS_FAIL_COMPLETED = 24,
    CS_START_SCAN = 25,
    CS_SEND_FIRE_TRIG_REQ = 26,
    CS_WAIT_FIRE_TRIG_REQ = 27
  };
};

#endif
