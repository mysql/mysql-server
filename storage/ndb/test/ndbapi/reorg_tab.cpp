/*
   Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <ndb_global.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

#include <getarg.h>

int main(int argc, const char** argv){
  ndb_init();

  int _help = 0;
  int _p = 0;
  const char * db = "TEST_DB";
  const char* _connectstr = NULL;

  struct getargs args[] = {
    { "database", 'd', arg_string, &db, "database", 0 },
    { "connstr", 'c', arg_string, &_connectstr, "Connect string", "cs" },
    { "partitions", 'p', arg_integer, &_p, "New no of partitions", 0},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
    "tabname\n"                                                         \
    "This program will alter no of partitions of table in Ndb.\n";

  if(getarg(args, num_args, argc, argv, &optind) || _help){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  if(argv[optind] == NULL)
  {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }


  // Connect to Ndb
  Ndb_cluster_connection con(_connectstr);
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb(&con, db );

  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;

  NdbDictionary::Dictionary* MyDic = MyNdb.getDictionary();
  for (int i = optind; i<argc; i++)
  {
    printf("altering %s/%s...", db, argv[i]);
    const NdbDictionary::Table* oldTable = MyDic->getTable(argv[i]);

    if (oldTable == 0)
    {
      ndbout << "Failed to retrieve table " << argv[i]
             << ": " << MyDic->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    NdbDictionary::Table newTable = *oldTable;
    newTable.setFragmentCount(_p);

    if (MyDic->beginSchemaTrans() != 0)
      goto err;

    if (MyDic->prepareHashMap(*oldTable, newTable) != 0)
      goto err;

    if (MyDic->alterTable(*oldTable, newTable) != 0)
      goto err;

    if (MyDic->endSchemaTrans())
      goto err;

    ndbout_c("done");
  }

  return NDBT_ProgramExit(NDBT_OK);

err:
  NdbError err = MyDic->getNdbError();
  if (MyDic->hasSchemaTrans())
    MyDic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);

  ndbout << "Failed! "
         << err << endl;
  return NDBT_ProgramExit(NDBT_FAILED);

}
