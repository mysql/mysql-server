/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

#include <getarg.h>

static NdbDictionary::ForeignKey::FkAction parse_action(const char *);

int
main(int argc, const char** argv){
  ndb_init();

  const char* _dbname = "TEST_DB";
  int _help = 0;
  char* _parent = NULL;
  char* _parenti = NULL;
  char* _child = NULL;
  char* _childi = NULL;
  const char* on_update_action = "noaction";
  const char* on_delete_action = "noaction";

  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname",
      "Name of database table is in"},
    { "parent", 'p', arg_string, &_parent, "Parent table", "" },
    { "parent-index", 'i', arg_string, &_parenti, "Parent index", "" },
    { "child", 'c', arg_string, &_child, "Child table", "" },
    { "child-index", 'j', arg_string, &_childi, "Child index", "" },
    { "on-update-action", 0, arg_string, &on_update_action, "On update action", "" },
    { "on-delete-action", 0, arg_string, &on_delete_action, "On delete action", "" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
    "<tabname>+\n"\
    "This program will create one unique hash index named ind_<tabname> "
    " for each table. The index will contain all columns in the table";

  if(getarg(args, num_args, argc, argv, &optind) || _help ||
     argv[optind] == NULL){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;

  NdbDictionary::Dictionary * dict = MyNdb.getDictionary();

  const NdbDictionary::Table * parent = dict->getTable(_parent);
  if (parent == 0)
  {
    g_err << "Unknown table: " << _parent << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  const NdbDictionary::Table * child = dict->getTable(_child);
  if (child == 0)
  {
    g_err << "Unknown table: " << _child << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  const NdbDictionary::Index * pi = 0;
  const NdbDictionary::Index * ci = 0;
  if (_parenti != 0)
  {
    pi = dict->getIndex(_parenti, _parent);
    if (pi == 0)
    {
      g_err << "Unknown parent index: " << _parenti << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  }

  if (_childi != 0)
  {
    ci = dict->getIndex(_childi, _child);
    if (ci == 0)
    {
      g_err << "Unknown child index: " << _childi
            << " on " << _child << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  }

  const char * name = argv[optind];
  NdbDictionary::ForeignKey fk;
  fk.setName(name);
  fk.setParent(* parent, pi);
  fk.setChild(* child, ci);

  NdbDictionary::ForeignKey::FkAction fu = parse_action(on_update_action);
  fk.setOnUpdateAction(fu);

  NdbDictionary::ForeignKey::FkAction fd = parse_action(on_delete_action);
  fk.setOnDeleteAction(fd);

  const int res = dict->createForeignKey(fk);
  if(res != 0)
  {
    ndbout << endl << dict->getNdbError() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  ndbout << "OK" << endl;
  return NDBT_ProgramExit(NDBT_OK);
}

static
NdbDictionary::ForeignKey::FkAction
parse_action(const char * str)
{
  if (native_strcasecmp(str, "noaction") == 0)
    return NdbDictionary::ForeignKey::NoAction;
  if (native_strcasecmp(str, "restrict") == 0)
    return NdbDictionary::ForeignKey::Restrict;
  else if (native_strcasecmp(str, "cascade") == 0)
    return NdbDictionary::ForeignKey::Cascade;
  else if (native_strcasecmp(str, "setnull") == 0)
    return NdbDictionary::ForeignKey::SetNull;
  else if (native_strcasecmp(str, "setnull") == 0)
    return NdbDictionary::ForeignKey::SetDefault;

  ndbout_c("Unknown action: %s "
           " (supported: noaction, restrict, cascade, setnull, setdefault)\n",
           str);
  exit(0);
}
