/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef SYNCTHREADVIAREQCONF_HPP
#define SYNCTHREADVIAREQCONF_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 514


/**
 * This is a local signal sent between QMGR and TRPMAN proxy do drain any pending
 * signals in THRMAN queue from each TRPMAN.
 *
 * Used when sending out NODE_FAILREP from Qmgr to ensure that all signals from
 * a failed node have been processed before NODE_FAILREP arrives.
 *
 * Also used to synchronize signal order when changing to multi socket setup.
 */

class SyncThreadViaReqConf
{
  /* Sender */
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;

  /* Receiver */
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;
  enum
  {
    FOR_NODE_FAILREP = 0,
    FOR_ACTIVATE_TRP_REQ = 1
  };

  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 actionType;
};

#undef JAM_FILE_ID

#endif
