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

#ifndef TRANSID_AI_HPP
#define TRANSID_AI_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 192


class TransIdAI {
  /**
   * Sender(s)
   */
  friend class Dbtup;
  
  /**
   * Receiver(s)
   */
  friend class NdbTransaction;
  friend class Dbtc;
  friend class Dbutil;
  friend class Dblqh;
  friend class Suma;

  friend bool printTRANSID_AI(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( HeaderLength = 3 );
  STATIC_CONST( DataLength = 22 );

  // Public methods
public:
 Uint32* getData() const;

public:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 attrData[DataLength];
};

inline
Uint32* TransIdAI::getData() const
{
  return (Uint32*)&attrData[0];
}


#undef JAM_FILE_ID

#endif
