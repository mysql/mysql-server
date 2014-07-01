/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CALLBACK_SIGNAL_HPP
#define CALLBACK_SIGNAL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 22


/*
 * Callbacks via signals.  The "Req" is done by a method call
 * so there is only "Conf" and optional "Ack".
 */

struct CallbackConf {
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderData;    // callee: e.g. lgman logfile_group_id
  Uint32 senderRef;     // callee
  Uint32 callbackIndex; // caller: index into own CallbackTable passed in Req
  Uint32 callbackData;  // caller: e.g. dbtup opPtr.i passed in Req
  Uint32 callbackInfo;  // callee: anything, returned in CallbackAck
  Uint32 returnCode;    // callee
};

struct CallbackAck {
  STATIC_CONST( SignalLength = 2 );
  Uint32 senderData;    // from CallbackConf
  Uint32 callbackInfo;  // from CallbackConf
};


#undef JAM_FILE_ID

#endif
