/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef ENABLE_COM_H
#define ENABLE_COM_H

#include "SignalData.hpp"

#define JAM_FILE_ID 40

// Only local async signal, not global.

class EnableComReq {
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 m_senderRef;
  Uint32 m_senderData;
  Uint32 m_enableNodeId;
  NodeBitmask m_nodeIds;  // Not part of signal, but first section
};

class EnableComConf {
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;
  friend class Cmvmi;

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 m_senderRef;
  Uint32 m_senderData;
  Uint32 m_enableNodeId;
};

#undef JAM_FILE_ID

#endif
