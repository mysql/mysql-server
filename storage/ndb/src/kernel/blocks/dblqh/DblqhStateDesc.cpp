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
#define DBLQH_STATE_EXTRACT
#include "Dblqh.hpp"

#define SDESC(a,b,c) { (unsigned)Dblqh::TcConnectionrec::a, #a, b, c }

struct ndbkernel_state_desc g_dblqh_tcconnect_state_desc[] =
{
  SDESC(IDLE, "Idle", ""),
  SDESC(WAIT_ACC, "WaitLock", ""),
  SDESC(WAIT_TUPKEYINFO, "", ""),
  SDESC(WAIT_ATTR, "WaitData", ""),
  SDESC(WAIT_TUP, "WaitTup", ""),
  SDESC(STOPPED, "Stopped", ""),
  SDESC(LOG_QUEUED, "LogPrepare", ""),
  SDESC(PREPARED, "Prepared", ""),
  SDESC(LOG_COMMIT_WRITTEN_WAIT_SIGNAL, "", ""),
  SDESC(LOG_COMMIT_QUEUED_WAIT_SIGNAL, "", ""),

  // Commit in progress states
  /* -------------------------------------------------------------------- */
  SDESC(COMMIT_STOPPED, "CommittingStopped", ""),
  SDESC(LOG_COMMIT_QUEUED, "Committing", ""),
  SDESC(COMMIT_QUEUED, "Committing", ""),
  SDESC(COMMITTED, "Committed", ""),
  SDESC(WAIT_TUP_COMMIT, "Committing", ""),

  /* -------------------------------------------------------------------- */
  // Abort in progress states
  /* -------------------------------------------------------------------- */
  SDESC(WAIT_ACC_ABORT, "Aborting", ""),
  SDESC(ABORT_QUEUED, "Aborting", ""),
  SDESC(ABORT_STOPPED, "AbortingStopped", ""),
  SDESC(WAIT_AI_AFTER_ABORT, "Aborting", ""),
  SDESC(LOG_ABORT_QUEUED, "Aborting", ""),
  SDESC(WAIT_TUP_TO_ABORT, "Aborting", ""),

  /* -------------------------------------------------------------------- */
  // Scan in progress states
  /* -------------------------------------------------------------------- */
  SDESC(WAIT_SCAN_AI, "Scanning", ""),
  SDESC(SCAN_STATE_USED, "Scanning", ""),
  SDESC(SCAN_FIRST_STOPPED, "Scanning", ""),
  SDESC(SCAN_CHECK_STOPPED, "Scanning", ""),
  SDESC(SCAN_STOPPED, "ScanningStopped", ""),
  SDESC(SCAN_RELEASE_STOPPED, "ScanningStopped", ""),
  SDESC(SCAN_CLOSE_STOPPED, "ScanningStopped", ""),
  SDESC(COPY_CLOSE_STOPPED, "ScanningStopped", ""),
  SDESC(COPY_FIRST_STOPPED, "ScanningStopped", ""),
  SDESC(COPY_STOPPED, "ScanningStopped", ""),
  SDESC(SCAN_TUPKEY, "Scanning", ""),
  SDESC(COPY_TUPKEY, "NodeRecoveryScanning", ""),

  SDESC(TC_NOT_CONNECTED, "Idle", ""),
  SDESC(PREPARED_RECEIVED_COMMIT, "Committing", ""),
  SDESC(LOG_COMMIT_WRITTEN, "Committing", ""),

  { 0, 0, 0, 0 }
};
