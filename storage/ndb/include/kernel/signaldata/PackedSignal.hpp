/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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
