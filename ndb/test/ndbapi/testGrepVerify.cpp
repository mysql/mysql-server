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


#include "mgmapi.h"
#include <string.h>
#include <NdbMain.h>
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <getarg.h>

#include <NdbApi.hpp>
#include <NDBT.hpp>

#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>


#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< "getStep" \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int main(int argc, const char** argv){
  ndb_init();


  const char * connectString = NULL;
  const char * table = NULL;
  int records = 0;
  int _help = 0;
  
  struct getargs args[] = {
    { "connectString", 'c', arg_string, &connectString, 
      "ConnectString", "nodeid=<api id>;host=<hostname:port>" },
    { "tableName", 't', arg_string, &table, 
      "table", "Table" },
    { "records", 'r', arg_integer, &records, "Number of records", "recs"},
    { "usage", '?', arg_flag, &_help, "Print help", "" }    
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "hostname:port\n"\
    "This program will connect to the mgmsrv of a NDB cluster.\n"\
    "It will then wait for all nodes to be started, then restart node(s)\n"\
    "and wait for all to restart inbetween. It will do this \n"\
    "loop number of times\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  ndbout_c("table %s connectStirng %s", table, connectString);  
  if(connectString == 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if(table == 0) 
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  Ndb * m_ndb = new Ndb("");
  m_ndb->useFullyQualifiedNames(false);
  m_ndb->setConnectString(connectString);
  /**
   * @todo  Set proper max no of transactions?? needed?? Default 12??
   */
  m_ndb->init(2048);
  if (m_ndb->waitUntilReady() != 0){
    ndbout_c("NDB Cluster not ready for connections");
  }

  int count = 0;
  int result = NDBT_OK;


  const NdbDictionary::Table * tab =  NDBT_Table::discoverTableFromDb( m_ndb, table);
//  ndbout << *tab << endl;

  UtilTransactions utilTrans(*tab);
  HugoTransactions hugoTrans(*tab);

  do{

    // Check that there are as many records as we expected
    CHECK(utilTrans.selectCount(m_ndb, 64, &count) == 0);
    
    g_err << "count = " << count;
    g_err << " records = " << records;
    g_err << endl;

    CHECK(count == records);
    
    // Read and verify every record
    CHECK(hugoTrans.pkReadRecords(m_ndb, records) == 0);

  } while (false);
  

  return NDBT_ProgramExit(result);    

}
