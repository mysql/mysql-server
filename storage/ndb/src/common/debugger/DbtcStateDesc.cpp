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

#include "kernel/DbtcState.hpp"
#include "kernel/statedesc.hpp"

#define JAM_FILE_ID 351

#define SDESC(a, b, c) \
  { (unsigned)dbtc_apiconnect_state::a, #a, b, c }

/**
 * Value
 * Friendly name
 * Description
 */
struct ndbkernel_state_desc g_dbtc_apiconnect_state_desc[] = {
    SDESC(CS_CONNECTED, "Connected", "An allocated idle transaction object"),
    SDESC(CS_DISCONNECTED, "Disconnected", "An unallocated connection object"),
    SDESC(CS_STARTED, "Started", "A started transaction"),
    SDESC(CS_RECEIVING, "Receiving", "A transaction receiving operations"),
    SDESC(CS_RESTART, "", ""),
    SDESC(CS_ABORTING, "Aborting", "A transaction aborting"),
    SDESC(CS_COMPLETING, "Completing", "A transaction completing"),
    SDESC(CS_COMPLETE_SENT, "Completing", "A transaction completing"),
    SDESC(CS_PREPARE_TO_COMMIT, "", ""),
    SDESC(CS_COMMIT_SENT, "Committing", "A transaction committing"),
    SDESC(CS_START_COMMITTING, "", ""),
    SDESC(CS_COMMITTING, "Committing", "A transaction committing"),
    SDESC(CS_REC_COMMITTING, "", ""),
    SDESC(CS_WAIT_ABORT_CONF, "Aborting", ""),
    SDESC(CS_WAIT_COMPLETE_CONF, "Completing", ""),
    SDESC(CS_WAIT_COMMIT_CONF, "Committing", ""),
    SDESC(CS_FAIL_ABORTING, "TakeOverAborting", ""),
    SDESC(CS_FAIL_ABORTED, "TakeOverAborting", ""),
    SDESC(CS_FAIL_PREPARED, "", ""),
    SDESC(CS_FAIL_COMMITTING, "TakeOverCommitting", ""),
    SDESC(CS_FAIL_COMMITTED, "TakeOverCommitting", ""),
    SDESC(CS_FAIL_COMPLETED, "TakeOverCompleting", ""),
    SDESC(CS_START_SCAN, "Scanning", ""),
    SDESC(CS_SEND_FIRE_TRIG_REQ, "Precomitting", ""),
    SDESC(CS_WAIT_FIRE_TRIG_REQ, "Precomitting", ""),
    {0, nullptr, nullptr, nullptr}};
