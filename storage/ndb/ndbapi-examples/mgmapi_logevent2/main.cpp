/*
   Copyright (c) 2006, 2022, Oracle and/or its affiliates.
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

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <ndbapi/NdbApi.hpp>
#include <mgmapi.h>
#include <stdio.h>
#include <stdlib.h>

#define MGMERROR(h) \
{ \
  fprintf(stderr, "code: %d msg: %s\n", \
          ndb_mgm_get_latest_error(h), \
          ndb_mgm_get_latest_error_msg(h)); \
  exit(-1); \
}

#define LOGEVENTERROR(h) \
{ \
  fprintf(stderr, "code: %d msg: %s\n", \
          ndb_logevent_get_latest_error(h), \
          ndb_logevent_get_latest_error_msg(h)); \
  exit(-1); \
}

int main(int argc, char** argv)
{
  NdbMgmHandle h1,h2;
  NdbLogEventHandle le1,le2;
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
		   15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
		   15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
		   15, NDB_MGM_EVENT_CATEGORY_STARTUP,
		   15, NDB_MGM_EVENT_CATEGORY_ERROR,
		   0 };
  struct ndb_logevent event1, event2;

  if (argc < 3)
  {
    printf("Arguments are <connect_string cluster 1> <connect_string cluster 2> [<iterations>].\n");
    exit(-1);
  }
  const char *connectstring1 = argv[1];
  const char *connectstring2 = argv[2];
  int iterations = -1; 
  if (argc > 3)
    iterations = atoi(argv[3]);
  ndb_init();
  
  h1= ndb_mgm_create_handle();
  h2= ndb_mgm_create_handle();
  if ( h1 == 0 || h2 == 0 )
  {
    printf("Unable to create handle\n");
    exit(-1);
  }
  if (ndb_mgm_set_connectstring(h1, connectstring1) == -1 ||
      ndb_mgm_set_connectstring(h2, connectstring2))
  {
    printf("Unable to set connectstring\n");
    exit(-1);
  }
  if (ndb_mgm_connect(h1,0,0,0)) MGMERROR(h1);
  if (ndb_mgm_connect(h2,0,0,0)) MGMERROR(h2);

  if ((le1= ndb_mgm_create_logevent_handle(h1, filter)) == 0) MGMERROR(h1);
  if ((le2= ndb_mgm_create_logevent_handle(h1, filter)) == 0) MGMERROR(h2);

  while (iterations-- != 0)
  {
    int timeout= 1000;
    int r1= ndb_logevent_get_next(le1,&event1,timeout);
    if (r1 == 0)
      printf("No event within %d milliseconds\n", timeout);
    else if (r1 < 0)
      LOGEVENTERROR(le1)
    else
    {
      switch (event1.type) {
      case NDB_LE_BackupStarted:
	printf("Node %d: BackupStarted\n", event1.source_nodeid);
	printf("  Starting node ID: %d\n", event1.BackupStarted.starting_node);
	printf("  Backup ID: %d\n", event1.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupCompleted:
	printf("Node %d: BackupCompleted\n", event1.source_nodeid);
	printf("  Backup ID: %d\n", event1.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupAborted:
	printf("Node %d: BackupAborted\n", event1.source_nodeid);
	break;
      case NDB_LE_BackupFailedToStart:
	printf("Node %d: BackupFailedToStart\n", event1.source_nodeid);
	break;

      case NDB_LE_NodeFailCompleted:
	printf("Node %d: NodeFailCompleted\n", event1.source_nodeid);
	break;
      case NDB_LE_ArbitResult:
	printf("Node %d: ArbitResult\n", event1.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event1.ArbitResult.code & 0xffff,
	       event1.ArbitResult.arbit_node);
	break;
      case NDB_LE_DeadDueToHeartbeat:
	printf("Node %d: DeadDueToHeartbeat\n", event1.source_nodeid);
	printf("  node %d\n", event1.DeadDueToHeartbeat.node);
	break;

      case NDB_LE_Connected:
	printf("Node %d: Connected\n", event1.source_nodeid);
	printf("  node %d\n", event1.Connected.node);
	break;
      case NDB_LE_Disconnected:
	printf("Node %d: Disconnected\n", event1.source_nodeid);
	printf("  node %d\n", event1.Disconnected.node);
	break;
      case NDB_LE_NDBStartCompleted:
	printf("Node %d: StartCompleted\n", event1.source_nodeid);
	printf("  version %d.%d.%d\n",
	       event1.NDBStartCompleted.version >> 16 & 0xff,
	       event1.NDBStartCompleted.version >> 8 & 0xff,
	       event1.NDBStartCompleted.version >> 0 & 0xff);
	break;
      case NDB_LE_ArbitState:
	printf("Node %d: ArbitState\n", event1.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event1.ArbitState.code & 0xffff,
	       event1.ArbitResult.arbit_node);
	break;

      default:
	break;
      }
    }

    int r2= ndb_logevent_get_next(le1,&event2,timeout);
    if (r2 == 0)
      printf("No event within %d milliseconds\n", timeout);
    else if (r2 < 0)
      LOGEVENTERROR(le2)
    else
    {
      switch (event2.type) {
      case NDB_LE_BackupStarted:
	printf("Node %d: BackupStarted\n", event2.source_nodeid);
	printf("  Starting node ID: %d\n", event2.BackupStarted.starting_node);
	printf("  Backup ID: %d\n", event2.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupCompleted:
	printf("Node %d: BackupCompleted\n", event2.source_nodeid);
	printf("  Backup ID: %d\n", event2.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupAborted:
	printf("Node %d: BackupAborted\n", event2.source_nodeid);
	break;
      case NDB_LE_BackupFailedToStart:
	printf("Node %d: BackupFailedToStart\n", event2.source_nodeid);
	break;

      case NDB_LE_NodeFailCompleted:
	printf("Node %d: NodeFailCompleted\n", event2.source_nodeid);
	break;
      case NDB_LE_ArbitResult:
	printf("Node %d: ArbitResult\n", event2.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event2.ArbitResult.code & 0xffff,
	       event2.ArbitResult.arbit_node);
	break;
      case NDB_LE_DeadDueToHeartbeat:
	printf("Node %d: DeadDueToHeartbeat\n", event2.source_nodeid);
	printf("  node %d\n", event2.DeadDueToHeartbeat.node);
	break;

      case NDB_LE_Connected:
	printf("Node %d: Connected\n", event2.source_nodeid);
	printf("  node %d\n", event2.Connected.node);
	break;
      case NDB_LE_Disconnected:
	printf("Node %d: Disconnected\n", event2.source_nodeid);
	printf("  node %d\n", event2.Disconnected.node);
	break;
      case NDB_LE_NDBStartCompleted:
	printf("Node %d: StartCompleted\n", event2.source_nodeid);
	printf("  version %d.%d.%d\n",
	       event2.NDBStartCompleted.version >> 16 & 0xff,
	       event2.NDBStartCompleted.version >> 8 & 0xff,
	       event2.NDBStartCompleted.version >> 0 & 0xff);
	break;
      case NDB_LE_ArbitState:
	printf("Node %d: ArbitState\n", event2.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event2.ArbitState.code & 0xffff,
	       event2.ArbitResult.arbit_node);
	break;

      default:
	break;
      }
    }
  }
      
  ndb_mgm_destroy_logevent_handle(&le1);
  ndb_mgm_destroy_logevent_handle(&le2);
  ndb_mgm_destroy_handle(&h1);
  ndb_mgm_destroy_handle(&h2);
  ndb_end(0);
  return 0;
}
