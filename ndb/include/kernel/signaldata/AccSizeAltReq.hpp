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

#ifndef ACC_SIZE_ALT_REQ_H
#define ACC_SIZE_ALT_REQ_H

#include "SignalData.hpp"

class AccSizeAltReq  {
  /**
   * Sender(s)
   */
  friend class ClusterConfiguration;

  /**
   * Reciver(s)
   */
  friend class Dbacc;
private:
  /**
   * Indexes in theData
   */
  STATIC_CONST( IND_BLOCK_REF     = 0 );
  STATIC_CONST( IND_DIR_RANGE     = 1 );
  STATIC_CONST( IND_DIR_ARRAY     = 2 );
  STATIC_CONST( IND_FRAGMENT      = 3 );
  STATIC_CONST( IND_OP_RECS       = 4 );
  STATIC_CONST( IND_OVERFLOW_RECS = 5 );
  STATIC_CONST( IND_PAGE8         = 6 );
  STATIC_CONST( IND_ROOT_FRAG     = 7 );
  STATIC_CONST( IND_TABLE         = 8 );
  STATIC_CONST( IND_SCAN          = 9 );
  
  /**
   * Use the index definitions to use the signal data
   */
  UintR theData[10];
};

#endif
