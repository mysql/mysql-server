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
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

static const char* opt_connect_str= 0;
static const char* _dbname = "TEST_DB";
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}
static void usage()
{
  char desc[] = 
    "tabname\n"\
    "This program will drop one table in Ndb\n";
  print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:O,/tmp/ndb_drop_table.trace");
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "ndb_tools",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if (argc < 1) {
    usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb::setConnectString(opt_connect_str);
  Ndb MyNdb(_dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
  
  int res = 0;
  for(int i = 0; i<argc; i++){
    ndbout << "Dropping table " <<  argv[i] << "...";
    int tmp;
    if((tmp = MyNdb.getDictionary()->dropTable(argv[i])) != 0){
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
