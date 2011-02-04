/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ERRORREPORTER_H
#define ERRORREPORTER_H

#include <ndb_global.h>
#include <ndbd_exit_codes.h>

#include "../ndbd.hpp"

class ErrorReporter
{
public:
  static void handleAssert(const char* message, 
			   const char* file, 
			   int line, int ec = NDBD_EXIT_PRGERR)
    ATTRIBUTE_NORETURN;
  
  static void handleError(int faultID, 
			  const char* problemData,
                          const char* objRef,
			  enum NdbShutdownType = NST_ErrorHandler)
    ATTRIBUTE_NORETURN;
  
  static void formatMessage(int thr_no,
                            Uint32 num_threads, int faultID,
			    const char* problemData,
                            const char* objRef, 
			    const char* theNameOfTheTraceFile,
			    char* messptr);

  static int get_trace_no();
  
  static const char* formatTimeStampString();
  
private:
  static enum NdbShutdownType s_errorHandlerShutdownType;
};

#endif
