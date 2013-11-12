/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PGMAN_CONTINUEB_H
#define PGMAN_CONTINUEB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 179


class PgmanContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Pgman;
private:
  enum {
    STATS_LOOP = 0,
    BUSY_LOOP = 1,
    CLEANUP_LOOP = 2,
    LCP_LOOP = 3,
    LCP_LOCKED = 4
  };
};


#undef JAM_FILE_ID

#endif
