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
#include "UtilTransactions.hpp"

#include <getarg.h>

int main(int argc, const char** argv){
  ndb_init();

  const char* _tabname = NULL;
  const char* _to_tabname = NULL;
  const char* _dbname = "TEST_DB";
  const char* _connectstr = NULL;
  int _copy_data = true;
  int _help = 0;
  
  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"}, 
    { "connstr", 'c', arg_string, &_connectstr, "connect string", 
      "How to connect to NDB"}, 
    { "copy-data", '\0', arg_negative_flag, &_copy_data, "Don't copy data to new table", 
      "How to connect to NDB"}, 
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "srctab desttab\n"\
    "This program will copy one table in Ndb\n";

  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || argv[optind + 1] == NULL || _help){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  _to_tabname = argv[optind+1];
  
  Ndb_cluster_connection con(_connectstr);
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb(&con,_dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
  
  ndbout << "Copying table " <<  _tabname << " to " << _to_tabname << "...";
  const NdbDictionary::Table* ptab = MyNdb.getDictionary()->getTable(_tabname);
  if (ptab){
    NdbDictionary::Table tab2(*ptab);
    tab2.setName(_to_tabname);
    if (MyNdb.getDictionary()->createTable(tab2) != 0){
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  } else {
    ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  ndbout << "OK" << endl;
  if (_copy_data){
    ndbout << "Copying data..."<<endl;
    const NdbDictionary::Table * tab3 = 
      NDBT_Table::discoverTableFromDb(&MyNdb, 
				      _tabname);
    //    if (!tab3)

    UtilTransactions util(*tab3);

    if(util.copyTableData(&MyNdb,
			  _to_tabname) != NDBT_OK){
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    ndbout << "OK" << endl;
  }
  return NDBT_ProgramExit(NDBT_OK);
}
