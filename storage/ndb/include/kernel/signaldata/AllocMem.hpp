/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef ALLOC_MEM_HPP
#define ALLOC_MEM_HPP

#include "SignalData.hpp"

struct AllocMemReq
{
  STATIC_CONST( SignalLength = 3 );

  enum RequestType
  {
    RT_INIT = 0
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
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
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
};

#endif
