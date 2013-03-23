/* Copyright 2008 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef CALLBACK_SIGNAL_HPP
#define CALLBACK_SIGNAL_HPP

#include "SignalData.hpp"

struct CallbackConf {
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 callbackIndex;
  Uint32 callbackData;
  Uint32 returnCode;
};

struct CallbackAck {
  STATIC_CONST( SignalLength = 1 );
  Uint32 senderData;
};

#endif
