/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef INCLUDE_NODE_HB_PROTOCOL_HPP
#define INCLUDE_NODE_HB_PROTOCOL_HPP

#include "SignalData.hpp"

/**
 * Request to allocate node id
 */
class InclNodeHBProtocolRep
{
public:
  STATIC_CONST( SignalLength = 1 );

  Uint32 nodeId;
};

class NdbcntrStartWaitRep
{
public:
  STATIC_CONST ( SignalLength = 1 );

  Uint32 nodeId;
};

class NdbcntrStartedRep
{
public:
  STATIC_CONST ( SignalLength = 1 );

  Uint32 nodeId;
};

class SumaHandoverCompleteRep
{
public:
  STATIC_CONST ( SignalLength = 1 );

  Uint32 nodeId;
};

class LocalRecoveryCompleteRep
{
public:
  STATIC_CONST ( SignalLengthLocal = 4 );
  STATIC_CONST ( SignalLengthMaster = 2 );

  enum PhaseIds
  {
    RESTORE_FRAG_COMPLETED = 0,
    UNDO_DD_COMPLETED = 1,
    EXECUTE_REDO_LOG_COMPLETED = 2,
    LOCAL_RECOVERY_COMPLETED = 3
  };

  Uint32 nodeId;
  Uint32 phaseId;
  Uint32 senderData;
  Uint32 instanceId;
};

#undef JAM_FILE_ID
#endif
