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

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

#include <getarg.h>

int main(int argc, const char** argv){
  ndb_init();

  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _help = 0;
  
  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "<indexname>+\n"\
    "This program will drop index(es) in Ndb\n";

  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || _help){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  
  // Connect to Ndb
  Ndb MyNdb(_dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
  
  int res = 0;
  for(int i = optind; i<argc; i++){
    ndbout << "Dropping index " <<  argv[i] << "...";
    int tmp;
    if((tmp = MyNdb.getDictionary()->dropIndex(argv[i], 0)) != 0){
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      res = tmp;
    } else {
      ndbout << "OK" << endl;
    }
  }
  
  if(res != 0){
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  return NDBT_ProgramExit(NDBT_OK);
}
