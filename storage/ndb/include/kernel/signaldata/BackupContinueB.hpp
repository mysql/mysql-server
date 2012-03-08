/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef BACKUP_CONTINUEB_H
#define BACKUP_CONTINUEB_H

#include "SignalData.hpp"

class BackupContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Backup;
  friend bool printCONTINUEB_BACKUP(FILE * output, const Uint32 * theData, Uint32 len);
private:
  enum {
    START_FILE_THREAD = 0,
    BUFFER_UNDERFLOW  = 1,
    BUFFER_FULL_SCAN  = 2,
    BUFFER_FULL_FRAG_COMPLETE = 3,
    BUFFER_FULL_META  = 4,
    BACKUP_FRAGMENT_INFO = 5,
    RESET_DISK_SPEED_COUNTER = 6,
    ZDELAY_SCAN_NEXT = 7
  };
};

#endif
