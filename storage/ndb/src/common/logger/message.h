/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not
   limited to OpenSSL) that is licensed under separate terms,
   as designated
   in a particular file or component or in included license
   documentation.
   The authors of MySQL hereby grant you an additional
   permission to link the
   program and your derivative works with the
   separately licensed software
   that they have either included with
   the program or referenced in the
   documentation.

   This program is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  To change or add messages ndbd or ndb_mgmd writes to the Windows
  error log, run
   mc.exe message.mc
  and checkin generated messages.h, messages.rc and msg000001.bin under the
  source control.
  mc.exe can be installed with Windows SDK, some Visual Studio distributions
  do not include it.
*/
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//

//
// Define the severity codes
//

//
// MessageId: MSG_EVENTLOG
//
// MessageText:
//
// %1.
//
//
//
#define MSG_EVENTLOG 0xC0000064L
