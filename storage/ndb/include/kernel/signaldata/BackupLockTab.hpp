/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef BACKUP_LOCK_TAB
#define BACKUP_LOCK_TAB

#include "SignalData.hpp"

#define JAM_FILE_ID 82


/* This class is used for both REQ, CONF, and REF. */

class BackupLockTab {
  /* Sender(s). */
  friend class Backup;

  /* Receiver(s). */
  friend class Dbdict;

public:
  static constexpr Uint32 SignalLength = 7;

private:
  /* Values for m_lock_unlock. */
  enum {
    UNLOCK_TABLE = 0,
    LOCK_TABLE = 1
  };

  /* Values for m_backup_state. */
  enum {
    BACKUP_FRAGMENT_INFO = 0,
    GET_TABINFO_CONF = 1,
    CLEANUP = 2
  };

  Uint32 m_senderRef;
  Uint32 m_tableId;
  Uint32 m_lock_unlock;
  Uint32 errorCode;
  /* The remaining words are used to keep track of state in block Backup. */
  Uint32 m_backup_state;
  Uint32 m_backupRecordPtr_I;
  Uint32 m_tablePtr_I;
};


#undef JAM_FILE_ID

#endif
