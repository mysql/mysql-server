/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include "dba_internal.hpp"

static DBA_Error_t latestError = DBA_NO_ERROR;
static DBA_ErrorCode_t latestNdbError = 0;
static char latestMsg[1024];

/**
 * Private 
 */
void
DBA__SetLatestError(DBA_Error_t le, 
		    DBA_ErrorCode_t lnb, 
		    const char * msg, ...){

  require(msg != 0);

  latestError = le;
  latestNdbError = lnb;
  
  va_list ap;
  
  va_start(ap, msg);
  vsnprintf(latestMsg, sizeof(latestMsg)-1, msg, ap);
  va_end(ap);
}

/**
 * Get latest DBA error
 */
extern "C"
DBA_Error_t
DBA_GetLatestError(){
  return latestError;
}

/**
 * Get latest error string associated with GetLatestError
 *
 * String must not be free by caller of this method
 */
extern "C"
const char *
DBA_GetLatestErrorMsg(){
  return latestMsg;
}

/**
 * Get the latest NDB error
 *
 * Note only applicable to synchronous methods
 */
extern "C"
DBA_ErrorCode_t
DBA_GetLatestNdbError(){
  return latestNdbError;
}

extern "C"
const
char *
DBA_GetNdbErrorMsg(DBA_ErrorCode_t code){
  return DBA__TheNdb->getNdbError(code).message;
}

struct DBA_ErrorTxtMap {
  DBA_Error_t Error;
  const char * Msg;
};

static
const DBA_ErrorTxtMap errMap[] = {
  { DBA_NO_ERROR, "No error" },
  { DBA_NOT_IMPLEMENTED, "Function Not Implemented" },
  { DBA_NDB_ERROR, "Uncategorised NDB error" },
  { DBA_ERROR, "Uncategorised DBA implementation error" },
  { DBA_APPLICATION_ERROR,
    "Function called with invalid argument(s)/invalid sequence(s)" },
  { DBA_NO_DATA, "No row with specified PK existed" },
  { DBA_CONSTRAINT_VIOLATION, "There already existed a row with that PK" },
  
  { DBA_TEMPORARY_ERROR, "Request failed due to temporary reasons" },
  { DBA_INSUFFICIENT_SPACE,
    "The DB is full" },
  { DBA_OVERLOAD, "Request was rejected in NDB due to high load situation" },
  { DBA_TIMEOUT, "The request timed out, probably due to dead-lock" }
};

static const int ErrMsgs = sizeof(errMap)/sizeof(DBA_ErrorTxtMap);

extern "C"
const
char *
DBA_GetErrorMsg(DBA_Error_t e){
  for(int i = 0; i<ErrMsgs; i++)
    if(errMap[i].Error == e)
      return errMap[i].Msg;
  return "Invalid error code";
}

