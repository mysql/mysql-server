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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ENABLE_COM_H
#define ENABLE_COM_H

#include "SignalData.hpp"

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

#endif
