/* Copyright (C) 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef HASHMAP_IMPL_HPP
#define HASHMAP_IMPL_HPP

#include "SignalData.hpp"

struct CreateHashMapImplReq
{

  STATIC_CONST( SignalLength = 7 );

  enum RequestType {
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 objectId;
  Uint32 objectVersion;
  Uint32 buckets;
  Uint32 fragments;

  SECTION( INFO = 0 );
};

struct CreateHashMapImplConf
{

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 objectId;
  Uint32 objectVersion;
};

struct CreateHashMapImplRef
{
  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorKey;
  Uint32 errorStatus;
};

#endif
