/*
   Copyright (C) 2005, 2006, 2008 MySQL AB, 2008 Sun Microsystems, Inc.
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

#include <ndb_global.h>
#include <ndb_opts.h>

#include "userInterface.h"
#include "dbPopulate.h"
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <random.h>
#include <NDBT.hpp>

int useTableLogging;
int subscriberCount;

static 
void usage(const char *prog)
{
  
  ndbout_c(
	   "Usage: %s [-l][-s <count>]\n"
	   "  -l                  Use logging and checkpointing on tables\n"
           "  -s <count>          Number of subscribers to populate, default %u\n",
	   prog, NO_OF_SUBSCRIBERS);
  
  exit(1);
}

NDB_COMMAND(DbCreate, "DbCreate", "DbCreate", "DbCreate", 16384)
{
  ndb_init();
  int i;
  UserHandle *uh;
  
  useTableLogging = 0;
  subscriberCount = NO_OF_SUBSCRIBERS;

  for(i = 1; i<argc; i++){
    if(strcmp(argv[i], "-l") == 0){
      useTableLogging = 1;
    }
    else if (strcmp(argv[i], "-s") == 0)
    {
      if ((i + 1 >= argc) ||
          (sscanf(argv[i+1], "%u", &subscriberCount) == -1))
      {
        usage(argv[0]);
        return 0;
      }
      i++;
    }
    else {
      usage(argv[0]);
      return 0;
    }
  }

  ndbout_c("Using %s tables",
	   useTableLogging ? "logging" : "temporary");
  ndbout_c("Populating %u subscribers",
           subscriberCount);
  
  myRandom48Init(0x3e6f);
  
  uh = userDbConnect(1, "TEST_DB");
  dbPopulate(uh);
  userDbDisconnect(uh);
  
  return NDBT_ProgramExit(NDBT_OK);
}
