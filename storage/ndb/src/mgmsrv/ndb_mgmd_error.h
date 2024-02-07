/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MGMD_ERROR_H
#define NDB_MGMD_ERROR_H

#define NO_CONTACT_WITH_PROCESS 5000
#define WRONG_PROCESS_TYPE 5002
#define SEND_OR_RECEIVE_FAILED 5005
#define INVALID_ERROR_NUMBER 5007
#define INVALID_TRACE_NUMBER 5008
#define INVALID_BLOCK_NAME 5010
#define WAIT_FOR_NDBD_TO_START_SHUTDOWN_FAILED 5024
#define WAIT_FOR_NDBD_SHUTDOWN_FAILED 5025
#define NODE_SHUTDOWN_IN_PROGESS 5026
#define SYSTEM_SHUTDOWN_IN_PROGRESS 5027
#define NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH 5028
#define NO_CONTACT_WITH_DB_NODES 5030
#define UNSUPPORTED_NODE_SHUTDOWN 5031
#define NODE_NOT_API_NODE 5062
#define OPERATION_NOT_ALLOWED_START_STOP 5063

#endif
