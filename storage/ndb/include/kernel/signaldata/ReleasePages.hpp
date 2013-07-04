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

#ifndef RELEASE_PAGES_HPP
#define RELEASE_PAGES_HPP

#include "SignalData.hpp"

struct ReleasePagesReq {
  enum {
    RT_RELEASE_UNLOCKED = 1
  };
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType;
  Uint32 requestData;
};

struct ReleasePagesConf {
  STATIC_CONST( SignalLength = 2 );
  Uint32 senderData;
  Uint32 senderRef;
};

#endif
