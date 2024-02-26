/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef ATTRINFO_HPP
#define ATTRINFO_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 185


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
  friend class NdbOperation;

  friend bool printATTRINFO(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  static constexpr Uint32 HeaderLength = 3;
  static constexpr Uint32 DataLength = 22;
  static constexpr Uint32 MaxSignalLength = HeaderLength + DataLength;
  static constexpr Uint32 SectionSizeInfoLength = 5;

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 attrData[DataLength];
};

/*
  A train of ATTRINFO signals is used to specify attributes to read or
  attributes and values to insert/update in TCKEYREQ, and to specify
  attributes to read in SCAN_TABREQ.

  The ATTRINFO signal train defines a stream of attribute info words.  (Note
  that for TCKEYREQ, the first five words are stored inside the TCKEYREQ
  signal. For SCAN_TABREQ, all attribute info words are sent in ATTRINFO
  signals).

  For SCAN_TABREQ, and TCKEYREQ for read or update with interpreted code, 
  the attribute information can have up to five sections. The initial 
  five words of the stream defines the length of the sections, followed 
  by the words of each section in sequence.

  The sections are:
   1. Attributes to read before starting any interpreted program.
   2. Interpreted program.
   3. Attributes to update after running interpreted program.
   4. Attributes to read after interpreted program.
   5. Subroutine data.

  The formats of sections that specify attributes to read or update is a
  sequence of entries, each (1+N) words:
    1 word specifying the AttributeHeader (attribute id in upper 16 bits, and
           size in bytes of data in lower 16 bits).
    N words of data (N = (data byte length+3)>>2).
  For specifying attributes to read, the data length is always zero.
  For an index range scan of a table using an ordered index, the attribute IDs
  refer to columns in the underlying table, not to columns being indexed, so
  all attributes in the underlying table being indexed are accessible.
*/


#undef JAM_FILE_ID

#endif
