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
  case NDBT_SKIPPED:
    return "Skipped";
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
