/* Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDBD_HPP
#define NDBD_HPP

#define JAM_FILE_ID 218


void
ndbd_run(bool foreground, int report_fd,
         const char* connect_str, int force_nodeid, const char* bind_address,
         bool no_start, bool initial, bool initialstart,
         unsigned allocated_nodeid, int connect_retries, int connect_delay);

enum NdbShutdownType {
  NST_Normal,
  NST_Watchdog,
  NST_ErrorHandler,
  NST_ErrorHandlerSignal,
  NST_Restart,
  NST_ErrorInsert
};

enum NdbRestartType {
  NRT_Default               = 0,
  NRT_NoStart_Restart       = 1, // -n
  NRT_DoStart_Restart       = 2, //
  NRT_NoStart_InitialStart  = 3, // -n -i
  NRT_DoStart_InitialStart  = 4  // -i
};

/**
 * Shutdown/restart Ndb
 *
 * @param error_code  - The error causing shutdown/restart
 * @param type        - Type of shutdown/restart
 * @param restartType - Type of restart (only valid if type == NST_Restart)
 *
 * NOTE! never returns
 */
void
NdbShutdown(int error_code,
            NdbShutdownType type,
	    NdbRestartType restartType = NRT_Default);




#undef JAM_FILE_ID

#endif
