/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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
#include <ndb_version.h>
#include <mgmapi_configuration.hpp>

#include <NdbMain.h>
#include <Properties.hpp>

#include "InitConfigFileParser.hpp"
#include <Config.hpp>

void usage(const char * prg){
  ndbout << "Usage " << prg << ": <Init config> <Binary file>" << endl;
  
}

NDB_COMMAND(mkconfig, 
	    "mkconfig", "mkconfig", 
	    "Make a binary configuration from a config file", 16384){ 
  ndb_init();
  if(argc < 3){
    usage(argv[0]);
    return 0;
  }
  
  InitConfigFileParser parser;
  Config* _cp;

  if ((_cp = parser.parseConfig(argv[1])) == 0)
    return false;

  ConfigValues* cp = &_cp->m_configValues->m_config;
  Uint32 sz = cp->getPackedSize();
  UtilBuffer buf;
  if(!cp->pack(buf))
    return -1;
  
  FILE * f = fopen(argv[2], "w");
  if(fwrite(buf.get_data(), 1, buf.length(), f) != sz){
    fclose(f);
    unlink(argv[2]);
    return -1;
  }
  fclose(f);
  return 0;
}
