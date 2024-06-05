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

#include <NdbSleep.h>
#include <getarg.h>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbOut.hpp>

int main(int argc, const char **argv) {
  ndb_init();

  int _records = 0;
  int _help = 0;
  int _batch = 512;
  int _loops = 0;
  int _rand = 0;
  int _onetrans = 0;
  int _abort = 0;
  const char *db = 0;

  struct getargs args[] = {
      {"records", 'r', arg_integer, &_records, "Number of records", "recs"},
      {"batch", 'b', arg_integer, &_batch,
       "Number of operations in each transaction", "batch"},
      {"loops", 'l', arg_integer, &_loops, "Number of loops", ""},
      {"database", 'd', arg_string, &db, "Database", ""},
      {"usage", '?', arg_flag, &_help, "Print help", ""},
      {"rnd-rows", 0, arg_flag, &_rand, "Rand number of records", "recs"},
      {"one-trans", 0, arg_flag, &_onetrans, "Insert as 1 trans", ""},
      {"abort", 0, arg_integer, &_abort, "Abort probability", ""}};
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
      "tabname\n"
      "This program will load one table in Ndb with calculated data. \n"
      "This means that it is possible to check the validity of the data \n"
      "at a later time. The last column in each table is used as an update \n"
      "counter, it's initialised to zero and should be incremented for each \n"
      "update of the record. \n";

  if (getarg(args, num_args, argc, argv, &optind) || argv[optind] == NULL ||
      _records == 0 || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  // Connect to Ndb
  Ndb_cluster_connection con;
  con.configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con.connect(12, 5, 1) != 0) {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (con.wait_until_ready(30, 0) < 0) {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, db ? db : "TEST_DB");

  if (MyNdb.init() != 0) {
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  for (int i = optind; i < argc; i++) {
    const char *_tabname = argv[i];
    // Check if table exists in db
    const NdbDictionary::Table *pTab =
        NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
    if (pTab == NULL) {
      ndbout << " Table " << _tabname << " does not exist!" << endl;
      return NDBT_ProgramExit(NDBT_WRONGARGS);
    }

    HugoTransactions hugoTrans(*pTab);
  loop:
    int rows = (_rand ? rand() % _records : _records);
    int abort = (rand() % 100) < _abort ? 1 : 0;
    if (abort) ndbout << "load+abort" << endl;
    if (hugoTrans.loadTable(&MyNdb, rows, _batch, true, 0, _onetrans, _loops,
                            abort) != 0) {
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    if (_loops > 0) {
      ndbout << "clearing..." << endl;
      hugoTrans.clearTable(&MyNdb);
      // hugoTrans.pkDelRecords(&MyNdb, _records);
      _loops--;
      goto loop;
    }
  }

  return NDBT_ProgramExit(NDBT_OK);
}
