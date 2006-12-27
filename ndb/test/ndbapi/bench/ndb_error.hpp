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

#ifndef NDB_ERROR_H
#define NDB_ERROR_H

#include <ndb_global.h>
#include <NdbOut.hpp>
#include "userInterface.h"
#include <NdbError.hpp>
#include <NdbApi.hpp>

#define error_handler(x,y, z) { \
   ndbout << x << " " << y << endl; \
   exit(-1); }

#define CHECK_MINUS_ONE(x, y, z) if(x == -1) \
   error_handler(y,(z->getNdbError()), 0)
  
inline
void
CHECK_ALLOWED_ERROR(const char * str, 
		    const ThreadData * td, 
		    const struct NdbError & error){
  
  char buf[100];
  snprintf(buf, sizeof(buf), "subscriber = %.*s ", 
	  SUBSCRIBER_NUMBER_LENGTH, 
	  td->transactionData.number);
  ndbout << str << " " << error << endl
	 << buf;
  showTime();
  
  switch(error.classification) { 
  case NdbError::TimeoutExpired:  
  case NdbError::OverloadError: 
  case NdbError::TemporaryResourceError: 
  case NdbError::NodeRecoveryError:
    break;    
  default:    
    if(error.status != NdbError::TemporaryError)
      exit(-1);
  }
}

inline
void
CHECK_NULL(void * null, 
	   const char * str, 
	   const ThreadData * td,
	   const struct NdbError & err){
  if(null == 0){
    CHECK_ALLOWED_ERROR(str, td, err);
    exit(-1);
  }
}

inline
void
CHECK_NULL(void * null, const char* msg, NdbConnection* obj)
{
  if(null == 0)
  {
    error_handler(msg, obj->getNdbError(), 0);
  }
}

#endif
