/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef SIGNAL_DATA_H
#define SIGNAL_DATA_H

#include <ndb_global.h>
#include <ndb_limits.h>
#include <kernel_types.h>

#define ASSERT_BOOL(flag, message) assert(flag<=1)
#define ASSERT_RANGE(value, min, max, message) \
 assert((value) >= (min) && (value) <= (max))
#define ASSERT_MAX(value, max, message) assert((value) <= (max))

#define SECTION(x) STATIC_CONST(x)

// defines for setter and getters on commonly used member data in signals

#define GET_SET_SENDERDATA \
  Uint32 getSenderData() { return senderData; }; \
  void setSenderData(Uint32 _s) { senderData = _s; };

#define GET_SET_SENDERREF \
  Uint32 getSenderRef() { return senderRef; }; \
  void setSenderRef(Uint32 _s) { senderRef = _s; };

#define GET_SET_PREPAREID \
  Uint32 getPrepareId() { return prepareId; }; \
  void setPrepareId(Uint32 _s) { prepareId = _s; };

#define GET_SET_ERRORCODE \
  Uint32 getErrorCode() { return errorCode; }; \
  void setErrorCode(Uint32 _s) { errorCode = _s; };

#define GET_SET_TCERRORCODE \
  Uint32 getTCErrorCode() { return TCErrorCode; }; \
  void setTCErrorCode(Uint32 _s) { TCErrorCode = _s; };

#endif
