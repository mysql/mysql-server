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

#ifndef CONTINUE_FRAGMENTED_HPP
#define CONTINUE_FRAGMENTED_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 106

class ContinueFragmented {
  /**
   * Sender/Reciver(s)
   */
  friend class SimulatedBlock;

  friend bool printCONTINUE_FRAGMENTED(FILE *, const Uint32 *, Uint32, Uint16);

 public:
 private:
  enum { CONTINUE_SENDING = 0, CONTINUE_CLEANUP = 1 };

  static constexpr Uint32 CONTINUE_CLEANUP_FIXED_WORDS = 5;

  enum {
    RES_FRAGSEND = 0, /* Fragmented send lists */
    RES_FRAGINFO = 1, /* Fragmented signal assembly hash */
    RES_LAST = 2      /* Must be last */
  };

  Uint32 type;

  union {
    Uint32 line; /* For CONTINUE_SENDING */
    struct       /* For CONTINUE_CLEANUP */
    {
      Uint32 failedNodeId;
      Uint32 resource;
      Uint32 cursor;
      Uint32 elementsCleaned;
      Uint32 callbackStart; /* Callback structure placed here */
    } cleanup;
  };
};

#undef JAM_FILE_ID

#endif
