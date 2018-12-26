/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <ndb_opts.h>

#include "userInterface.h"
#include "dbPopulate.h"
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


int main(int argc, char** argv)
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
