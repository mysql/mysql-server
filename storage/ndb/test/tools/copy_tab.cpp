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
#include "UtilTransactions.hpp"

#include <getarg.h>

int main(int argc, const char **argv) {
  ndb_init();

  const char *_tabname = NULL;
  const char *_dbname = "TEST_DB";
  const char *_connectstr = NULL;
  int _copy_data = true;
  int _help = 0;

  struct getargs args[] = {
      {"database", 'd', arg_string, &_dbname, "dbname",
       "Name of database table is in"},
      {"connstr", 'c', arg_string, &_connectstr, "connect string",
       "How to connect to NDB"},
      {"copy-data", '\0', arg_negative_flag, &_copy_data,
       "Don't copy data to new table", "How to connect to NDB"},
      {"usage", '?', arg_flag, &_help, "Print help", ""}};
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
      "srctab desttab\n"
      "This program will copy one table in Ndb\n";

  if (getarg(args, num_args, argc, argv, &optind) || argv[optind] == NULL ||
      argv[optind + 1] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

  Ndb_cluster_connection con(_connectstr);
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

  const NdbDictionary::Table *ptab = MyNdb.getDictionary()->getTable(_tabname);
  if (ptab == 0) {
    ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Vector<NdbDictionary::Index *> indexes;
  {
    NdbDictionary::Dictionary::List list;
    if (MyNdb.getDictionary()->listIndexes(list, *ptab) != 0) {
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    for (unsigned i = 0; i < list.count; i++) {
      const NdbDictionary::Index *idx =
          MyNdb.getDictionary()->getIndex(list.elements[i].name, _tabname);
      if (idx) {
        ndbout << " found index " << list.elements[i].name << endl;
        NdbDictionary::Index *copy = new NdbDictionary::Index();
        copy->setName(idx->getName());
        copy->setType(idx->getType());
        copy->setLogging(idx->getLogging());
        for (unsigned j = 0; j < idx->getNoOfColumns(); j++) {
          copy->addColumn(idx->getColumn(j)->getName());
        }
        indexes.push_back(copy);
      }
    }
  }
  for (int i = optind + 1; i < argc; i++) {
    const char *_to_tabname = argv[i];
    ndbout << "Copying table " << _tabname << " to " << _to_tabname << "...";
    NdbDictionary::Table tab2(*ptab);
    tab2.setName(_to_tabname);
    if (MyNdb.getDictionary()->beginSchemaTrans() != 0) {
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    if (MyNdb.getDictionary()->createTable(tab2) != 0) {
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    for (unsigned j = 0; j < indexes.size(); j++) {
      NdbDictionary::Index *idx = indexes[j];
      idx->setTable(_to_tabname);
      int res = MyNdb.getDictionary()->createIndex(*idx);
      if (res != 0) {
        ndbout << "Failed to create index: " << idx->getName() << " : "
               << MyNdb.getDictionary()->getNdbError() << endl;
        return NDBT_ProgramExit(NDBT_FAILED);
      }
    }

    if (MyNdb.getDictionary()->endSchemaTrans() != 0) {
      ndbout << endl << MyNdb.getDictionary()->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    ndbout << "OK" << endl;
    if (_copy_data) {
      ndbout << "Copying data..." << endl;
      const NdbDictionary::Table *tab3 =
          NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
      UtilTransactions util(*tab3);

      if (util.copyTableData(&MyNdb, _to_tabname) != NDBT_OK) {
        return NDBT_ProgramExit(NDBT_FAILED);
      }
      ndbout << "OK" << endl;
    }
  }

  for (unsigned j = 0; j < indexes.size(); j++) {
    delete indexes[j];
  }

  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<NdbDictionary::Index *>;
