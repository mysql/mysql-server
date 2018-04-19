/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
