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

#include <ndb_global.h>
#include <ndb_version.h>

#include <NdbMain.h>
#include <Properties.hpp>

#include "InitConfigFileParser.hpp"
#include <Config.hpp>
#include <assert.h>

void usage(const char * prg){
  ndbout << "Usage " << prg << ": <Init config> <Binary file>" << endl;
  
}

NDB_COMMAND(mkconfig, 
	    "mkconfig", "mkconfig", 
	    "Make a binary configuration from a config file", 16384){ 
  if(argc < 3){
    usage(argv[0]);
    return 0;
  }
  
  InitConfigFileParser parser(argv[1]);
  Config* cp;

  if (!parser.readConfigFile())
    return false;

  cp = (Config *) parser.getConfig();
  if (cp == NULL)
    return false;

  cp->put("VersionId", (Uint32)NDB_VERSION);
  
  Uint32 sz = cp->getPackedSize();
  Uint32 * buf = new Uint32[sz];
  if(!cp->pack(buf))
    return -1;

  FILE * f = fopen(argv[2], "w");
  if(fwrite(buf, 1, sz, f) != sz){
    fclose(f);
    unlink(argv[2]);
    return -1;
  }
  fclose(f);
  return 0;
}
