/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <ndb_opts.h>

#include "userInterface.h"
#include "dbPopulate.h"
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <random.h>
#include <NDBT.hpp>

#ifdef	__cplusplus
extern "C" {
#endif
int useTableLogging;
#ifdef	__cplusplus
}
#endif


static void usage()
{
}

static 
void usage(const char *prog)
{
  
  ndbout_c(
	   "Usage: %s [-l]\n"
	   "  -l                  Use logging and checkpointing on tables\n",
	   prog);
  
  exit(1);
}

NDB_STD_OPTS_VARS;

NDB_COMMAND(DbCreate, "DbCreate", "DbCreate", "DbCreate", 16384)
{
  ndb_init();
  int i;
  UserHandle *uh;
  
  useTableLogging = 0;
  
  for(i = 1; i<argc; i++){
    if(strcmp(argv[i], "-l") == 0){
      useTableLogging = 1;
    } else {
      usage(argv[0]);
      return 0;
    }
  }

  ndbout_c("Using %s tables",
	   useTableLogging ? "logging" : "temporary");
  
  myRandom48Init(0x3e6f);
  
  uh = userDbConnect(1, "TEST_DB");
  dbPopulate(uh);
  userDbDisconnect(uh);
  
  return NDBT_ProgramExit(NDBT_OK);
}
