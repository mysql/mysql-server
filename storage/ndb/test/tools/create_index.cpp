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

#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbOut.hpp>

#include <getarg.h>

int main(int argc, const char **argv) {
  ndb_init();

  const char *_dbname = "TEST_DB";
  int _help = 0;
  int _ordered = 0, _pk = 1;
  char *_iname = NULL;
  char *_tname = NULL;

  struct getargs args[] = {
      {"database", 'd', arg_string, &_dbname, "dbname",
       "Name of database table is in"},
      {"ordered", 'o', arg_flag, &_ordered, "Create ordered index", ""},
      {"pk", 'p', arg_flag, &_pk, "Create index on primary key", ""},
      {"idxname", 'i', arg_string, &_iname, "idxname",
       "Override default name for index"},
      {"tabname", 't', arg_string, &_tname, "tabname",
       "Specify single tabname and list of col names as args"},
      {"usage", '?', arg_flag, &_help, "Print help", ""}};

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
      "<tabname>+\n"
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

  for (int i = optind; i < argc; i++) {
    const char *tabName = (_tname) ? _tname : argv[i];
    const NdbDictionary::Table *tab = dict->getTable(tabName);
    if (tab == 0) {
      g_err << "Unknown table: " << tabName << endl;
      if (_tname) return NDBT_ProgramExit(NDBT_FAILED);
      continue;
    }

    NdbDictionary::Index ind;
    if (_ordered) {
      ind.setType(NdbDictionary::Index::OrderedIndex);
      ind.setLogging(false);
    } else {
      ind.setType(NdbDictionary::Index::UniqueHashIndex);
    }
    char buf[512];
    if (!_iname) {
      sprintf(buf, "IND_%s_%s_%c", argv[i], (_pk ? "PK" : "FULL"),
              (_ordered ? 'O' : 'U'));
      ind.setName(buf);
    } else {
      ind.setName(_iname);
    }

    ind.setTable(tabName);

    if (!_tname) {
      ndbout << "creating index " << ind.getName() << " on table " << tabName
             << "(";
      for (int c = 0; c < tab->getNoOfColumns(); c++) {
        if (!_pk || tab->getColumn(c)->getPrimaryKey()) {
          ndbout << tab->getColumn(c)->getName() << ", ";
          ind.addIndexColumn(tab->getColumn(c)->getName());
        }
      }
      ndbout << ")" << endl;
    } else {
      /* Treat args as column names */
      ndbout << "creating index " << ind.getName() << " on table " << tabName
             << "(";
      for (int argNum = i; argNum < argc; argNum++) {
        const char *colName = argv[argNum];
        if (tab->getColumn(colName) == NULL) {
          g_err << "Column " << colName << " does not exist in table "
                << tabName << endl;
          return NDBT_ProgramExit(NDBT_FAILED);
        }
        ndbout << colName << ", ";
        ind.addIndexColumn(colName);
      }
      ndbout << ")" << endl;
    }
    const int res = dict->createIndex(ind);
    if (res != 0)
      ndbout << endl << dict->getNdbError() << endl;
    else
      ndbout << "OK" << endl;

    if (_tname)  // Just create a single index
      return NDBT_ProgramExit(NDBT_OK);
  }

  return NDBT_ProgramExit(NDBT_OK);
}
