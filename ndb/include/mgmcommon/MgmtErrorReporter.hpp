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

//******************************************************************************
// Description: This file contains the error reporting macros to be used
//  within management server.
// 
// Author: Peter Lind
//******************************************************************************


#include <ndb_global.h> // exit
#include <NdbOut.hpp>

#define REPORT_WARNING(message) \
   ndbout << "WARNING: " << message << endl

//****************************************************************************
// Description: Report a warning, the message is printed on ndbout.
// Parameters:
//  message: A text describing the warning.
// Returns: -
//****************************************************************************


#define REPORT_ERROR(message) \
   ndbout << "ERROR: " << message << endl

//****************************************************************************
// Description: Report an error, the message is printed on ndbout.
// Parameters:
//  message: A text describing the error.
// Returns: -
//****************************************************************************


#ifdef MGMT_TRACE

#define TRACE(message) \
   ndbout << "MGMT_TRACE: " << message << endl
#else 
#define TRACE(message)

#endif

//****************************************************************************
// Description: Print a message on ndbout.
// Parameters:
//  message: The message
// Returns: -
//****************************************************************************

#define MGM_REQUIRE(x) \
  if (!(x)) { ndbout << __FILE__ << " " << __LINE__ \
    << ": Warning! Requirement failed" << endl; }
