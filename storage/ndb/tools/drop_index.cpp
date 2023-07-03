/*
<<<<<<< HEAD
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.
=======
   Copyright (c) 2003, 2021, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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

#include "my_alloc.h"

static const char* _dbname = "TEST_DB";

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "database", 'd', "Name of database table is in",
    &_dbname, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

int main(int argc, char** argv){
<<<<<<< HEAD
  NDB_INIT(argv[0]);
=======
<<<<<<< HEAD
>>>>>>> pr/231
  Ndb_opts opts(argc, argv, my_long_options);
  if (opts.handle_options())
=======
  NDB_INIT(argv[0]);
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ndb_load_defaults(NULL,load_default_groups,&argc,&argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
  {
    ndb_free_defaults(argv);
>>>>>>> upstream/cluster-7.6
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if (argc < 1) {
<<<<<<< HEAD
    opts.usage();
=======
    usage();
    ndb_free_defaults(argv);
>>>>>>> upstream/cluster-7.6
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  
  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndb_drop_index");
  if(con.connect(opt_connect_retries - 1, opt_connect_retry_delay, 1) != 0)
  {
    ndb_free_defaults(argv);
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (con.wait_until_ready(30,3) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    ndb_free_defaults(argv);
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname );
  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    ndb_free_defaults(argv);
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  int res = 0;
  for(int i = 0; i+1<argc; i += 2){
    ndbout << "Dropping index " << argv[i] << "/" << argv[i+1] << "...";
    int tmp;
    if((tmp = MyNdb.getDictionary()->dropIndex(argv[i+1], argv[i])) != 0){
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      res = tmp;
    } else {
      ndbout << "OK" << endl;
    }
  }
  
  if(res != 0)
  {
    ndb_free_defaults(argv);
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  ndb_free_defaults(argv);
  return NDBT_ProgramExit(NDBT_OK);
}
