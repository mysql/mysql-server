/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KEY_INFO_HPP
#define KEY_INFO_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 162


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
  
  /* IndexBound constants */
  STATIC_CONST( PerBoundColumnOverhead = 2 );
  /* Max number of key columns with max total key size */
  STATIC_CONST( MaxWordsPerBoundRow =   \
                (PerBoundColumnOverhead * MAX_ATTRIBUTES_IN_INDEX)    \
                + MAX_KEY_SIZE_IN_WORDS );
  /* Single key column with max total key size */
  STATIC_CONST( MaxWordsPerBoundColumn =     \
                PerBoundColumnOverhead + MAX_KEY_SIZE_IN_WORDS );

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
  SCAN_TABREQ and associated KEYINFO stream (using NdbRecord Index scans and
  multiple calls to setBound with different range numbers). In this case, the 
  first word of each range bound contains additional information: 
  bits 16-31 holds the length of this bound, in words of KEYINFO data,
  and bits 4-15 holds a number RANGE_NO specified by the application that 
  can be read back from the RANGE_NO pseudo-column.

*/

#undef JAM_FILE_ID

#endif
