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

#include <getarg.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>




int main(int argc, const char** argv){
  ndb_init();
  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _unqualified = 0;
  int _help = 0;
  
  struct getargs args[] = {
    { "unqualified", 'u', arg_flag, &_unqualified, "unqualified", 
      "Use unqualified table names"}, 
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program list all properties of table(s) in NDB Cluster.\n"\
    "  ex: desc T1 T2 T4\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL ||_help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

  Ndb* pMyNdb;
  pMyNdb = new Ndb(_dbname);  
  pMyNdb->init();
  
  ndbout << "Waiting...";
  while (pMyNdb->waitUntilReady() != 0) {
    ndbout << "...";
  }
  ndbout << endl;

  NdbDictionary::Dictionary * dict = pMyNdb->getDictionary();
  for (int i = optind; i < argc; i++) {
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
