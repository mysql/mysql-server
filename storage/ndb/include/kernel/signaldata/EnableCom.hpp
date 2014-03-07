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

#ifndef ENABLE_COM_H
#define ENABLE_COM_H

#include "SignalData.hpp"

#define JAM_FILE_ID 40


class EnableComReq  {
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;

public:
  STATIC_CONST( SignalLength = 2 + NodeBitmask::Size );
private:

  Uint32 m_senderRef;
  Uint32 m_senderData;
  Uint32 m_nodeIds[NodeBitmask::Size];
};

class EnableComConf  {
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;
  friend class Cmvmi;

public:
  STATIC_CONST( SignalLength = 2 + NodeBitmask::Size );
private:

  Uint32 m_senderRef;
  Uint32 m_senderData;
  Uint32 m_nodeIds[NodeBitmask::Size];
};


#undef JAM_FILE_ID

#endif
