/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef UNDO_LOG_LEVEL_HPP
#define UNDO_LOG_LEVEL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 499


class UndoLogLevelRep {
  /**
   * Sender(s)
   */
  friend class Lgman;
  friend class Ndbcntr;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

public:
  STATIC_CONST( SignalLength = 1 );

private:

  Uint32 levelUsed; // in percent
};

#undef JAM_FILE_ID

#endif
