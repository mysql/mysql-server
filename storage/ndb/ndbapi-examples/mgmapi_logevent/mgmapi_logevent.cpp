/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <mysql.h>
#include <ndbapi/NdbApi.hpp>
#include <mgmapi.h>
#include <stdio.h>

/*
 * export LD_LIBRARY_PATH=../../../libmysql_r/.libs:../../../ndb/src/.libs
 */

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
  NdbMgmHandle h;
  NdbLogEventHandle le;
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
		   15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
		   15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
		   15, NDB_MGM_EVENT_CATEGORY_STARTUP,
		   15, NDB_MGM_EVENT_CATEGORY_ERROR,
		   0 };
  struct ndb_logevent event;

  if (argc < 2)
  {
    printf("Arguments are <connect_string cluster> [<iterations>].\n");
    exit(-1);
  }
  const char *connectstring = argv[1];
  int iterations = -1; 
  if (argc > 2)
    iterations = atoi(argv[2]);
  ndb_init();
  
  h= ndb_mgm_create_handle();
  if ( h == 0)
  {
    printf("Unable to create handle\n");
    exit(-1);
  }
  if (ndb_mgm_set_connectstring(h, connectstring) == -1)
  {
    printf("Unable to set connectstring\n");
    exit(-1);
  }
  if (ndb_mgm_connect(h,0,0,0)) MGMERROR(h);

  le= ndb_mgm_create_logevent_handle(h, filter);
  if ( le == 0 )  MGMERROR(h);

  while (iterations-- != 0)
  {
    int timeout= 1000;
    int r= ndb_logevent_get_next(le,&event,timeout);
    if (r == 0)
      printf("No event within %d milliseconds\n", timeout);
    else if (r < 0)
      LOGEVENTERROR(le)
    else
    {
      switch (event.type) {
      case NDB_LE_BackupStarted:
	printf("Node %d: BackupStarted\n", event.source_nodeid);
	printf("  Starting node ID: %d\n", event.BackupStarted.starting_node);
	printf("  Backup ID: %d\n", event.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupCompleted:
	printf("Node %d: BackupCompleted\n", event.source_nodeid);
	printf("  Backup ID: %d\n", event.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupAborted:
	printf("Node %d: BackupAborted\n", event.source_nodeid);
	break;
      case NDB_LE_BackupFailedToStart:
	printf("Node %d: BackupFailedToStart\n", event.source_nodeid);
	break;

      case NDB_LE_NodeFailCompleted:
	printf("Node %d: NodeFailCompleted\n", event.source_nodeid);
	break;
      case NDB_LE_ArbitResult:
	printf("Node %d: ArbitResult\n", event.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event.ArbitResult.code & 0xffff,
	       event.ArbitResult.arbit_node);
	break;
      case NDB_LE_DeadDueToHeartbeat:
	printf("Node %d: DeadDueToHeartbeat\n", event.source_nodeid);
	printf("  node %d\n", event.DeadDueToHeartbeat.node);
	break;

      case NDB_LE_Connected:
	printf("Node %d: Connected\n", event.source_nodeid);
	printf("  node %d\n", event.Connected.node);
	break;
      case NDB_LE_Disconnected:
	printf("Node %d: Disconnected\n", event.source_nodeid);
	printf("  node %d\n", event.Disconnected.node);
	break;
      case NDB_LE_NDBStartCompleted:
	printf("Node %d: StartCompleted\n", event.source_nodeid);
	printf("  version %d.%d.%d\n",
	       event.NDBStartCompleted.version >> 16 & 0xff,
	       event.NDBStartCompleted.version >> 8 & 0xff,
	       event.NDBStartCompleted.version >> 0 & 0xff);
	break;
      case NDB_LE_ArbitState:
	printf("Node %d: ArbitState\n", event.source_nodeid);
	printf("  code %d, arbit_node %d\n",
	       event.ArbitState.code & 0xffff,
	       event.ArbitResult.arbit_node);
	break;

      default:
	break;
      }
    }
  }
      
  ndb_mgm_destroy_logevent_handle(&le);
  ndb_mgm_destroy_handle(&h);
  ndb_end(0);
  return 0;
}
