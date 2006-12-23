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

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

#include <getarg.h>



int 
main(int argc, const char** argv){
  ndb_init();

  const char* _dbname = "TEST_DB";
  int _help = 0;
  int _ordered = 0, _pk = 1;

  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"},
    { "ordered", 'o', arg_flag, &_ordered, "Create ordered index", "" },
    { "pk", 'p', arg_flag, &_pk, "Create index on primary key", "" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "<tabname>+\n"\
    "This program will create one unique hash index named ind_<tabname> "
    " for each table. The index will contain all columns in the table";
  
  if(getarg(args, num_args, argc, argv, &optind) || _help ||
     argv[optind] == NULL){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
  
  NdbDictionary::Dictionary * dict = MyNdb.getDictionary();
  
  for(int i = optind; i<argc; i++){
    const NdbDictionary::Table * tab = dict->getTable(argv[i]);
    if(tab == 0){
      g_err << "Unknown table: " << argv[i] << endl;
      continue;
    }
    
    if(tab->getNoOfColumns() > 16){
      g_err << "Table " <<  argv[i] << " has more than 16 columns" << endl;
    }
    
    NdbDictionary::Index ind;
    if(_ordered){
      ind.setType(NdbDictionary::Index::OrderedIndex);
      ind.setLogging(false);
    } else {
      ind.setType(NdbDictionary::Index::UniqueHashIndex);
    }
    char buf[512];
    sprintf(buf, "IND_%s_%s_%c", 
	    argv[i], (_pk ? "PK" : "FULL"), (_ordered ? 'O' : 'U'));
    ind.setName(buf);
    ind.setTable(argv[i]);
    for(int c = 0; c<tab->getNoOfColumns(); c++){
      if(!_pk || tab->getColumn(c)->getPrimaryKey())
	ind.addIndexColumn(tab->getColumn(c)->getName());
    }
    ndbout << "creating index " << buf << " on table " << argv[i] << "...";
    const int res = dict->createIndex(ind);
    if(res != 0)
      ndbout << endl << dict->getNdbError() << endl;
    else
      ndbout << "OK" << endl;
  }  
  
  return NDBT_ProgramExit(NDBT_OK);
}


