/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
static int _fully_qualified = 0;
static int _parsable = 0;
static int show_temp_status = 0;

static void
fatal(char const* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

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
  ATTRIBUTE_FORMAT(printf, 1, 2);

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
    /**
     * Display fully qualified table names if --fully-qualified is set to 1.
     *
     * useFq passed to listObjects() and listIndexes() below in this context
     * actually behaves like 'unqualified'.
     * useFq == true : Strip off the database and schema (and tableid) and
     * return the table/index name
     * useFq == false : Return the full name
     * (database/schema/[tableid/]indexname|tablename)
     */
    bool useFq = !_fully_qualified;

    NdbDictionary::Dictionary::List list;
    if (tabname == 0) {
	if (dic->listObjects(list, type, useFq) == -1)
	    fatal_dict("listObjects");
    } else {
	if (dic->listIndexes(list, tabname, useFq) == -1)
	    fatal_dict("listIndexes");
    }
    if (!_parsable)
    {
      if (ndb->usingFullyQualifiedNames())
      {
        if (show_temp_status)
          ndbout_c("%-5s %-20s %-8s %-7s %-4s %-12s %-8s %s", "id", "type", "state", "logging", "temp", "database", "schema", "name");
        else
          ndbout_c("%-5s %-20s %-8s %-7s %-12s %-8s %s", "id", "type", "state", "logging", "database", "schema", "name");
      }
      else
      {
        if (show_temp_status)
          ndbout_c("%-5s %-20s %-8s %-7s %-4s %s", "id", "type", "state", "logging", "temp", "name");
        else
          ndbout_c("%-5s %-20s %-8s %-7s %s", "id", "type", "state", "logging", "name");
      }
    }
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
        case NdbDictionary::Object::ReorgTrigger:
            strcpy(type, "ReorgTrigger");
            break;
	case NdbDictionary::Object::Tablespace:
	  strcpy(type, "Tablespace");
	  break;
	case NdbDictionary::Object::LogfileGroup:
	  strcpy(type, "LogfileGroup");
	  break;
	case NdbDictionary::Object::Datafile:
	  strcpy(type, "Datafile");
	  break;
	case NdbDictionary::Object::Undofile:
	  strcpy(type, "Undofile");
	  break;
        case NdbDictionary::Object::TableEvent:
            strcpy(type, "TableEvent");
            break;
        case NdbDictionary::Object::ForeignKey:
            strcpy(type, "ForeignKey");
            break;
        case NdbDictionary::Object::FKParentTrigger:
            strcpy(type, "FKParentTrigger");
            break;
        case NdbDictionary::Object::FKChildTrigger:
            strcpy(type, "FKChildTrigger");
            break;
        case NdbDictionary::Object::HashMap:
            strcpy(type, "HashMap");
            break;
        case NdbDictionary::Object::FullyReplicatedTrigger:
            strcpy(type, "FullyRepTrigger");
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
	    case NdbDictionary::Object::StoreNotLogged:
		strcpy(store, "No");
		break;
	    case NdbDictionary::Object::StorePermanent:
		strcpy(store, "Yes");
		break;
	    default:
		sprintf(store, "%d", (int)elt.store);
		break;
	    }
	}
        char temp[100];
        if (show_temp_status)
        {
          if (! isTable)
              strcpy(temp, "-");
          else {
              switch (elt.temp) {
              case NDB_TEMP_TAB_PERMANENT:
                  strcpy(temp, "No");
                  break;
              case NDB_TEMP_TAB_TEMPORARY:
                  strcpy(temp, "Yes");
                  break;
              default:
                  sprintf(temp, "%d", (int)elt.temp);
                  break;
              }
          }
        }
	if (ndb->usingFullyQualifiedNames())
        {
          if (_parsable)
          {
            if (show_temp_status)
              ndbout_c("%d\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'", elt.id, type, state, store, temp, (elt.database)?elt.database:"", (elt.schema)?elt.schema:"", elt.name);
            else
              ndbout_c("%d\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'", elt.id, type, state, store, (elt.database)?elt.database:"", (elt.schema)?elt.schema:"", elt.name);
          }
          else
          {
            if (show_temp_status)
              ndbout_c("%-5d %-20s %-8s %-7s %-4s %-12s %-8s %s", elt.id, type, state, store, temp, (elt.database)?elt.database:"", (elt.schema)?elt.schema:"", elt.name);
            else
              ndbout_c("%-5d %-20s %-8s %-7s %-12s %-8s %s", elt.id, type, state, store, (elt.database)?elt.database:"", (elt.schema)?elt.schema:"", elt.name);
          }
        }
        else
        {
          if (_parsable)
          {
            if (show_temp_status)
              ndbout_c("%d\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'", elt.id, type, state, store, temp, elt.name);
            else
              ndbout_c("%d\t'%s'\t'%s'\t'%s'\t'%s'", elt.id, type, state, store, elt.name);
          }
          else
          {
            if (show_temp_status)
              ndbout_c("%-5d %-20s %-8s %-7s %-4s %s", elt.id, type, state, store, temp, elt.name);
            else
              ndbout_c("%-5d %-20s %-8s %-7s %s", elt.id, type, state, store, elt.name);
          }
        }
    }
    if (_parsable)
      exit(0);
}

