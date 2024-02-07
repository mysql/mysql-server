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

#ifndef START_ME_HPP
#define START_ME_HPP

#define JAM_FILE_ID 152

/**
 * This signal is sent...
 *
 * It also contains the Sysfile.
 * Since the Sysfile can be larger than on StartMeConf signal,
 *   there might be more than on of these signals sent before
 *   the entire sysfile is transferred
 *
 */
class StartMeReq {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 startingRef;
  Uint32 startingVersion;
};

class StartMeConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 SignalLength_v1 = 25;
  static constexpr Uint32 SignalLength_v2 = 2;

 private:
  Uint32 startingNodeId;
  Uint32 startWord;

  /**
   * No of free words to carry data
   */
  static constexpr Uint32 DATA_SIZE = 23;

  Uint32 data[DATA_SIZE];
};

#undef JAM_FILE_ID

#endif
