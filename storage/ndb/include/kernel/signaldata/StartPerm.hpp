/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef START_PERM_REQ_HPP
#define START_PERM_REQ_HPP

#define JAM_FILE_ID 204

/**
 * This signal is sent by starting DIH to master DIH
 *
 * Used when starting in an already started cluster
 *
 */
class StartPermReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 startType;
};

class StartPermConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 startingNodeId;
  Uint32 systemFailureNo;
  Uint32 microGCP;
};

class StartPermRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 startingNodeId;
  Uint32 errorCode;

  enum ErrorCode {
    ZNODE_ALREADY_STARTING_ERROR = 305,
    ZNODE_START_DISALLOWED_ERROR = 309,
    InitialStartRequired = 320
  };
};

class StartPermRep {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;

 public:
  static constexpr Uint32 SignalLength = 2;
  enum { PermissionToStart = 0, CompletedStart = 1 };

 private:
  Uint32 startNodeId;
  Uint32 reason;
};
#undef JAM_FILE_ID

#endif
