/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
  static constexpr Uint32 SignalLength = 2;

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
