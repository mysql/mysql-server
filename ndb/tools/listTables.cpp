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

/*
 * list_tables
 *
 * List objects(tables, triggers, etc.) in NDB Cluster
 *
 */

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbApi.hpp>
#include <NDBT.hpp>

static Ndb_cluster_connection *ndb_cluster_connection= 0;
static Ndb* ndb = 0;
static const NdbDictionary::Dictionary * dic = 0;
static int _unqualified = 0;

const char *load_default_groups[]= { "mysql_cluster",0 };

static void
fatal(char const* fmt, ...)
{
    va_list ap;
    char buf[500];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << buf;
    if (ndb)
      ndbout << " - " << ndb->getNdbError();
    ndbout << endl;
    NDBT_ProgramExit(NDBT_FAILED);
    exit(1);
}

static void
fatal_dict(char const* fmt, ...)
{
    va_list ap;
    char buf[500];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << buf;
    if (dic)
      ndbout << " - " << dic->getNdbError();
    ndbout << endl;
    NDBT_ProgramExit(NDBT_FAILED);
    exit(1);
}

static void
list(const char * tabname, 
     NdbDictionary::Object::Type type)
{
    NdbDictionary::Dictionary::List list;
    if (tabname == 0) {
	if (dic->listObjects(list, type) == -1)
	    fatal_dict("listObjects");
    } else {
	if (dic->listIndexes(list, tabname) == -1)
	    fatal_dict("listIndexes");
    }
    if (ndb->usingFullyQualifiedNames())
       ndbout_c("%-5s %-20s %-8s %-7s %-12s %-8s %s", "id", "type", "state", "logging", "database", "schema", "name");
     else
       ndbout_c("%-5s %-20s %-8s %-7s %s", "id", "type", "state", "logging", "name");
    for (unsigned i = 0; i < list.count; i++) {
	NdbDictionary::Dictionary::List::Element& elt = list.elements[i];
        char type[100];
	bool isTable = false;
        switch (elt.type) {
        case NdbDictionary::Object::SystemTable:
            strcpy(type, "SystemTable");
	    isTable = true;
            break;
        case NdbDictionary::Object::UserTable:
            strcpy(type, "UserTable");
	    isTable = true;
            break;
        case NdbDictionary::Object::UniqueHashIndex:
            strcpy(type, "UniqueHashIndex");
	    isTable = true;
            break;
        case NdbDictionary::Object::OrderedIndex:
            strcpy(type, "OrderedIndex");
	    isTable = true;
            break;
        case NdbDictionary::Object::HashIndexTrigger:
            strcpy(type, "HashIndexTrigger");
            break;
        case NdbDictionary::Object::IndexTrigger:
            strcpy(type, "IndexTrigger");
            break;
        case NdbDictionary::Object::SubscriptionTrigger:
            strcpy(type, "SubscriptionTrigger");
            break;
        case NdbDictionary::Object::ReadOnlyConstraint:
            strcpy(type, "ReadOnlyConstraint");
            break;
        default:
            sprintf(type, "%d", (int)elt.type);
            break;
        }
        char state[100];
        switch (elt.state) {
        case NdbDictionary::Object::StateOffline:
            strcpy(state, "Offline");
            break;
        case NdbDictionary::Object::StateBuilding:
            strcpy(state, "Building");
            break;
        case NdbDictionary::Object::StateDropping:
            strcpy(state, "Dropping");
            break;
        case NdbDictionary::Object::StateOnline:
            strcpy(state, "Online");
            break;
        case NdbDictionary::Object::StateBackup:
	    strcpy(state, "Backup");
            break;
        case NdbDictionary::Object::StateBroken:
            strcpy(state, "Broken");
            break;
        default:
            sprintf(state, "%d", (int)elt.state);
            break;
        }
        char store[100];
	if (! isTable)
	    strcpy(store, "-");
	else {
	    switch (elt.store) {
	    case NdbDictionary::Object::StoreTemporary:
		strcpy(store, "No");
		break;
	    case NdbDictionary::Object::StorePermanent:
		strcpy(store, "Yes");
		break;
	    default:
		sprintf(state, "%d", (int)elt.store);
		break;
	    }
	}
	if (ndb->usingFullyQualifiedNames())
	  ndbout_c("%-5d %-20s %-8s %-7s %-12s %-8s %s", elt.id, type, state, store, (elt.database)?elt.database:"", (elt.schema)?elt.schema:"", elt.name);
       else
	 ndbout_c("%-5d %-20s %-8s %-7s %s", elt.id, type, state, store, elt.name);
    }
}

NDB_STD_OPTS_VARS;

static const char* _dbname = "TEST_DB";
static int _loops;
static int _type;
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_show_tables"),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "loops", 'l', "loops",
    (gptr*) &_loops, (gptr*) &_loops, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 }, 
  { "type", 't', "type",
    (gptr*) &_type, (gptr*) &_type, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "unqualified", 'u', "Use unqualified table names",
    (gptr*) &_unqualified, (gptr*) &_unqualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void usage()
{
  char desc[] = 
    "tabname\n"\
    "This program list all system objects in  NDB Cluster.\n"\
    "Type of objects to display can be limited with -t option\n"\
    " ex: ndb_show_tables -t 2 would show all UserTables\n"\
    "To show all indexes for a table write table name as final argument\n"\
    "  ex: ndb_show_tables T1\n";
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char* _tabname;
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_show_tables.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  _tabname = argv[0];

  ndb_cluster_connection = new Ndb_cluster_connection(opt_connect_str);
  if (ndb_cluster_connection->connect(12,5,1))
    fatal("Unable to connect to management server.");
  if (ndb_cluster_connection->wait_until_ready(30,0) < 0)
    fatal("Cluster nodes not ready in 30 seconds.");

  ndb = new Ndb(ndb_cluster_connection, _dbname);
  if (ndb->init() != 0)
    fatal("init");
  dic = ndb->getDictionary();
  for (int i = 0; _loops == 0 || i < _loops; i++) {
    list(_tabname, (NdbDictionary::Object::Type)_type);
  }
  delete ndb;
  delete ndb_cluster_connection;
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=4:
