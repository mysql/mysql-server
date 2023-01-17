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

#ifndef ERRORHANDLINGMACROS_H
#define ERRORHANDLINGMACROS_H

#include <ndbd_exit_codes.h>
#include "ErrorReporter.hpp"

#define JAM_FILE_ID 489


#define ERROR_SET_SIGNAL(not_used, messageID, problemData, objectRef) \
        ErrorReporter::handleError(messageID, problemData, objectRef, NST_ErrorHandlerSignal)
#define ERROR_SET(not_used, messageID, problemData, objectRef) \
        ErrorReporter::handleError(messageID, problemData, objectRef)
        // Description:
        //      Call ErrorHandler with the supplied arguments. The
        //      ErrorHandler decides how to report the error.
        // Parameters:
        //      messageID       IN      Code identifying the error. If less
        //                              than 1000 a unix error is assumed. If
        //                              greater than 1000 the code is treated 
        //                              as the specific problem code.
        //      problemData     IN      A (short) text describing the error.
        //                              The context information is added to
        //                              this text.
        //      objectRef       IN      The name of the "victim" of the error.
        //                              Specify NULL if not applicable.
        // Return value:
        //      -
        // Reported errors:
        //      -
        // Additional information:
        //      -


#undef JAM_FILE_ID

#endif
