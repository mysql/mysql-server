/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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

#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbOut.hpp>

#include <getarg.h>

int main(int argc, const char **argv) {
  ndb_init();

  const char *_dbname = "TEST_DB";
  int _help = 0;

  struct getargs args[] = {{"database", 'd', arg_string, &_dbname, "dbname",
                            "Name of database table is in"},
                           {"usage", '?', arg_flag, &_help, "Print help", ""}};

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
      "<fkname>+\n"
      "This program will create one unique hash index named ind_<tabname> "
      " for each table. The index will contain all columns in the table";

  if (getarg(args, num_args, argc, argv, &optind) || _help ||
      argv[optind] == NULL) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb_cluster_connection con;
  con.configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con.connect(12, 5, 1) != 0) {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if (MyNdb.init() != 0) {
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  while (MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;

  NdbDictionary::Dictionary *dict = MyNdb.getDictionary();

  bool ok = true;
  for (int i = optind; i < argc; i++) {
    const char *name = argv[i];
    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, name) != 0) {
      ndbout << "Failed to retreive foreign key: " << name << endl;
      ok = false;
      continue;
    }

    ndbout << "Dropping foreign key " << name << "..." << flush;
    if (dict->dropForeignKey(fk) == 0) {
      ndbout << "OK" << endl;
    } else {
      ndbout << "ERROR" << endl << dict->getNdbError() << endl;
      ok = false;
    }
  }

  if (ok) {
    return NDBT_ProgramExit(NDBT_OK);
  } else {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
}
