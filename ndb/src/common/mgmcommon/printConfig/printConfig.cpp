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

#include <NdbMain.h>
#include <mgmapi.h>
#include <ConfigRetriever.hpp>
#include <Properties.hpp>
#include <NdbOut.hpp>

void usage(const char * prg){
  ndbout << "Usage " << prg 
	 << " host <mgm host> <mgm port> <node id> [<ver id>]" << endl;
  
  char buf[255];
  for(unsigned i = 0; i<strlen(prg); i++)
    buf[i] = ' ';
  buf[strlen(prg)] = 0;
  ndbout << "      " << buf << "  file <filename> <node id> [<ver id>]"
	 << endl;
}

NDB_COMMAND(printConfig, 
	    "printConfig", "printConfig", "Prints configuration", 16384){ 
  if(argc < 4){
    usage(argv[0]);
    return 0;
  }
  if(strcmp("file", argv[1]) != 0 && strcmp("host", argv[1]) != 0){
    usage(argv[0]);
    return 0;
  }
  
  if(strcmp("host", argv[1]) == 0 && argc < 5){
    usage(argv[0]);
    return 0;
  }
  
  ConfigRetriever c; 
  struct ndb_mgm_configuration * p = 0;

  if(strcmp("host", argv[1]) == 0){
    int verId = 0;
    if(argc > 5)
      verId = atoi(argv[5]);
    
    ndbout << "Getting config from: " << argv[2] << ":" << atoi(argv[3]) 
	   << " NodeId =" << atoi(argv[4]) 
	   << " VersionId = " << verId << endl;
    
    p = c.getConfig(argv[2], 
		    atoi(argv[3]), 
		    verId);
  } else if (strcmp("file", argv[1]) == 0){
    int verId = 0;
    if(argc > 4)
      verId = atoi(argv[4]);
    
    ndbout << "Getting config from: " << argv[2]
	   << " NodeId =" << atoi(argv[3]) 
	   << " VersionId = " << verId << endl;
    
    p = c.getConfig(argv[2], atoi(argv[3]), verId);
  }
  
  if(p != 0){
    //
    free(p);
  } else {
    ndbout << "Configuration not found: " << c.getErrorString() << endl;
  }

  return 0;
}
