/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include "NdbToolsLogging.hpp"
#include "NdbToolsProgramExitCodes.hpp"

#include "my_alloc.h"
#include "portlib/ssl_applink.h"

static const char *_dbname = "TEST_DB";

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::mgmd_host,
    NdbStdOpt::connectstring,
    NdbStdOpt::ndb_nodeid,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::connect_retries,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    NDB_STD_OPT_DEBUG{"database", 'd', "Name of database table is in", &_dbname,
                      nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr,
                      0, nullptr},
    NdbStdOpt::end_of_options};

static void short_usage_sub() {
  ndb_short_usage_sub("<table name>[, <table name>[, ...]]");
}

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub);
  if (opts.handle_options()) return NdbToolsProgramExitCode::WRONG_ARGS;
  if (argc < 1) {
    opts.usage();
    return NdbToolsProgramExitCode::WRONG_ARGS;
  }

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndb_drop_table");
  con.configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con.connect(opt_connect_retries - 1, opt_connect_retry_delay, 1) != 0) {
    ndbout << "Unable to connect to management server." << endl;
    return NdbToolsProgramExitCode::FAILED;
  }
  if (con.wait_until_ready(30, 3) < 0) {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NdbToolsProgramExitCode::FAILED;
  }

  Ndb MyNdb(&con, _dbname);
  if (MyNdb.init() != 0) {
    NDB_ERR(MyNdb.getNdbError());
    return NdbToolsProgramExitCode::FAILED;
  }

  int res = 0;
  for (int i = 0; i < argc; i++) {
    ndbout << "Dropping table " << argv[i] << "...";
    int tmp;
    if ((tmp = MyNdb.getDictionary()->dropTable(argv[i])) != 0) {
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      res = tmp;
    } else {
      ndbout << "OK" << endl;
    }
  }

  if (res != 0) {
    return NdbToolsProgramExitCode::FAILED;
  }

  return NdbToolsProgramExitCode::OK;
}
