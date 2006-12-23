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

#ifndef ATTRINFO_HPP
#define ATTRINFO_HPP

#include "SignalData.hpp"

class AttrInfo {
  /**
   * Sender(s)
   */
  friend class DbUtil;
  
  /**
   * Receiver(s)
   */
  friend class Dbtup;

  /**
   * Sender(s) / Receiver(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class NdbScanOperation;
  friend class Restore;

  friend bool printATTRINFO(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( HeaderLength = 3 );
  STATIC_CONST( DataLength = 22 );
  STATIC_CONST( MaxSignalLength = HeaderLength + DataLength );

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 attrData[DataLength];
};

#endif
