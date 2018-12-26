/*
   Copyright (c) 2018, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef REDO_STATE_REP_HPP
#define REDO_STATE_REP_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 500

class RedoStateRep
{
  /**
   * Sender(s)
   * Receiver(s)
   */
  friend class Backup;
  friend class Dbdih;
  friend class Ndbcntr;

  friend bool printREDO_STATE_REP(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 2);

  enum ReceiverInfo
  {
    ToNdbcntr = 0,
    ToLocalDih = 1,
    ToAllDih = 2,
    ToBackup = 3
  };
  enum RedoAlertState
  {
    NO_REDO_ALERT = 0,
    REDO_ALERT_LOW = 1,
    REDO_ALERT_HIGH = 2,
    REDO_ALERT_CRITICAL = 3
  };
private:
  Uint32 receiverInfo;
  Uint32 redoState;
};

#undef JAM_FILE_ID
#endif
