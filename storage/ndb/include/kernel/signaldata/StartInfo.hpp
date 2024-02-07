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

#ifndef START_INFO_HPP
#define START_INFO_HPP

#define JAM_FILE_ID 9

/**
 * This signal is sent from the master DIH to all DIHs
 * when a node is starting.
 * If the typeStart is initial node restart then the node
 * has started without filesystem.
 * All DIHs must then "forget" that the starting node has
 * performed LCP's ever.
 *
 * @see StartPermReq
 */

class StartInfoReq {
  /**
   * Sender/Receiver
   */
  friend class Dbdih;

  Uint32 startingNodeId;
  Uint32 typeStart;
  Uint32 systemFailureNo;

 public:
  static constexpr Uint32 SignalLength = 3;
};

class StartInfoConf {
  /**
   * Sender/Receiver
   */
  friend class Dbdih;

  /**
   * NodeId of sending node
   * which is "done"
   */
  Uint32 sendingNodeId;
  Uint32 startingNodeId;

 public:
  static constexpr Uint32 SignalLength = 2;
};

class StartInfoRef {
  /**
   * Sender/Receiver
   */
  friend class Dbdih;

  /**
   * NodeId of sending node
   * The node was refused to start. This could be
   * because there are still processes handling
   * previous information from the starting node.
   */
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 errorCode;

 public:
  static constexpr Uint32 SignalLength = 3;
};

#undef JAM_FILE_ID

#endif
