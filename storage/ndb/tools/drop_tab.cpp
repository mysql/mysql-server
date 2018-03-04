/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

static const char* _dbname = "TEST_DB";

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int main(int argc, char** argv){
  Ndb_opts opts(argc, argv, my_long_options);
  if (opts.handle_options())
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if (argc < 1) {
    opts.usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndb_drop_table");
  if(con.connect(opt_connect_retries - 1, opt_connect_retry_delay, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (con.wait_until_ready(30,3) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname );
  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
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
