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
#include <getarg.h>

#include <NdbApi.hpp>
#include <NDBT.hpp>

static Ndb_cluster_connection *ndb_cluster_connection= 0;
static Ndb* ndb = 0;
static NdbDictionary::Dictionary* dic = 0;
static int _unqualified = 0;

static void
fatal(char const* fmt, ...)
{
    va_list ap;
    char buf[500];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
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
    vsnprintf(buf, sizeof(buf), fmt, ap);
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

#ifndef DBUG_OFF
const char *debug_option= 0;
#endif

int main(int argc, const char** argv){
  ndb_init();
  int _loops = 1;
  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _type = 0;
  int _help = 0;
  const char* _connect_str = NULL;
  
  struct getargs args[] = {
    { "loops", 'l', arg_integer, &_loops, "loops", 
      "Number of times to run(default = 1)" },
    { "unqualified", 'u', arg_flag, &_unqualified, "unqualified", 
      "Use unqualified table names"}, 
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"}, 
    { "type", 't', arg_integer, &_type, "type", 
      "Type of objects to show, see NdbDictionary.hpp for numbers(default = 0)" },
    { "connect-string", 'c', arg_string, &_connect_str,
      "Set connect string for connecting to ndb_mgmd. <constr>=\"host=<hostname:port>[;nodeid=<id>]\". Overides specifying entries in NDB_CONNECTSTRING and config file",
      "<constr>" },
#ifndef DBUG_OFF
    { "debug", 0, arg_string, &debug_option,
      "Specify debug options e.g. d:t:i:o,out.trace", "options" },
#endif
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program list all system objects in  NDB Cluster.\n"\
    "Type of objects to display can be limited with -t option\n"\
    " ex: list_tables -t 2 would show all UserTables\n"\
    "To show all indexes for a table write table name as final argument\n"\
    "  ex: list_tables T1\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  
#ifndef DBUG_OFF
  if (debug_option)
    DBUG_PUSH(debug_option);
#endif

  ndb_cluster_connection = new Ndb_cluster_connection(_connect_str);
  ndb = new Ndb(ndb_cluster_connection, _dbname);
  if (ndb->init() != 0)
    fatal("init");
  ndb_cluster_connection->connect();
  if (ndb->waitUntilReady(30) < 0)
    fatal("waitUntilReady");
  dic = ndb->getDictionary();
  for (int i = 0; _loops == 0 || i < _loops; i++) {
    list(_tabname, (NdbDictionary::Object::Type)_type);
  }
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=4:
