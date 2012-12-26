/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <kernel/statedesc.hpp>
#define DBTC_STATE_EXTRACT
#include "Dbtc.hpp"

#define SDESC(a,b,c) { (unsigned)Dbtc::a, #a, b, c }

/**
 * Value
 * Friendly name
 * Description
 */
struct ndbkernel_state_desc g_dbtc_apiconnect_state_desc[] =
{
  SDESC(CS_CONNECTED, "Connected",
        "An allocated idle transaction object"),
  SDESC(CS_DISCONNECTED, "Disconnected",
        "An unallocated connection object"),
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
  { 0, 0, 0, 0 }
};
