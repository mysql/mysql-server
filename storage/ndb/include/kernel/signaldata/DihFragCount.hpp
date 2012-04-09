/*
   Copyright (C) 2006, 2007 MySQL AB
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
 
#ifndef DIH_FRAG_COUNT_HPP
#define DIH_FRAG_COUNT_HPP
 
#include "SignalData.hpp"

/**
 * DihFragCountReq
 */
class DihFragCountReq {

public:
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( RetryInterval = 5 );
  Uint32 m_connectionData;
  Uint32 m_tableRef;
  Uint32 m_senderData;
  Uint32 m_schemaTransId;
};

/**
 * DihFragCountConf
 */
class DihFragCountConf {

public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 m_connectionData;
  Uint32 m_tableRef;
  Uint32 m_senderData;
  Uint32 m_fragmentCount;
  Uint32 m_noOfBackups;
};

/**
 * DihFragCountRef
 */
class DihFragCountRef {

public:
  enum ErrorCode {
    ErroneousState = 0,
    ErroneousTableState = 1
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 m_connectionData;
  Uint32 m_tableRef;
  Uint32 m_senderData;
  Uint32 m_error;
  Uint32 m_tableStatus; // Dbdih::TabRecord::tabStatus
  Uint32 m_schemaTransId;
};

#endif
