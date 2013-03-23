/* Copyright (C) 2008 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef BACKUP_LOCK_TAB
#define BACKUP_LOCK_TAB

#include "SignalData.hpp"

/* This class is used for both REQ, CONF, and REF. */

class BackupLockTab {
  /* Sender(s). */
  friend class Backup;

  /* Receiver(s). */
  friend class Dbdict;

public:
  STATIC_CONST( SignalLength = 7 );

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

#endif
