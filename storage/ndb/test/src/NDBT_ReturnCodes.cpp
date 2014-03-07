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

/* System include files */
#include <ndb_global.h>

#include "NDBT_ReturnCodes.h"

/* Ndb include files */
#include <NdbOut.hpp>

static
const char* rcodeToChar(int rcode){
  switch (rcode){
  case NDBT_OK:
    return "OK";
    break;
  case NDBT_FAILED:
    return "Failed";
    break;
  case NDBT_WRONGARGS:
    return "Wrong arguments";
    break;
  case NDBT_TEMPORARY:
    return "Temporary error";
    break;
    
  default:
    return "Unknown";
    break;
  }
}

int NDBT_ProgramExit(int rcode){
   ndbout_c("\nNDBT_ProgramExit: %d - %s\n", rcode, rcodeToChar(rcode));
   //   exit(rcode);
   return rcode;
}
