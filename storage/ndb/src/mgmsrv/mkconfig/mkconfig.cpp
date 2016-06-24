/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include <ndb_version.h>
#include <mgmapi_configuration.hpp>

#include <NdbMain.h>
#include <Properties.hpp>

#include "InitConfigFileParser.hpp"
#include <Config.hpp>

void usage(const char * prg){
  ndbout << "Usage " << prg << ": <Init config> <Binary file>" << endl;
  
}

inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  exit(exitcode);
}

NDB_COMMAND(mkconfig, 
	    "mkconfig", "mkconfig", 
	    "Make a binary configuration from a config file", 16384){ 
  ndb_init();
  if(argc < 3){
    usage(argv[0]);
    ndb_end_and_exit(0);
  }
  
  InitConfigFileParser parser;
  Config* _cp;

  if ((_cp = parser.parseConfig(argv[1])) == 0)
  {
    ndb_end_and_exit(0);
  }

  ConfigValues* cp = &_cp->m_configValues->m_config;
  Uint32 sz = cp->getPackedSize();
  UtilBuffer buf;
  if(!cp->pack(buf))
  {
    ndb_end_and_exit(-1);
  }

  FILE * f = fopen(argv[2], "w");
  if(fwrite(buf.get_data(), 1, buf.length(), f) != sz){
    fclose(f);
    unlink(argv[2]);
    ndb_end_and_exit(-1);
  }
  fclose(f);
  ndb_end_and_exit(0);
}
