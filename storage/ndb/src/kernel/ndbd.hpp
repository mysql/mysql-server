/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef NDBD_HPP
#define NDBD_HPP

void
ndbd_run(bool foreground, int report_fd,
         const char* connect_str, int force_nodeid, const char* bind_address,
         bool no_start, bool initial, bool initialstart,
         unsigned allocated_nodeid);

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



#endif
