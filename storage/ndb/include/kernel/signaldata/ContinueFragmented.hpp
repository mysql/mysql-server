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

#ifndef CONTINUE_FRAGMENTED_HPP
#define CONTINUE_FRAGMENTED_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 106


class ContinueFragmented {
  
  /**
   * Sender/Reciver(s)
   */
  friend class SimulatedBlock;
  
  friend bool printCONTINUE_FRAGMENTED(FILE *,const Uint32 *, Uint32, Uint16);
public:
  
private:
  enum {
    CONTINUE_SENDING = 0,
    CONTINUE_CLEANUP = 1
  };
  
  STATIC_CONST(CONTINUE_CLEANUP_FIXED_WORDS = 5);

  enum {
    RES_FRAGSEND = 0, /* Fragmented send lists */
    RES_FRAGINFO = 1, /* Fragmented signal assembly hash */
    RES_LAST = 2      /* Must be last */
  };

  Uint32 type;
  
  union
  {
    Uint32 line;  /* For CONTINUE_SENDING */
    struct        /* For CONTINUE_CLEANUP */
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
