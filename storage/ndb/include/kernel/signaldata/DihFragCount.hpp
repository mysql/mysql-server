/*
   Copyright (c) 2006, 2022, Oracle and/or its affiliates.

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
 
#ifndef DIH_FRAG_COUNT_HPP
#define DIH_FRAG_COUNT_HPP
 
#include "SignalData.hpp"

#define JAM_FILE_ID 12


/**
 * DihFragCountReq
 */
class DihFragCountReq {

public:
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 RetryInterval = 5;
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
  static constexpr Uint32 SignalLength = 5;
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
  static constexpr Uint32 SignalLength = 6;
  Uint32 m_connectionData;
  Uint32 m_tableRef;
  Uint32 m_senderData;
  Uint32 m_error;
  Uint32 m_tableStatus; // Dbdih::TabRecord::tabStatus
  Uint32 m_schemaTransId;
};


#undef JAM_FILE_ID

#endif
