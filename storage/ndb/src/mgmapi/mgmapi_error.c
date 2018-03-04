/*
   Copyright 2008, 2009 Sun Microsystems, Inc.
    Use is subject to license terms.

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

#include <mgmapi_error.h>

const struct Ndb_Mgm_Error_Msg ndb_mgm_error_msgs[] = {
  { NDB_MGM_NO_ERROR, "No error" },
  
  /* Request for service errors */
  { NDB_MGM_ILLEGAL_CONNECT_STRING, "Illegal connect string" },
  { NDB_MGM_ILLEGAL_SERVER_HANDLE, "Illegal server handle" },
  { NDB_MGM_ILLEGAL_SERVER_REPLY, "Illegal reply from server" },
  { NDB_MGM_ILLEGAL_NUMBER_OF_NODES, "Illegal number of nodes" },
  { NDB_MGM_ILLEGAL_NODE_STATUS, "Illegal node status" },
  { NDB_MGM_OUT_OF_MEMORY, "Out of memory" },
  { NDB_MGM_SERVER_NOT_CONNECTED, "Management server not connected" },
  { NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, "Could not connect to socket" },
  
  /* Service errors - Start/Stop Node or System */
  { NDB_MGM_START_FAILED, "Start failed" },
  { NDB_MGM_STOP_FAILED, "Stop failed" },
  { NDB_MGM_RESTART_FAILED, "Restart failed" },
  
  /* Service errors - Backup */
  { NDB_MGM_COULD_NOT_START_BACKUP, "Could not start backup" },
  { NDB_MGM_COULD_NOT_ABORT_BACKUP, "Could not abort backup" },
  
  /* Service errors - Single User Mode */
  { NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE,
    "Could not enter single user mode" },
  { NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE,
    "Could not exit single user mode" },
  
  /* Service errors - Configuration change */
  { NDB_MGM_CONFIG_CHANGE_FAILED,
    "Failed to complete configuration change" },
  { NDB_MGM_GET_CONFIG_FAILED,
    "Failed to get configuration" },

  /* Usage errors */
  { NDB_MGM_USAGE_ERROR,
    "Usage error" }
};

const int ndb_mgm_noOfErrorMsgs =
  sizeof(ndb_mgm_error_msgs)/sizeof(struct Ndb_Mgm_Error_Msg);
