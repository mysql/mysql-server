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
#include <NDBT.hpp>
#include <NdbApi.hpp>

static const char* opt_connect_str= 0;
static const char* _dbname = "TEST_DB";
static int _unqualified = 0;
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "unqualified", 'u', "Use unqualified table names",
    (gptr*) &_unqualified, (gptr*) &_unqualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
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
    "This program list all properties of table(s) in NDB Cluster.\n"\
    "  ex: desc T1 T2 T4\n";  
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
    DBUG_PUSH(argument ? argument : "d:t:O,/tmp/ndb_desc.trace");
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

  Ndb::setConnectString(opt_connect_str);

  Ndb* pMyNdb;
  pMyNdb = new Ndb(_dbname);  
  pMyNdb->init();
  
  ndbout << "Waiting...";
  while (pMyNdb->waitUntilReady() != 0) {
    ndbout << "...";
  }
  ndbout << endl;

  NdbDictionary::Dictionary * dict = pMyNdb->getDictionary();
  for (int i = 0; i < argc; i++) {
    NDBT_Table* pTab = (NDBT_Table*)dict->getTable(argv[i]);
    if (pTab != 0){
      ndbout << (* pTab) << endl;

      NdbDictionary::Dictionary::List list;
      if (dict->listIndexes(list, argv[i]) != 0){
	ndbout << argv[i] << ": " << dict->getNdbError() << endl;
	return NDBT_ProgramExit(NDBT_FAILED);
      }
        
      ndbout << "-- Indexes -- " << endl;
      ndbout << "PRIMARY KEY(";
      unsigned j;
      for (j= 0; (int)j < pTab->getNoOfPrimaryKeys(); j++)
      {
	const NdbDictionary::Column * col = pTab->getColumn(j);
	ndbout << col->getName();
	if ((int)j < pTab->getNoOfPrimaryKeys()-1)
	  ndbout << ", ";       
      }
      ndbout << ") - UniqueHashIndex" << endl;
	
      for (j= 0; j < list.count; j++) {
	NdbDictionary::Dictionary::List::Element& elt = list.elements[j];
	const NdbDictionary::Index *pIdx = dict->getIndex(elt.name, argv[i]);
	if (!pIdx){
	  ndbout << argv[i] << ": " << dict->getNdbError() << endl;
	  return NDBT_ProgramExit(NDBT_FAILED);
	}
	  
	ndbout << (*pIdx) << endl;
      }
      ndbout << endl;
    }
    else
      ndbout << argv[i] << ": " << dict->getNdbError() << endl;
  }
  
  delete pMyNdb;
  return NDBT_ProgramExit(NDBT_OK);
}
