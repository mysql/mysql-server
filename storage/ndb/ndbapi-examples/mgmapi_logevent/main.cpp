/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mgmapi.h>
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <ndbapi/NdbApi.hpp>

#define MGMERROR(h)                                                    \
  {                                                                    \
    fprintf(stderr, "code: %d msg: %s\n", ndb_mgm_get_latest_error(h), \
            ndb_mgm_get_latest_error_msg(h));                          \
    exit(-1);                                                          \
  }

#define LOGEVENTERROR(h)                                                    \
  {                                                                         \
    fprintf(stderr, "code: %d msg: %s\n", ndb_logevent_get_latest_error(h), \
            ndb_logevent_get_latest_error_msg(h));                          \
    exit(-1);                                                               \
  }

#define make_uint64(a, b) (((Uint64)(a)) + (((Uint64)(b)) << 32))

int main(int argc, char **argv) {
  NdbMgmHandle h;
  NdbLogEventHandle le;
  int filter[] = {15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                  15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
                  15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
                  15, NDB_MGM_EVENT_CATEGORY_STARTUP,
                  15, NDB_MGM_EVENT_CATEGORY_ERROR,
                  0};
  struct ndb_logevent event;

  if (argc < 2) {
    printf("Arguments are <connect_string cluster> [<iterations>].\n");
    exit(-1);
  }
  const char *connectstring = argv[1];
  int iterations = -1;
  if (argc > 2) iterations = atoi(argv[2]);
  ndb_init();

  h = ndb_mgm_create_handle();
  if (h == 0) {
    printf("Unable to create handle\n");
    exit(-1);
  }
  if (ndb_mgm_set_connectstring(h, connectstring) == -1) {
    printf("Unable to set connectstring\n");
    exit(-1);
  }
  if (ndb_mgm_connect(h, 0, 0, 0)) MGMERROR(h);

  le = ndb_mgm_create_logevent_handle(h, filter);
  if (le == 0) MGMERROR(h);

  while (iterations-- != 0) {
    int timeout = 1000;
    int r = ndb_logevent_get_next(le, &event, timeout);
    if (r == 0)
      printf("No event within %d milliseconds\n", timeout);
    else if (r < 0)
      LOGEVENTERROR(le)
    else {
      switch (event.type) {
        case NDB_LE_BackupStarted:
          printf("Node %d: BackupStarted\n", event.source_nodeid);
          printf("  Starting node ID: %d\n", event.BackupStarted.starting_node);
          printf("  Backup ID: %d\n", event.BackupStarted.backup_id);
          break;
        case NDB_LE_BackupStatus:
          printf("Node %d: BackupStatus\n", event.source_nodeid);
          printf("  Starting node ID: %d\n", event.BackupStarted.starting_node);
          printf("  Backup ID: %d\n", event.BackupStarted.backup_id);
          printf("  Data written: %llu bytes (%llu records)\n",
                 make_uint64(event.BackupStatus.n_bytes_lo,
                             event.BackupStatus.n_bytes_hi),
                 make_uint64(event.BackupStatus.n_records_lo,
                             event.BackupStatus.n_records_hi));
          printf("  Log written: %llu bytes (%llu records)\n",
                 make_uint64(event.BackupStatus.n_log_bytes_lo,
                             event.BackupStatus.n_log_bytes_hi),
                 make_uint64(event.BackupStatus.n_log_records_lo,
                             event.BackupStatus.n_log_records_hi));
          break;
        case NDB_LE_BackupCompleted:
          printf("Node %d: BackupCompleted\n", event.source_nodeid);
          printf("  Backup ID: %d\n", event.BackupStarted.backup_id);
          printf("  Data written: %llu bytes (%llu records)\n",
                 make_uint64(event.BackupCompleted.n_bytes,
                             event.BackupCompleted.n_bytes_hi),
                 make_uint64(event.BackupCompleted.n_records,
                             event.BackupCompleted.n_records_hi));
          printf("  Log written: %llu bytes (%llu records)\n",
                 make_uint64(event.BackupCompleted.n_log_bytes,
                             event.BackupCompleted.n_log_bytes_hi),
                 make_uint64(event.BackupCompleted.n_log_records,
                             event.BackupCompleted.n_log_records_hi));
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
          printf("  code %d, arbit_node %d\n", event.ArbitResult.code & 0xffff,
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
          printf("  code %d, arbit_node %d\n", event.ArbitState.code & 0xffff,
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
