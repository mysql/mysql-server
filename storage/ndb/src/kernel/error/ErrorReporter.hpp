/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ERRORREPORTER_H
#define ERRORREPORTER_H

#include <ndb_global.h>
#include <ndbd_exit_codes.h>

#include "../ndbd.hpp"

#define JAM_FILE_ID 487


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

  static void prepare_to_crash(bool first_phase, bool error_insert_crash);
private:
  static enum NdbShutdownType s_errorHandlerShutdownType;
};


#undef JAM_FILE_ID

#endif
