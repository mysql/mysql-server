/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* System include files */
#include <ndb_global.h>

#include "NDBT_ReturnCodes.h"

/* Ndb include files */
#include <NdbOut.hpp>

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
