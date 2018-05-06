/*
   Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ALLOC_MEM_HPP
#define ALLOC_MEM_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 116


struct AllocMemReq
{
  STATIC_CONST( SignalLength = 5 );

  enum RequestType
  {
    RT_MAP    = 0, // Map any unmapped chunk
    RT_EXTEND = 1, // extend heap with bytes_hi/lo

    RT_MEMLOCK = 1 << 8
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
  Uint32 bytes_hi;
  Uint32 bytes_lo;
};

struct AllocMemRef
{
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
  Uint32 errorCode;
};

struct AllocMemConf
{
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
  Uint32 bytes_hi;
  Uint32 bytes_lo;
};


#undef JAM_FILE_ID

#endif