static const char* _dbname = 0;
static const char* _tabname = 0;
static int _loops;
static int _type;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_show_tables"),
  { "database", 'd', "Name of database table is in. Requires table-name in argument",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "loops", 'l', "loops",
    (uchar**) &_loops, (uchar**) &_loops, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 }, 
  { "type", 't', "type",
    (uchar**) &_type, (uchar**) &_type, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "fully-qualified", 'f', "Show fully qualified table names",
    (uchar**) &_fully_qualified, (uchar**) &_fully_qualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "parsable", 'p', "Return output suitable for mysql LOAD DATA INFILE",
    (uchar**) &_parsable, (uchar**) &_parsable, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "show-temp-status", NDB_OPT_NOSHORT, "Show table temporary flag",
    (uchar**) &show_temp_status, (uchar**) &show_temp_status, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[table-name]");
}

int main(int argc, char** argv){
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub);
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_show_tables.trace";
#endif
  bool using_default_database = false;
  if (opts.handle_options())
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if(_dbname && argc==0) {
    ndbout << "-d option given without table name." << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if (argc>0)
      _tabname = argv[0];
  if (argc > 1) {
    ndbout << "Wrong Argument" << endl;
    ndbout << "Please use the option --help for usage." << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  ndb_cluster_connection = new Ndb_cluster_connection(opt_ndb_connectstring,
                                                      opt_ndb_nodeid);
  if (ndb_cluster_connection == NULL)
    fatal("Unable to create cluster connection");

  ndb_cluster_connection->set_name("ndb_show_tables");
  if (ndb_cluster_connection->connect(opt_connect_retries - 1, opt_connect_retry_delay, 1))
    fatal("Unable to connect to management server.\n - Error: '%d: %s'",
          ndb_cluster_connection->get_latest_error(),
          ndb_cluster_connection->get_latest_error_msg());
  if (ndb_cluster_connection->wait_until_ready(30,0) < 0)
    fatal("Cluster nodes not ready in 30 seconds.");

  ndb = new Ndb(ndb_cluster_connection);
  if (ndb->init() != 0)
    fatal("init");
  if (_dbname == 0 && _tabname != 0)
  {
    _dbname = "TEST_DB";
    using_default_database = true;
  }
  ndb->setDatabaseName(_dbname);
  dic = ndb->getDictionary();
  if( argc >0){
    if(!dic->getTable(_tabname)){
      if( using_default_database )
      {
        ndbout << "Please specify database name using the -d option. "
               << "Use option --help for more details." << endl;
      }
      else
      {
        ndbout << "Table " << _tabname << ": not found - "
               << dic->getNdbError() << endl;
      }
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  }
  for (int i = 0; _loops == 0 || i < _loops; i++) {
    list(_tabname, (NdbDictionary::Object::Type)_type);
  }
  delete ndb;
  delete ndb_cluster_connection;
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=4:
