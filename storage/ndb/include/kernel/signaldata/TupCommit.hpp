/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TUP_COMMIT_H
#define TUP_COMMIT_H

#include "SignalData.hpp"

class TupCommitReq {
  /**
   * Reciver(s)
   */
  friend class Dbtup;

  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * For printing
   */
  friend bool printTUPCOMMITREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 4 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 opPtr;
  Uint32 gci;
  Uint32 hashValue;
  Uint32 diskpage;
};

#endif
