/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef PACKED_SIGNAL_HPP
#define PACKED_SIGNAL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 73


// -------- CODES FOR COMPRESSED SIGNAL (PACKED_SIGNAL) -------
#define ZCOMMIT 0
#define ZCOMPLETE 1
#define ZCOMMITTED 2
#define ZCOMPLETED 3
#define ZLQHKEYCONF 4
#define ZREMOVE_MARKER 5
#define ZFIRE_TRIG_REQ 6
#define ZFIRE_TRIG_CONF 7

// Definitions for verification of packed signals
static const int VERIFY_PACKED_SEND = 1;
#ifdef VM_TRACE
static const int VERIFY_PACKED_RECEIVE = 1;
#else
static const int VERIFY_PACKED_RECEIVE = 0;
#endif
static const int LQH_RECEIVE_TYPES = ((1 << ZCOMMIT) +
                                      (1 << ZCOMPLETE) + 
                                      (1 << ZLQHKEYCONF) +
                                      (1 << ZREMOVE_MARKER) +
                                      (1 << ZFIRE_TRIG_REQ));
static const int TC_RECEIVE_TYPES = ((1 << ZCOMMITTED) +
                                     (1 << ZCOMPLETED) +
                                     (1 << ZLQHKEYCONF) +
                                     (1 << ZFIRE_TRIG_CONF));

class PackedSignal {
public:
  static bool verify(const Uint32* data, Uint32 len, Uint32 typesExpected, Uint32 commitLen, Uint32 receiverBlockNo);

private:
  static Uint32 getSignalType(Uint32 data);

  /**
   * For printing
   */
  friend bool printPACKED_SIGNAL(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);
};

inline
Uint32 PackedSignal::getSignalType(Uint32 data) { return data >> 28; }


#undef JAM_FILE_ID

#endif
