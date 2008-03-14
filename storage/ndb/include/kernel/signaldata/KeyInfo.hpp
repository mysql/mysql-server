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

#ifndef KEY_INFO_HPP
#define KEY_INFO_HPP

#include "SignalData.hpp"

class KeyInfo {
  /**
   * Sender(s)
   */
  friend class DbUtil;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class Restore;

  /**
   * Reciver(s)
   */
  friend class Dbtc;
  
public:
  STATIC_CONST( HeaderLength = 3 );
  STATIC_CONST( DataLength = 20 );
  STATIC_CONST( MaxSignalLength = HeaderLength + DataLength );

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 keyData[DataLength];
};

/*
  The KEYINFO signal is used to send a stream of data defining keys for
  primary key operations (TCKEYREQ) or ordered index scan bounds (SCAN_TABREQ).

  For TCKEYREQ, the first 8 words of the KEYINFO stream are actually stored
  inside the TCKEYREQ signal, so for shorter keys, no KEYINFO signals are
  needed. Otherwise as many consecutive KEYINFO signals as needed are sent with
  max KeyInfo::Datalength words of data in each.

  For scan bounds for ordered indexes, the data sent consists of a sequence of
  entries, each (2+N) words:
    1 word of bound type (0:<= 1:< 2:>= 3:> 4:==)
    1 word of AttributeHeader (containing attribute Id and byte length)
    N words of attribute data (N = (length+3)>>2).
  Additionally, it is possible to send multiple range bounds in a single
  SCAN_TABREQ and associated ATTRINFO stream (using
  NdbIndexScanOperation::end_of_bound(RANGE_NO) ). In this case, the first word
  of each range bound contains additional information: bits 16-31 holds the
  length of this bound, in words of ATTRINFO data, and bits 4-15 holds a
  number RANGE_NO specified by the application that can be read back from the
  RANGE_NO pseudo-column.

*/
#endif
