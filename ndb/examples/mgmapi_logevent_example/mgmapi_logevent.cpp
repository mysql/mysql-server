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

int main()
{
  NdbMgmHandle h;
  NdbLogEventHandle l;
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0 };
  struct ndb_logevent event;

  ndb_init();

  h= ndb_mgm_create_handle();
  if ( h == 0)
  {
    printf("Unable to create handle\n");
    exit(-1);
  }
  if (ndb_mgm_connect(h,0,0,0)) MGMERROR(h);

  l= ndb_mgm_create_logevent_handle(h, filter);
  if ( l == 0 )  MGMERROR(h);

  while (1)
  {
    int timeout= 5000;
    int r= ndb_logevent_get_next(l,&event,timeout);
    if (r == 0)
      printf("No event within %d milliseconds\n", timeout);
    else if (r < 0)
      LOGEVENTERROR(l)
    else
    {
      printf("Event %d from node ID %d\n",
	     event.type,
	     event.source_nodeid);
      printf("Category %d, severity %d, level %d\n",
	     event.category,
	     event.severity,
	     event.level);
      switch (event.type) {
      case NDB_LE_BackupStarted:
	printf("BackupStartded\n");
	printf("Starting node ID: %d\n", event.BackupStarted.starting_node);
	printf("Backup ID: %d\n", event.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupCompleted:
	printf("BackupCompleted\n");
	printf("Backup ID: %d\n", event.BackupStarted.backup_id);
	break;
      case NDB_LE_BackupAborted:
	break;
      case NDB_LE_BackupFailedToStart:
	break;
      default:
	printf("Unexpected event\n");
	break;
      }
    }
  }
      
  ndb_mgm_destroy_logevent_handle(&l);
  ndb_mgm_destroy_handle(&h);
  ndb_end(0);
  return 0;
}
