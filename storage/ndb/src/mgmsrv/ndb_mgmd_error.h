/*
   Copyright (C) 2007 MySQL AB
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

#ifndef NDB_MGMD_ERROR_H
#define NDB_MGMD_ERROR_H

#define NO_CONTACT_WITH_PROCESS 5000
#define WRONG_PROCESS_TYPE 5002
#define SEND_OR_RECEIVE_FAILED 5005
#define INVALID_ERROR_NUMBER 5007
#define INVALID_TRACE_NUMBER 5008
#define INVALID_BLOCK_NAME 5010
#define NODE_SHUTDOWN_IN_PROGESS 5026
#define SYSTEM_SHUTDOWN_IN_PROGRESS 5027
#define NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH 5028
#define NO_CONTACT_WITH_DB_NODES 5030
#define UNSUPPORTED_NODE_SHUTDOWN 5031
#define NODE_NOT_API_NODE 5062
#define OPERATION_NOT_ALLOWED_START_STOP 5063

#endif
