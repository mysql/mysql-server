/*
   Copyright (C) 2003, 2005-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef NDBT_Error_HPP
#define NDBT_Error_HPP

#include <NdbOut.hpp>
#include <NdbError.hpp>

/**
 * NDBT_Error.hpp 
 * This is the main include file about error handling in NDBT test programs 
 *
 */
class ErrorData {

public:
  ErrorData();
  ~ErrorData();

  /**
   * Parse cmd line arg
   *
   * Return true if successeful
   */
  bool parseCmdLineArg(const char ** argv, int & i);
  
  /**
   * Print cmd line arguments
   */
  void printCmdLineArgs(NdbOut & out = ndbout);

  /**
   * Print settings
   */
  void printSettings(NdbOut & out = ndbout);
  
  /**
   * Print error count
   */
  void printErrorCounters(NdbOut & out = ndbout) const;
  
  /**
   * Reset error counters
   */
  void resetErrorCounters();
  
  /**
   * 
   */
  int handleErrorCommon(const NdbError & error);
  
private:
  bool key_error;
  bool temporary_resource_error;
  bool insufficient_space_error;
  bool node_recovery_error;
  bool overload_error;
  bool timeout_error;
  bool internal_error;
  bool user_error;
  bool application_error;
  
  Uint32 * errorCountArray;
};

//
//  ERR prints an NdbError object together with a description of where the
//  error occured
//
#define ERR_OUT(where, error) \
  {  where << "ERROR: " << error.code << " " \
           << error.message << endl \
           << "           " << "Status: " << error.status \
           << ", Classification: " << error.classification << endl\
           << "           " << "File: " << __FILE__ \
           << " (Line: " << __LINE__ << ")" << endl \
	   ; \
  }

#define ERR(error) \
{ \
  const NdbError &_error= (error); \
  ERR_OUT(g_err, _error); \
}
#define ERR_INFO(error) ERR_OUT(g_info, error)

#endif
