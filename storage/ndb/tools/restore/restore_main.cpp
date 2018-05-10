/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <Logger.hpp>
#include <NDBT_ReturnCodes.h>
#include <NdbOut.hpp>
#include <NdbTCP.h>
#include <OutputStream.hpp>
#include <Properties.hpp>
#include <Vector.hpp>
#include <ndb_global.h>
#include <ndb_limits.h>
#include <ndb_opts.h>

#include "../src/ndbapi/NdbDictionaryImpl.hpp"
#include "consumer_printer.hpp"
#include "consumer_restore.hpp"
#include "my_alloc.h"

#define TMP_TABLE_PREFIX "#sql"
#define TMP_TABLE_PREFIX_LEN 4

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static Uint32 g_tableCompabilityMask = 0;
static int ga_nodeId = 0;
static int ga_nParallelism = 128;
static int ga_backupId = 0;
bool ga_dont_ignore_systab_0 = false;
static bool ga_no_upgrade = false;
static bool ga_promote_attributes = false;
static bool ga_demote_attributes = false;
static Vector<class BackupConsumer *> g_consumers;
static BackupPrinter* g_printer = NULL;

static const char* default_backupPath = "." DIR_SEPARATOR;
static const char* ga_backupPath = default_backupPath;

static const char *opt_nodegroup_map_str= 0;
static unsigned opt_nodegroup_map_len= 0;
static NODE_GROUP_MAP opt_nodegroup_map[MAX_NODE_GROUP_MAPS];
#define OPT_NDB_NODEGROUP_MAP 'z'

const char *opt_ndb_database= NULL;
const char *opt_ndb_table= NULL;
unsigned int opt_verbose;
unsigned int opt_hex_format;
unsigned int opt_progress_frequency;
NDB_TICKS g_report_prev;
Vector<BaseString> g_databases;
Vector<BaseString> g_tables;
Vector<BaseString> g_include_tables, g_exclude_tables;
Vector<BaseString> g_include_databases, g_exclude_databases;
Properties g_rewrite_databases;
NdbRecordPrintFormat g_ndbrecord_print_format;
unsigned int opt_no_binlog;

class RestoreOption
{
public:
  virtual ~RestoreOption() { }
  int optid;
  BaseString argument;
};

Vector<class RestoreOption *> g_include_exclude;
static void save_include_exclude(int optid, char * argument);

static inline void parse_rewrite_database(char * argument);

/**
 * print and restore flags
 */
static bool ga_restore_epoch = false;
static bool ga_restore = false;
static bool ga_print = false;
static bool ga_skip_table_check = false;
static bool ga_exclude_missing_columns = false;
static bool ga_exclude_missing_tables = false;
static bool opt_exclude_intermediate_sql_tables = true;
#ifdef ERROR_INSERT 
static unsigned int _error_insert = 0;
#endif
static int _print = 0;
static int _print_meta = 0;
static int _print_data = 0;
static int _print_log = 0;
static int _print_sql_log = 0;
static int _restore_data = 0;
static int _restore_meta = 0;
static int _no_restore_disk = 0;
static bool _preserve_trailing_spaces = false;
static bool ga_disable_indexes = false;
static bool ga_rebuild_indexes = false;
bool ga_skip_unknown_objects = false;
bool ga_skip_broken_objects = false;
BaseString g_options("ndb_restore");

const char *load_default_groups[]= { "mysql_cluster","ndb_restore",0 };

enum ndb_restore_options {
  OPT_VERBOSE = NDB_STD_OPTIONS_LAST,
  OPT_INCLUDE_TABLES,
  OPT_EXCLUDE_TABLES,
  OPT_INCLUDE_DATABASES,
  OPT_EXCLUDE_DATABASES,
  OPT_REWRITE_DATABASE
#ifdef ERROR_INSERT
  ,OPT_ERROR_INSERT
#endif
};
static const char *opt_fields_enclosed_by= NULL;
static const char *opt_fields_terminated_by= NULL;
static const char *opt_fields_optionally_enclosed_by= NULL;
static const char *opt_lines_terminated_by= NULL;

static const char *tab_path= NULL;
static int opt_append;
static const char *opt_exclude_tables= NULL;
static const char *opt_include_tables= NULL;
static const char *opt_exclude_databases= NULL;
static const char *opt_include_databases= NULL;
static const char *opt_rewrite_database= NULL;
static bool opt_restore_privilege_tables = false;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_restore"),
  { "connect", 'c', "same as --connect-string",
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodeid", 'n', "Backup files from node with id",
    (uchar**) &ga_nodeId, (uchar**) &ga_nodeId, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "backupid", 'b', "Backup id",
    (uchar**) &ga_backupId, (uchar**) &ga_backupId, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_data", 'r', 
    "Restore table data/logs into NDB Cluster using NDBAPI", 
    (uchar**) &_restore_data, (uchar**) &_restore_data,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_meta", 'm',
    "Restore meta data into NDB Cluster using NDBAPI",
    (uchar**) &_restore_meta, (uchar**) &_restore_meta,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-upgrade", 'u',
    "Don't upgrade array type for var attributes, which don't resize VAR data and don't change column attributes",
    (uchar**) &ga_no_upgrade, (uchar**) &ga_no_upgrade, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "promote-attributes", 'A',
    "Allow attributes to be promoted when restoring data from backup",
    (uchar**) &ga_promote_attributes, (uchar**) &ga_promote_attributes, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "lossy-conversions", 'L',
    "Allow lossy conversions for attributes (type demotions or integral"
    " signed/unsigned type changes) when restoring data from backup",
    (uchar**) &ga_demote_attributes, (uchar**) &ga_demote_attributes, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "preserve-trailing-spaces", 'P',
    "Allow to preserve the tailing spaces (including paddings) When char->varchar or binary->varbinary is promoted",
    (uchar**) &_preserve_trailing_spaces, (uchar**)_preserve_trailing_spaces , 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-restore-disk-objects", 'd',
    "Dont restore disk objects (tablespace/logfilegroups etc)",
    (uchar**) &_no_restore_disk, (uchar**) &_no_restore_disk,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_epoch", 'e', 
    "Restore epoch info into the status table. Convenient on a MySQL Cluster "
    "replication slave, for starting replication. The row in "
    NDB_REP_DB "." NDB_APPLY_TABLE " with id 0 will be updated/inserted.", 
    (uchar**) &ga_restore_epoch, (uchar**) &ga_restore_epoch,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-table-check", 's', "Skip table structure check during restore of data",
   (uchar**) &ga_skip_table_check, (uchar**) &ga_skip_table_check, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "parallelism", 'p',
    "No of parallel transactions during restore of data."
    "(parallelism can be 1 to 1024)", 
    (uchar**) &ga_nParallelism, (uchar**) &ga_nParallelism, 0,
    GET_INT, REQUIRED_ARG, 128, 1, 1024, 0, 1, 0 },
  { "print", NDB_OPT_NOSHORT, "Print metadata, data and log to stdout",
    (uchar**) &_print, (uchar**) &_print, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_data", NDB_OPT_NOSHORT, "Print data to stdout",
    (uchar**) &_print_data, (uchar**) &_print_data, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_meta", NDB_OPT_NOSHORT, "Print meta data to stdout",
    (uchar**) &_print_meta, (uchar**) &_print_meta,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_log", NDB_OPT_NOSHORT, "Print log to stdout",
    (uchar**) &_print_log, (uchar**) &_print_log,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_sql_log", NDB_OPT_NOSHORT, "Print SQL log to stdout",
    (uchar**) &_print_sql_log, (uchar**) &_print_sql_log,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "backup_path", NDB_OPT_NOSHORT, "Path to backup files",
    (uchar**) &ga_backupPath, (uchar**) &ga_backupPath, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "dont_ignore_systab_0", 'f',
    "Do not ignore system table during --print-data.", 
    (uchar**) &ga_dont_ignore_systab_0, (uchar**) &ga_dont_ignore_systab_0, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "ndb-nodegroup-map", OPT_NDB_NODEGROUP_MAP,
    "Nodegroup map for ndbcluster. Syntax: list of (source_ng, dest_ng)",
    (uchar**) &opt_nodegroup_map_str,
    (uchar**) &opt_nodegroup_map_str,
    0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-enclosed-by", NDB_OPT_NOSHORT,
    "Fields are enclosed by ...",
    (uchar**) &opt_fields_enclosed_by, (uchar**) &opt_fields_enclosed_by, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-terminated-by", NDB_OPT_NOSHORT,
    "Fields are terminated by ...",
    (uchar**) &opt_fields_terminated_by,
    (uchar**) &opt_fields_terminated_by, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-optionally-enclosed-by", NDB_OPT_NOSHORT,
    "Fields are optionally enclosed by ...",
    (uchar**) &opt_fields_optionally_enclosed_by,
    (uchar**) &opt_fields_optionally_enclosed_by, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "hex", NDB_OPT_NOSHORT, "print binary types in hex format", 
    (uchar**) &opt_hex_format, (uchar**) &opt_hex_format, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "tab", 'T', "Creates tab separated textfile for each table to "
    "given path. (creates .txt files)",
   (uchar**) &tab_path, (uchar**) &tab_path, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "append", NDB_OPT_NOSHORT, "for --tab append data to file", 
    (uchar**) &opt_append, (uchar**) &opt_append, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "lines-terminated-by", NDB_OPT_NOSHORT, "",
    (uchar**) &opt_lines_terminated_by, (uchar**) &opt_lines_terminated_by, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "progress-frequency", NDB_OPT_NOSHORT,
    "Print status uf restore periodically in given seconds", 
    (uchar**) &opt_progress_frequency, (uchar**) &opt_progress_frequency, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 65535, 0, 0, 0 },
  { "no-binlog", NDB_OPT_NOSHORT,
    "If a mysqld is connected and has binary log, do not log the restored data", 
    (uchar**) &opt_no_binlog, (uchar**) &opt_no_binlog, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", OPT_VERBOSE,
    "verbosity", 
    (uchar**) &opt_verbose, (uchar**) &opt_verbose, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 255, 0, 0, 0 },
  { "include-databases", OPT_INCLUDE_DATABASES,
    "Comma separated list of databases to restore. Example: db1,db3",
    (uchar**) &opt_include_databases, (uchar**) &opt_include_databases, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-databases", OPT_EXCLUDE_DATABASES,
    "Comma separated list of databases to not restore. Example: db1,db3",
    (uchar**) &opt_exclude_databases, (uchar**) &opt_exclude_databases, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "rewrite-database", OPT_REWRITE_DATABASE,
    "A pair 'source,dest' of database names from/into which to restore. "
    "Example: --rewrite-database=oldDb,newDb",
    (uchar**) &opt_rewrite_database, (uchar**) &opt_rewrite_database, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "include-tables", OPT_INCLUDE_TABLES, "Comma separated list of tables to "
    "restore. Table name should include database name. Example: db1.t1,db3.t1", 
    (uchar**) &opt_include_tables, (uchar**) &opt_include_tables, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-tables", OPT_EXCLUDE_TABLES, "Comma separated list of tables to "
    "not restore. Table name should include database name. "
    "Example: db1.t1,db3.t1",
    (uchar**) &opt_exclude_tables, (uchar**) &opt_exclude_tables, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore-privilege-tables", NDB_OPT_NOSHORT,
    "Restore privilege tables (after they have been moved to ndb)",
    (uchar**) &opt_restore_privilege_tables,
    (uchar**) &opt_restore_privilege_tables, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-missing-columns", NDB_OPT_NOSHORT,
    "Ignore columns present in backup but not in database",
    (uchar**) &ga_exclude_missing_columns,
    (uchar**) &ga_exclude_missing_columns, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-missing-tables", NDB_OPT_NOSHORT,
    "Ignore tables present in backup but not in database",
    (uchar**) &ga_exclude_missing_tables,
    (uchar**) &ga_exclude_missing_tables, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-intermediate-sql-tables", NDB_OPT_NOSHORT,
    "Do not restore intermediate tables with #sql-prefixed names",
    (uchar**) &opt_exclude_intermediate_sql_tables,
    (uchar**) &opt_exclude_intermediate_sql_tables, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "disable-indexes", NDB_OPT_NOSHORT,
    "Disable indexes and foreign keys",
    (uchar**) &ga_disable_indexes,
    (uchar**) &ga_disable_indexes, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "rebuild-indexes", NDB_OPT_NOSHORT,
    "Rebuild indexes",
    (uchar**) &ga_rebuild_indexes,
    (uchar**) &ga_rebuild_indexes, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-unknown-objects", 256, "Skip unknown object when parsing backup",
    (uchar**) &ga_skip_unknown_objects, (uchar**) &ga_skip_unknown_objects, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-broken-objects", 256, "Skip broken object when parsing backup",
    (uchar**) &ga_skip_broken_objects, (uchar**) &ga_skip_broken_objects, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
#ifdef ERROR_INSERT
  { "error-insert", OPT_ERROR_INSERT,
    "Insert errors (testing option)",
    (uchar **)&_error_insert, (uchar **)&_error_insert, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static char* analyse_one_map(char *map_str, uint16 *source, uint16 *dest)
{
  char *end_ptr;
  int number;
  DBUG_ENTER("analyse_one_map");
  /*
    Search for pattern ( source_ng , dest_ng )
  */

  while (isspace(*map_str)) map_str++;

  if (*map_str != '(')
  {
    DBUG_RETURN(NULL);
  }
  map_str++;

  while (isspace(*map_str)) map_str++;

  number= strtol(map_str, &end_ptr, 10);
  if (!end_ptr || number < 0 || number >= MAX_NODE_GROUP_MAPS)
  {
    DBUG_RETURN(NULL);
  }
  *source= (uint16)number;
  map_str= end_ptr;

  while (isspace(*map_str)) map_str++;

  if (*map_str != ',')
  {
    DBUG_RETURN(NULL);
  }
  map_str++;

  number= strtol(map_str, &end_ptr, 10);
  if (!end_ptr || number < 0 || number >= NDB_UNDEF_NODEGROUP)
  {
    DBUG_RETURN(NULL);
  }
  *dest= (uint16)number;
  map_str= end_ptr;

  if (*map_str != ')')
  {
    DBUG_RETURN(NULL);
  }
  map_str++;

  while (isspace(*map_str)) map_str++;
  DBUG_RETURN(map_str);
}

static bool insert_ng_map(NODE_GROUP_MAP *ng_map,
                          uint16 source_ng, uint16 dest_ng)
{
  uint index= source_ng;
  uint ng_index= ng_map[index].no_maps;

  opt_nodegroup_map_len++;
  if (ng_index >= MAX_MAPS_PER_NODE_GROUP)
    return true;
  ng_map[index].no_maps++;
  ng_map[index].map_array[ng_index]= dest_ng;
  return false;
}

static void init_nodegroup_map()
{
  uint i,j;
  NODE_GROUP_MAP *ng_map = &opt_nodegroup_map[0];

  for (i = 0; i < MAX_NODE_GROUP_MAPS; i++)
  {
    ng_map[i].no_maps= 0;
    for (j= 0; j < MAX_MAPS_PER_NODE_GROUP; j++)
      ng_map[i].map_array[j]= NDB_UNDEF_NODEGROUP;
  }
}

static bool analyse_nodegroup_map(const char *ng_map_str,
                                  NODE_GROUP_MAP *ng_map)
{
  uint16 source_ng, dest_ng;
  char *local_str= (char*)ng_map_str;
  DBUG_ENTER("analyse_nodegroup_map");

  do
  {
    if (!local_str)
    {
      DBUG_RETURN(TRUE);
    }
    local_str= analyse_one_map(local_str, &source_ng, &dest_ng);
    if (!local_str)
    {
      DBUG_RETURN(TRUE);
    }
    if (insert_ng_map(ng_map, source_ng, dest_ng))
    {
      DBUG_RETURN(TRUE);
    }
    if (!(*local_str))
      break;
  } while (TRUE);
  DBUG_RETURN(FALSE);
}

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[<path to backup files>]");
}

static bool
get_one_option(int optid, const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument)
{
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_restore.trace";
#endif
  ndb_std_get_one_option(optid, opt, argument);
  switch (optid) {
  case OPT_VERBOSE:
    info.setThreshold(255-opt_verbose);
    break;
  case 'n':
    if (ga_nodeId == 0)
    {
      err << "Error in --nodeid,-n setting, see --help";
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    info.setLevel(254);
    info << "Nodeid = " << ga_nodeId << endl;
    break;
  case 'b':
    if (ga_backupId == 0)
    {
      err << "Error in --backupid,-b setting, see --help";
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    info.setLevel(254);
    info << "Backup Id = " << ga_backupId << endl;
    break;
  case OPT_NDB_NODEGROUP_MAP:
    /*
      This option is used to set a map from nodegroup in original cluster
      to nodegroup in new cluster.
    */
    opt_nodegroup_map_len= 0;

    info.setLevel(254);
    info << "Analyse node group map" << endl;
    if (analyse_nodegroup_map(opt_nodegroup_map_str,
                              &opt_nodegroup_map[0]))
    {
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    break;
  case OPT_INCLUDE_DATABASES:
  case OPT_EXCLUDE_DATABASES:
  case OPT_INCLUDE_TABLES:
  case OPT_EXCLUDE_TABLES:
    save_include_exclude(optid, argument);
    break;
  case OPT_REWRITE_DATABASE:
    parse_rewrite_database(argument);
    break;
  }
  return 0;
}

static const char* SCHEMA_NAME="/def/";
static const int SCHEMA_NAME_SIZE= 5;

int
makeInternalTableName(const BaseString &externalName, 
                      BaseString& internalName)
{
  // Make dbname.table1 into dbname/def/table1
  Vector<BaseString> parts;

  // Must contain a dot
  if (externalName.indexOf('.') == -1)
    return -1;
  externalName.split(parts,".");
  // .. and only 1 dot
  if (parts.size() != 2)
    return -1;
  internalName.clear();
  internalName.append(parts[0]); // db name
  internalName.append(SCHEMA_NAME); // /def/
  internalName.append(parts[1]); // table name
  return 0;
}

void
processTableList(const char* str, Vector<BaseString> &lst)
{
  // Process tables list like db1.t1,db2.t1 and exits when
  // it finds problems.
  Vector<BaseString> tmp;
  unsigned int i;
  /* Split passed string on comma into 2 BaseStrings in the vector */
  BaseString(str).split(tmp,",");
  for (i=0; i < tmp.size(); i++)
  {
    BaseString internalName;
    if (makeInternalTableName(tmp[i], internalName))
    {
      info << "`" << tmp[i] << "` is not a valid tablename!" << endl;
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    lst.push_back(internalName);
  }
}

BaseString
makeExternalTableName(const BaseString &internalName)
{
   // Make dbname/def/table1 into dbname.table1
  BaseString externalName;
  
  ssize_t idx = internalName.indexOf('/');
  externalName = internalName.substr(0,idx);
  externalName.append(".");
  externalName.append(internalName.substr(idx + SCHEMA_NAME_SIZE,
                                          internalName.length()));
  return externalName;
}

#include "sql/ndb_dist_priv_util.h"

// Exclude privilege tables unless explicitely included
void
exclude_privilege_tables()
{
  const char* table_name;
  Ndb_dist_priv_util dist_priv;
  while((table_name= dist_priv.iter_next_table()))
  {
    BaseString priv_tab;
    priv_tab.assfmt("%s.%s", dist_priv.database(), table_name);
    g_exclude_tables.push_back(priv_tab);
    save_include_exclude(OPT_EXCLUDE_TABLES, (char *)priv_tab.c_str());
  }
}


bool
readArguments(Ndb_opts & opts, char*** pargv)
{
  Uint32 i;
  BaseString tmp;
  debug << "Load defaults" << endl;

  init_nodegroup_map();
  debug << "handle_options" << endl;

  opts.set_usage_funcs(short_usage_sub);

  if (opts.handle_options(get_one_option))
  {
    exit(NDBT_ProgramExit(NDBT_WRONGARGS));
  }
  for (i = 0; i < MAX_NODE_GROUP_MAPS; i++)
    opt_nodegroup_map[i].curr_index = 0;

#if 0
  /*
    Test code written t{
o verify nodegroup mapping
  */
  printf("Handled options successfully\n");
  Uint16 map_ng[16];
  Uint32 j;
  for (j = 0; j < 4; j++)
  {
  for (i = 0; i < 4 ; i++)
    map_ng[i] = i;
  map_nodegroups(&map_ng[0], (Uint32)4);
  for (i = 0; i < 4 ; i++)
    printf("NG %u mapped to %u \n", i, map_ng[i]);
  }
  for (j = 0; j < 4; j++)
  {
  for (i = 0; i < 8 ; i++)
    map_ng[i] = i >> 1;
  map_nodegroups(&map_ng[0], (Uint32)8);
  for (i = 0; i < 8 ; i++)
    printf("NG %u mapped to %u \n", i >> 1, map_ng[i]);
  }
  exit(NDBT_ProgramExit(NDBT_WRONGARGS));
#endif

  g_printer = new BackupPrinter(opt_nodegroup_map,
                                opt_nodegroup_map_len);
  if (g_printer == NULL)
    return false;

  BackupRestore* restore = new BackupRestore(opt_ndb_connectstring,
                                             opt_ndb_nodeid,
                                             opt_nodegroup_map,
                                             opt_nodegroup_map_len,
                                             ga_nodeId,
                                             ga_nParallelism,
                                             opt_connect_retry_delay,
                                             opt_connect_retries);
  if (restore == NULL) 
  {
    delete g_printer;
    g_printer = NULL;
    return false;
  }

  if (_print) 
  {
    ga_print = true;
    ga_restore = true;
    g_printer->m_print = true;
  } 
  if (_print_meta) 
  {
    ga_print = true;
    g_printer->m_print_meta = true;
  }
  if (_print_data) 
  {
    ga_print = true;
    g_printer->m_print_data = true;
  }
  if (_print_log) 
  {
    ga_print = true;
    g_printer->m_print_log = true;
  }
  if (_print_sql_log)
    {
      ga_print = true;
      g_printer->m_print_sql_log = true;
    }

  if (_restore_data)
  {
    ga_restore = true;
    restore->m_restore = true; 
  }

  if (_restore_meta)
  {
    //    ga_restore = true;
    restore->m_restore_meta = true;
    if(ga_exclude_missing_tables)
    {
      //conflict in options
      err << "Conflicting arguments found : "
          << "Cannot use `restore-meta` and "
          << "`exclude-missing-tables` together. Exiting..." << endl;
      return false;
    }
  }

  if (_no_restore_disk)
  {
    restore->m_no_restore_disk = true;
  }
  
  if (ga_no_upgrade)
  {
     restore->m_no_upgrade = true;
  }

  if (_preserve_trailing_spaces)
  {
     restore->m_preserve_trailing_spaces = true;
  }

  if (ga_restore_epoch)
  {
    restore->m_restore_epoch = true;
  }

  if (ga_disable_indexes)
  {
    restore->m_disable_indexes = true;
  }

  if (ga_rebuild_indexes)
  {
    restore->m_rebuild_indexes = true;
  }

  {
    BackupConsumer * c = g_printer;
    g_consumers.push_back(c);
  }
  {
    BackupConsumer * c = restore;
    g_consumers.push_back(c);
  }
  for (;;)
  {
    int i= 0;
    if (ga_backupPath == default_backupPath)
    {
      // Set backup file path
      if ((*pargv)[i] == NULL)
        break;
      ga_backupPath = (*pargv)[i++];
    }
    if ((*pargv)[i] == NULL)
      break;
    g_databases.push_back((*pargv)[i++]);
    while ((*pargv)[i] != NULL)
    {
      g_tables.push_back((*pargv)[i++]);
    }
    break;
  }
  info.setLevel(254);
  info << "backup path = " << ga_backupPath << endl;
  if (g_databases.size() > 0)
  {
    info << "WARNING! Using deprecated syntax for selective object restoration." << endl;
    info << "Please use --include-*/--exclude-* options in future." << endl;
    info << "Restoring only from database " << g_databases[0].c_str() << endl;
    if (g_tables.size() > 0)
    {
        info << "Restoring tables:";
    }
    for (unsigned i= 0; i < g_tables.size(); i++)
    {
      info << " " << g_tables[i].c_str();
    }
    if (g_tables.size() > 0)
      info << endl;
  }

  if (ga_restore)
  {
    // Exclude privilege tables unless explicitely included
    if (!opt_restore_privilege_tables)
      exclude_privilege_tables();

    // Move over old style arguments to include/exclude lists
    if (g_databases.size() > 0)
    {
      BaseString tab_prefix, tab;
      tab_prefix.append(g_databases[0].c_str());
      tab_prefix.append(".");
      if (g_tables.size() == 0)
      {
        g_include_databases.push_back(g_databases[0]);
        save_include_exclude(OPT_INCLUDE_DATABASES,
                             (char *)g_databases[0].c_str());
      }
      for (unsigned i= 0; i < g_tables.size(); i++)
      {
        tab.assign(tab_prefix);
        tab.append(g_tables[i]);
        g_include_tables.push_back(tab);
        save_include_exclude(OPT_INCLUDE_TABLES, (char *)tab.c_str());
      }
    }
  }

  if (opt_include_databases)
  {
    tmp = BaseString(opt_include_databases);
    tmp.split(g_include_databases,",");
    info << "Including Databases: ";
    for (i= 0; i < g_include_databases.size(); i++)
    {
      info << g_include_databases[i] << " ";
    }
    info << endl;
  }
  
  if (opt_exclude_databases)
  {
    tmp = BaseString(opt_exclude_databases);
    tmp.split(g_exclude_databases,",");
    info << "Excluding databases: ";
    for (i= 0; i < g_exclude_databases.size(); i++)
    {
      info << g_exclude_databases[i] << " ";
    }
    info << endl;
  }
  
  if (opt_rewrite_database)
  {
    info << "Rewriting databases:";
    Properties::Iterator it(&g_rewrite_databases);
    const char * src;
    for (src = it.first(); src != NULL; src = it.next()) {
      const char * dst = NULL;
      bool r = g_rewrite_databases.get(src, &dst);
      require(r && (dst != NULL));
      info << " (" << src << "->" << dst << ")";
    }
    info << endl;
  }

  if (opt_include_tables)
  {
    processTableList(opt_include_tables, g_include_tables);
    info << "Including tables: ";
    for (i= 0; i < g_include_tables.size(); i++)
    {
      info << makeExternalTableName(g_include_tables[i]).c_str() << " ";
    }
    info << endl;
  }
  
  if (opt_exclude_tables)
  {
    processTableList(opt_exclude_tables, g_exclude_tables);
    info << "Excluding tables: ";
    for (i= 0; i < g_exclude_tables.size(); i++)
    {
      info << makeExternalTableName(g_exclude_tables[i]).c_str() << " ";
    }
    info << endl;
  }

  /*
    the below formatting follows the formatting from mysqldump
    do not change unless to adopt to changes in mysqldump
  */
  g_ndbrecord_print_format.fields_enclosed_by=
    opt_fields_enclosed_by ? opt_fields_enclosed_by : "";
  g_ndbrecord_print_format.fields_terminated_by=
    opt_fields_terminated_by ? opt_fields_terminated_by : "\t";
  g_ndbrecord_print_format.fields_optionally_enclosed_by=
    opt_fields_optionally_enclosed_by ? opt_fields_optionally_enclosed_by : "";
  g_ndbrecord_print_format.lines_terminated_by=
    opt_lines_terminated_by ? opt_lines_terminated_by : "\n";
  if (g_ndbrecord_print_format.fields_optionally_enclosed_by[0] == '\0')
    g_ndbrecord_print_format.null_string= "\\N";
  else
    g_ndbrecord_print_format.null_string= "";
  g_ndbrecord_print_format.hex_prefix= "";
  g_ndbrecord_print_format.hex_format= opt_hex_format;

  if (ga_skip_table_check)
  {
    g_tableCompabilityMask = ~(Uint32)0;
    ga_skip_unknown_objects = true;
  }

  if (ga_promote_attributes)
  {
    g_tableCompabilityMask |= TCM_ATTRIBUTE_PROMOTION;
  }

  if (ga_demote_attributes)
  {
    g_tableCompabilityMask |= TCM_ATTRIBUTE_DEMOTION;
  }

  if (ga_exclude_missing_columns)
  {
    g_tableCompabilityMask |= TCM_EXCLUDE_MISSING_COLUMNS;
  }
  return true;
}

void
clearConsumers()
{
  for(Uint32 i= 0; i<g_consumers.size(); i++)
    delete g_consumers[i];
  g_consumers.clear();
}

static inline bool
checkSysTable(const TableS* table)
{
  return ! table->getSysTable();
}

static inline bool
checkSysTable(const RestoreMetaData& metaData, uint i)
{
  assert(i < metaData.getNoOfTables());
  return checkSysTable(metaData[i]);
}

static inline bool
isBlobTable(const TableS* table)
{
  return table->getMainTable() != NULL;
}

static inline bool
isIndex(const TableS* table)
{
  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* table->m_dictTable);
  return (int) tmptab.m_indexType != (int) NdbDictionary::Index::Undefined;
}

static inline bool
isSYSTAB_0(const TableS* table)
{
  return table->isSYSTAB_0();
}

const char*
getTableName(const TableS* table)
{
  const char *table_name;
  if (isBlobTable(table))
    table_name= table->getMainTable()->getTableName();
  else if (isIndex(table))
    table_name=
      NdbTableImpl::getImpl(*table->m_dictTable).m_primaryTable.c_str();
  else
    table_name= table->getTableName();
    
  return table_name;
}

static void parse_rewrite_database(char * argument)
{
  const BaseString arg(argument);
  Vector<BaseString> args;
  unsigned int n = arg.split(args, ",");
  if ((n == 2)
      && (args[0].length() > 0)
      && (args[1].length() > 0)) {
    const BaseString src = args[0];
    const BaseString dst = args[1];
    const bool replace = true;
    bool r = g_rewrite_databases.put(src.c_str(), dst.c_str(), replace);
    require(r);
    return; // ok
  }

  info << "argument `" << arg.c_str()
       << "` is not a pair 'a,b' of non-empty names." << endl;
  exit(NDBT_ProgramExit(NDBT_WRONGARGS));
}

static void save_include_exclude(int optid, char * argument)
{
  BaseString arg = argument;
  Vector<BaseString> args;
  arg.split(args, ",");
  for (uint i = 0; i < args.size(); i++)
  {
    RestoreOption * option = new RestoreOption();
    BaseString arg;
    
    option->optid = optid;
    switch (optid) {
    case OPT_INCLUDE_TABLES:
    case OPT_EXCLUDE_TABLES:
      if (makeInternalTableName(args[i], arg))
      {
        info << "`" << args[i] << "` is not a valid tablename!" << endl;
        exit(NDBT_ProgramExit(NDBT_WRONGARGS));
      }
      break;
    default:
      arg = args[i];
      break;
    }
    option->argument = arg;
    g_include_exclude.push_back(option);
  }
}
static bool check_include_exclude(BaseString database, BaseString table)
{
  const char * db = database.c_str();
  const char * tbl = table.c_str();
  bool do_include = true;

  if (g_include_databases.size() != 0 ||
      g_include_tables.size() != 0)
  {
    /*
      User has explicitly specified what databases
      and/or tables should be restored. If no match is
      found then DON'T restore table.
     */
    do_include = false;
  }
  if (do_include &&
      (g_exclude_databases.size() != 0 ||
       g_exclude_tables.size() != 0))
  {
    /*
      User has not explicitly specified what databases
      and/or tables should be restored.
      User has explicitly specified what databases
      and/or tables should NOT be restored. If no match is
      found then DO restore table.
     */
    do_include = true;
  }

  if (g_include_exclude.size() != 0)
  {
    /*
      Scan include exclude arguments in reverse.
      First matching include causes table to be restored.
      first matching exclude causes table NOT to be restored.      
     */
    for(uint i = g_include_exclude.size(); i > 0; i--)
    {
      RestoreOption *option = g_include_exclude[i-1];
      switch (option->optid) {
      case OPT_INCLUDE_TABLES:
        if (strcmp(tbl, option->argument.c_str()) == 0)
          return true; // do include
        break;
      case OPT_EXCLUDE_TABLES:
        if (strcmp(tbl, option->argument.c_str()) == 0)
          return false; // don't include
        break;
      case OPT_INCLUDE_DATABASES:
        if (strcmp(db, option->argument.c_str()) == 0)
          return true; // do include
        break;
      case OPT_EXCLUDE_DATABASES:
        if (strcmp(db, option->argument.c_str()) == 0)
          return false; // don't include
        break;
      default:
        continue;
      }
    }
  }

  return do_include;
}

static bool
check_intermediate_sql_table(const char *table_name)
{
  BaseString tbl(table_name);
  Vector<BaseString> fields;
  tbl.split(fields, "/");
  if((fields.size() == 3) && !fields[2].empty() && strncmp(fields[2].c_str(), TMP_TABLE_PREFIX, TMP_TABLE_PREFIX_LEN) == 0) 
    return true;  
  return false;
}
  
static inline bool
checkDoRestore(const TableS* table)
{
  bool ret = true;
  BaseString db, tbl;
  
  tbl.assign(getTableName(table));
  ssize_t idx = tbl.indexOf('/');
  db = tbl.substr(0, idx);
  
  /*
    Include/exclude flags are evaluated right
    to left, and first match overrides any other
    matches. Non-overlapping arguments are accumulative.
    If no include flags are specified this means all databases/tables
    except any excluded are restored.
    If include flags are specified than only those databases
    or tables specified are restored.
   */
  ret = check_include_exclude(db, tbl);
  return ret;
}

static inline bool
checkDbAndTableName(const TableS* table)
{
  if (table->isBroken())
    return false;

  const char *table_name = getTableName(table);
  if(opt_exclude_intermediate_sql_tables && (check_intermediate_sql_table(table_name) == true)) {
    return false;
  }

  // If new options are given, ignore the old format
  if (opt_include_tables || g_exclude_tables.size() > 0 ||
      opt_include_databases || opt_exclude_databases ) {
    return (checkDoRestore(table));
  }
  
  if (g_tables.size() == 0 && g_databases.size() == 0)
    return true;

  if (g_databases.size() == 0)
    g_databases.push_back("TEST_DB");

  // Filter on the main table name for indexes and blobs
  unsigned i;
  for (i= 0; i < g_databases.size(); i++)
  {
    if (strncmp(table_name, g_databases[i].c_str(),
                g_databases[i].length()) == 0 &&
        table_name[g_databases[i].length()] == '/')
    {
      // we have a match
      if (g_databases.size() > 1 || g_tables.size() == 0)
        return true;
      break;
    }
  }
  if (i == g_databases.size())
    return false; // no match found

  while (*table_name != '/') table_name++;
  table_name++;
  while (*table_name != '/') table_name++;
  table_name++;

  // Check if table should be restored
  for (i= 0; i < g_tables.size(); i++)
  {
    if (strcmp(table_name, g_tables[i].c_str()) == 0)
      return true;
  }
  return false;
}

static void
exclude_missing_tables(const RestoreMetaData& metaData)
{
  Uint32 i, j;
  bool isMissing;
  Vector<BaseString> missingTables;
  for(i = 0; i < metaData.getNoOfTables(); i++)
  {
    const TableS *table= metaData[i];
    isMissing = false;
    for(j = 0; j < g_consumers.size(); j++)
      isMissing |= g_consumers[j]->isMissingTable(*table);
    if( isMissing )
    {
      /* add missing tables to exclude list */
      g_exclude_tables.push_back(table->getTableName());
      BaseString tableName = makeExternalTableName(table->getTableName());
      save_include_exclude(OPT_EXCLUDE_TABLES, (char*)tableName.c_str());
      missingTables.push_back(tableName);
    }
  }

  if(missingTables.size() > 0){
    info << "Excluded Missing tables: ";
    for (i=0 ; i < missingTables.size(); i++)
    {
      info << missingTables[i] << " ";
    }
    info << endl;
  }
}

static void
free_data_callback()
{
  for(Uint32 i= 0; i < g_consumers.size(); i++) 
    g_consumers[i]->tuple_free();
}

static void
free_include_excludes_vector()
{
  for (unsigned i = 0; i < g_include_exclude.size(); i++)
  {
    delete g_include_exclude[i];
  }
  g_include_exclude.clear();
}

static void exitHandler(int code)
{
  free_include_excludes_vector();
  NDBT_ProgramExit(code);
  if (opt_core)
    abort();
  else
    exit(code);
}

static void init_progress()
{
  g_report_prev = NdbTick_getCurrentTicks();
}

static int check_progress()
{
  if (!opt_progress_frequency)
    return 0;

  const NDB_TICKS now = NdbTick_getCurrentTicks();
  
  if (NdbTick_Elapsed(g_report_prev, now).seconds() >= opt_progress_frequency)
  {
    g_report_prev = now;
    return 1;
  }
  return 0;
}

static void report_progress(const char *prefix, const BackupFile &f)
{
  info.setLevel(255);
  if (f.get_file_size())
    info << prefix << (f.get_file_pos() * 100 + f.get_file_size()-1) / f.get_file_size() 
         << "%(" << f.get_file_pos() << " bytes)\n";
  else
    info << prefix << f.get_file_pos() << " bytes\n";
}

/**
 * Reports, clears information on columns where data truncation was detected.
 */
static void
check_data_truncations(const TableS * table)
{
  assert(table);
  const char * tname = table->getTableName();
  const int n = table->getNoOfAttributes();
  for (int i = 0; i < n; i++) {
    AttributeDesc * desc = table->getAttributeDesc(i);
    if (desc->truncation_detected) {
      const char * cname = desc->m_column->getName();
      info.setLevel(254);
      info << "Data truncation(s) detected for attribute: "
           << tname << "." << cname << endl;
      desc->truncation_detected = false;
    }
  }
}

int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster","ndb_restore",0 };
  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);

  debug << "Start readArguments" << endl;
  if (!readArguments(opts, &argv))
  {
    exitHandler(NDBT_FAILED);
  }

  g_options.appfmt(" -b %u", ga_backupId);
  g_options.appfmt(" -n %d", ga_nodeId);
  if (_restore_meta)
    g_options.appfmt(" -m");
  if (ga_no_upgrade)
    g_options.appfmt(" -u");
  if (ga_promote_attributes)
    g_options.appfmt(" -A");
  if (ga_demote_attributes)
    g_options.appfmt(" -L");
  if (_preserve_trailing_spaces)
    g_options.appfmt(" -P");
  if (ga_skip_table_check)
    g_options.appfmt(" -s");
  if (_restore_data)
    g_options.appfmt(" -r");
  if (ga_restore_epoch)
    g_options.appfmt(" -e");
  if (_no_restore_disk)
    g_options.appfmt(" -d");
  if (ga_exclude_missing_columns)
    g_options.append(" --exclude-missing-columns");
  if (ga_exclude_missing_tables)
    g_options.append(" --exclude-missing-tables");
  if (ga_disable_indexes)
    g_options.append(" --disable-indexes");
  if (ga_rebuild_indexes)
    g_options.append(" --rebuild-indexes");
  g_options.appfmt(" -p %d", ga_nParallelism);
  if (ga_skip_unknown_objects)
    g_options.append(" --skip-unknown-objects");
  if (ga_skip_broken_objects)
    g_options.append(" --skip-broken-objects");

  init_progress();

  char timestamp[64];

  /**
   * we must always load meta data, even if we will only print it to stdout
   */

  debug << "Start restoring meta data" << endl;

  RestoreMetaData metaData(ga_backupPath, ga_nodeId, ga_backupId);
#ifdef ERROR_INSERT
  if(_error_insert > 0)
  {
    metaData.error_insert(_error_insert);
  }
#endif 

  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Read meta data file header" << endl;

  if (!metaData.readHeader())
  {
    err << "Failed to read " << metaData.getFilename() << endl << endl;
    exitHandler(NDBT_FAILED);
  }

  const BackupFormat::FileHeader & tmp = metaData.getFileHeader();
  const Uint32 version = tmp.BackupVersion;
  
  char buf[NDB_VERSION_STRING_BUF_SZ];
  info.setLevel(254);
  info << "Backup version in files: " 
       <<  ndbGetVersionString(version, 0, 
                               isDrop6(version) ? "-drop6" : 0, 
                               buf, sizeof(buf));
  if (version >= NDBD_RAW_LCP)
  {
    info << " ndb version: "
         << ndbGetVersionString(tmp.NdbVersion, tmp.MySQLVersion, 0, 
                                buf, sizeof(buf));
  }

  info << endl;

  /**
   * check wheater we can restore the backup (right version).
   */
  // in these versions there was an error in how replica info was
  // stored on disk
  if (version >= MAKE_VERSION(5,1,3) && version <= MAKE_VERSION(5,1,9))
  {
    err << "Restore program incompatible with backup versions between "
        << ndbGetVersionString(MAKE_VERSION(5,1,3), 0, 0, buf, sizeof(buf))
        << " and "
        << ndbGetVersionString(MAKE_VERSION(5,1,9), 0, 0, buf, sizeof(buf))
        << endl;
    exitHandler(NDBT_FAILED);
  }

  if (version > NDB_VERSION)
  {
    err << "Restore program older than backup version. Not supported. "
        << "Use new restore program" << endl;
    exitHandler(NDBT_FAILED);
  }

  debug << "Load content" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Load content" << endl;

  int res  = metaData.loadContent();

  info << "Stop GCP of Backup: " << metaData.getStopGCP() << endl;
  
  if (res == 0)
  {
    err << "Restore: Failed to load content" << endl;
    exitHandler(NDBT_FAILED);
  }
  debug << "Get number of Tables" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Get number of Tables" << endl;
  if (metaData.getNoOfTables() == 0) 
  {
    err << "The backup contains no tables" << endl;
    exitHandler(NDBT_FAILED);
  }

  if(_print_sql_log && _print_log)
  {
    debug << "Check to ensure that both print-sql-log and print-log "
          << "options are not passed" << endl;
    err << "Both print-sql-log and print-log options passed. Exiting..."
        << endl;
    exitHandler(NDBT_FAILED);
  }

  if (_print_sql_log)
  {
    debug << "Check for tables with hidden PKs or column of type blob "
          << "when print-sql-log option is passed" << endl;
    for(Uint32 i = 0; i < metaData.getNoOfTables(); i++)
    {
      const TableS *table = metaData[i];
      if (!(checkSysTable(table) && checkDbAndTableName(table)))
        continue;
      /* Blobs are stored as separate tables with names prefixed
       * with NDB$BLOB. This can be used to check if there are
       * any columns of type blob in the tables being restored */
      BaseString tableName(table->getTableName());
      Vector<BaseString> tableNameParts;
      tableName.split(tableNameParts, "/");
      if (tableNameParts[2].substr(0,8) == "NDB$BLOB")
      {
        err << "Found column of type blob with print-sql-log option set. "
            << "Exiting..." << endl;
        exitHandler(NDBT_FAILED);
      }
      /* Hidden PKs are stored with the name $PK */
      int noOfPK = table->m_dictTable->getNoOfPrimaryKeys();
      for(int j = 0; j < noOfPK; j++)
      {
        const char* pkName = table->m_dictTable->getPrimaryKey(j);
        if(strcmp(pkName,"$PK") == 0)
        {
          err << "Found hidden primary key with print-sql-log option set. "
              << "Exiting..." << endl;
          exitHandler(NDBT_FAILED);
        }
      }
    }
  }

  debug << "Validate Footer" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Validate Footer" << endl;

  if (!metaData.validateFooter()) 
  {
    err << "Restore: Failed to validate footer." << endl;
    exitHandler(NDBT_FAILED);
  }
  debug << "Init Backup objects" << endl;
  Uint32 i;
  for(i= 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init(g_tableCompabilityMask))
    {
      clearConsumers();
      err << "Failed to initialize consumers" << endl;
      exitHandler(NDBT_FAILED);
    }

  }

  if(ga_exclude_missing_tables)
    exclude_missing_tables(metaData);

  /* report to clusterlog if applicable */
  for (i = 0; i < g_consumers.size(); i++)
    g_consumers[i]->report_started(ga_backupId, ga_nodeId);

  debug << "Restore objects (tablespaces, ..)" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Restore objects (tablespaces, ..)" << endl;
  for(i = 0; i<metaData.getNoOfObjects(); i++)
  {
    for(Uint32 j= 0; j < g_consumers.size(); j++)
      if (!g_consumers[j]->object(metaData.getObjType(i),
				  metaData.getObjPtr(i)))
      {
	err << "Restore: Failed to restore table: ";
        err << metaData[i]->getTableName() << " ... Exiting " << endl;
	exitHandler(NDBT_FAILED);
      } 
    if (check_progress())
    {
      info.setLevel(255);
      info << "Object create progress: "
           << i+1 << " objects out of "
           << metaData.getNoOfObjects() << endl;
    }
  }

  Vector<OutputStream *> table_output(metaData.getNoOfTables());
  debug << "Restoring tables" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Restoring tables" << endl;

  for(i = 0; i<metaData.getNoOfTables(); i++)
  {
    const TableS *table= metaData[i];
    table_output.push_back(NULL);
    if (!checkDbAndTableName(table))
      continue;
    if (isSYSTAB_0(table))
    {
      table_output[i]= ndbout.m_out;
    }
    if (checkSysTable(table))
    {
      if (!tab_path || isBlobTable(table) || isIndex(table))
      {
        table_output[i]= ndbout.m_out;
      }
      else
      {
        FILE* res;
        char filename[FN_REFLEN], tmp_path[FN_REFLEN];
        const char *table_name;
        table_name= table->getTableName();
        while (*table_name != '/') table_name++;
        table_name++;
        while (*table_name != '/') table_name++;
        table_name++;
        convert_dirname(tmp_path, tab_path, NullS);
        res= my_fopen(fn_format(filename, table_name, tmp_path, ".txt", 4),
                      opt_append ?
                      O_WRONLY|O_APPEND|O_CREAT :
                      O_WRONLY|O_TRUNC|O_CREAT,
                      MYF(MY_WME));
        if (res == 0)
        {
          exitHandler(NDBT_FAILED);
        }
        FileOutputStream *f= new FileOutputStream(res);
        table_output[i]= f;
      }
      for(Uint32 j= 0; j < g_consumers.size(); j++)
	if (!g_consumers[j]->table(* table))
	{
	  err << "Restore: Failed to restore table: `";
          err << table->getTableName() << "` ... Exiting " << endl;
	  exitHandler(NDBT_FAILED);
	} 
    } else {
      for(Uint32 j= 0; j < g_consumers.size(); j++)
        if (!g_consumers[j]->createSystable(* table))
        {
          err << "Restore: Failed to restore system table: ";
          err << table->getTableName() << " ... Exiting " << endl;
          exitHandler(NDBT_FAILED);
        }
    }
    if (check_progress())
    {
      info.setLevel(255);
      info << "Table create progress: "
           << i+1 << " tables out of "
           << metaData.getNoOfTables() << endl;
    }
  }

  debug << "Save foreign key info" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_metadata]" << " Save foreign key info" << endl;
  for(i = 0; i<metaData.getNoOfObjects(); i++)
  {
    for(Uint32 j= 0; j < g_consumers.size(); j++)
      if (!g_consumers[j]->fk(metaData.getObjType(i),
			      metaData.getObjPtr(i)))
      {
        exitHandler(NDBT_FAILED);
      } 
  }

  debug << "Close tables" << endl; 
  for(i= 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->endOfTables())
    {
      err << "Restore: Failed while closing tables" << endl;
      exitHandler(NDBT_FAILED);
    } 
    if (!ga_disable_indexes && !ga_rebuild_indexes)
    {
      if (!g_consumers[i]->endOfTablesFK())
      {
        err << "Restore: Failed while closing tables FKs" << endl;
        exitHandler(NDBT_FAILED);
      } 
    }
  }
  /* report to clusterlog if applicable */
  for(i= 0; i < g_consumers.size(); i++)
  {
    g_consumers[i]->report_meta_data(ga_backupId, ga_nodeId);
  }
  debug << "Iterate over data" << endl;
  Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
  info << timestamp << " [restore_data]" << " Start restoring table data" << endl;
  if (ga_restore || ga_print) 
  {
    if(_restore_data || _print_data)
    {
      // Check table compatibility
      for (i=0; i < metaData.getNoOfTables(); i++){
        if (checkSysTable(metaData, i) &&
            checkDbAndTableName(metaData[i]))
        {
          TableS & tableS = *metaData[i]; // not const
          for(Uint32 j= 0; j < g_consumers.size(); j++)
          {
            if (!g_consumers[j]->table_compatible_check(tableS))
            {
              err << "Restore: Failed to restore data, ";
              err << tableS.getTableName() << " table structure incompatible with backup's ... Exiting " << endl;
              exitHandler(NDBT_FAILED);
            } 
            if (tableS.m_staging &&
                !g_consumers[j]->prepare_staging(tableS))
            {
              err << "Restore: Failed to restore data, ";
              err << tableS.getTableName() << " failed to prepare staging table for data conversion ... Exiting " << endl;
              exitHandler(NDBT_FAILED);
            }
          } 
        }
      }
      for (i=0; i < metaData.getNoOfTables(); i++)
      {
        if (checkSysTable(metaData, i) &&
            checkDbAndTableName(metaData[i]))
        {
          // blob table checks use data which is populated by table compatibility checks
          TableS & tableS = *metaData[i];
          if(isBlobTable(&tableS))
          {
            for(Uint32 j= 0; j < g_consumers.size(); j++)
            {
              if (!g_consumers[j]->check_blobs(tableS))
              {
                  err << "Restore: Failed to restore data, ";
                  err << tableS.getTableName() << " table's blobs incompatible with backup's ... Exiting " << endl;
                  exitHandler(NDBT_FAILED);
              }
            }
          }
        }
      }
        
      RestoreDataIterator dataIter(metaData, &free_data_callback);

      if (!dataIter.validateBackupFile())
      {
          err << "Unable to allocate memory for BackupFile constructor" << endl;
          exitHandler(NDBT_FAILED);
      }


      if (!dataIter.validateRestoreDataIterator())
      {
          err << "Unable to allocate memory for RestoreDataIterator constructor" << endl;
          exitHandler(NDBT_FAILED);
      }
      
      Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
      info << timestamp << " [restore_data]" << " Read data file header" << endl;

      // Read data file header
      if (!dataIter.readHeader())
      {
	err << "Failed to read header of data file. Exiting..." << endl;
	exitHandler(NDBT_FAILED);
      }
      
      Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
      info << timestamp << " [restore_data]" << " Restore fragments" << endl;

      Uint32 fragmentId; 
      while (dataIter.readFragmentHeader(res= 0, &fragmentId))
      {
	const TupleS* tuple;
	while ((tuple = dataIter.getNextTuple(res= 1)) != 0)
	{
          const TableS* table = tuple->getTable();
          OutputStream *output = table_output[table->getLocalId()];
          if (!output)
            continue;
          OutputStream *tmp = ndbout.m_out;
          ndbout.m_out = output;
          for(Uint32 j= 0; j < g_consumers.size(); j++) 
            g_consumers[j]->tuple(* tuple, fragmentId);
          ndbout.m_out =  tmp;
          if (check_progress())
            report_progress("Data file progress: ", dataIter);
	} // while (tuple != NULL);
	
	if (res < 0)
	{
	  err <<" Restore: An error occured while restoring data. Exiting...";
          err << endl;
	  exitHandler(NDBT_FAILED);
	}
	if (!dataIter.validateFragmentFooter()) {
	  err << "Restore: Error validating fragment footer. ";
          err << "Exiting..." << endl;
	  exitHandler(NDBT_FAILED);
	}
      } // while (dataIter.readFragmentHeader(res))
      
      if (res < 0)
      {
	err << "Restore: An error occured while restoring data. Exiting... "
	    << "res= " << res << endl;
	exitHandler(NDBT_FAILED);
      }
      
      
      dataIter.validateFooter(); //not implemented
      
      for (i= 0; i < g_consumers.size(); i++)
	g_consumers[i]->endOfTuples();

      /* report to clusterlog if applicable */
      for(i= 0; i < g_consumers.size(); i++)
      {
        g_consumers[i]->report_data(ga_backupId, ga_nodeId);
      }
    }

    if(_restore_data || _print_log || _print_sql_log)
    {
      RestoreLogIterator logIter(metaData);

      Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
      info << timestamp << " [restore_log]" << " Read log file header" << endl;

      if (!logIter.readHeader())
      {
	err << "Failed to read header of data file. Exiting..." << endl;
	exitHandler(NDBT_FAILED);
      }
      
      const LogEntry * logEntry = 0;

      Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
      info << timestamp << " [restore_log]" << " Restore log entries" << endl;

      while ((logEntry = logIter.getNextLogEntry(res= 0)) != 0)
      {
        const TableS* table = logEntry->m_table;
        OutputStream *output = table_output[table->getLocalId()];
        if (!output)
          continue;
        for(Uint32 j= 0; j < g_consumers.size(); j++)
          g_consumers[j]->logEntry(* logEntry);

        if (check_progress())
          report_progress("Log file progress: ", logIter);
      }
      if (res < 0)
      {
	err << "Restore: An restoring the data log. Exiting... res=" 
	    << res << endl;
	exitHandler(NDBT_FAILED);
      }
      logIter.validateFooter(); //not implemented
      for (i= 0; i < g_consumers.size(); i++)
	g_consumers[i]->endOfLogEntrys();

      /* report to clusterlog if applicable */
      for(i= 0; i < g_consumers.size(); i++)
      {
        g_consumers[i]->report_log(ga_backupId, ga_nodeId);
      }
    }
    
    /* move data from staging table to real table */
    if(_restore_data)
    {
      for (i = 0; i < metaData.getNoOfTables(); i++)
      {
        const TableS* table = metaData[i];
        if (table->m_staging)
        {
          for(Uint32 j= 0; j < g_consumers.size(); j++)
          {
            if (!g_consumers[j]->finalize_staging(*table))
            {
              err << "Restore: Failed staging data to table: ";
              err << table->getTableName() << ". ";
              err << "Exiting... " << endl;
              exitHandler(NDBT_FAILED);
            }
          }
        }
      }
    }

    if(_restore_data)
    {
      for(i = 0; i < metaData.getNoOfTables(); i++)
      {
        const TableS* table = metaData[i];
        check_data_truncations(table);
        OutputStream *output = table_output[table->getLocalId()];
        if (!output)
          continue;
        for(Uint32 j= 0; j < g_consumers.size(); j++)
          if (!g_consumers[j]->finalize_table(*table))
          {
            err << "Restore: Failed to finalize restore table: %s. ";
            err << "Exiting... " << metaData[i]->getTableName() << endl;
            exitHandler(NDBT_FAILED);
          }
      }
    }
  }
  if (ga_restore_epoch)
  {
    Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    info << timestamp << " [restore_epoch]" << " Restoring epoch" << endl;

    for (i= 0; i < g_consumers.size(); i++)
      if (!g_consumers[i]->update_apply_status(metaData))
      {
        err << "Restore: Failed to restore epoch" << endl;
        exitHandler(NDBT_FAILED);
      }
  }

  unsigned j;
  for(j= 0; j < g_consumers.size(); j++) 
  {
    if (g_consumers[j]->has_temp_error())
    {
      clearConsumers();
      ndbout_c("\nRestore successful, but encountered temporary error, "
               "please look at configuration.");
    }               
  }

  if (ga_rebuild_indexes)
  {
    debug << "Rebuilding indexes" << endl;
    Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    info << timestamp << " [rebuild_indexes]" << " Rebuilding indexes" << endl;

    for(i = 0; i<metaData.getNoOfTables(); i++)
    {
      const TableS *table= metaData[i];
      if (! (checkSysTable(table) && checkDbAndTableName(table)))
        continue;
      if (isBlobTable(table) || isIndex(table))
        continue;
      for(Uint32 j= 0; j < g_consumers.size(); j++)
      {
        if (!g_consumers[j]->rebuild_indexes(* table))
          exitHandler(NDBT_FAILED);
      }
    }
    for(Uint32 j= 0; j < g_consumers.size(); j++)
    {
      if (!g_consumers[j]->endOfTablesFK())
        exitHandler(NDBT_FAILED);
    }
  }

  /* report to clusterlog if applicable */
  for (i = 0; i < g_consumers.size(); i++)
    g_consumers[i]->report_completed(ga_backupId, ga_nodeId);

  clearConsumers();

  for(i = 0; i < metaData.getNoOfTables(); i++)
  {
    if (table_output[i] &&
        table_output[i] != ndbout.m_out)
    {
      my_fclose(((FileOutputStream *)table_output[i])->getFile(), MYF(MY_WME));
      delete table_output[i];
      table_output[i] = NULL;
    }
  }
  free_include_excludes_vector();

  if (opt_verbose)
    return NDBT_ProgramExit(NDBT_OK);
  else
    return 0;
} // main

template class Vector<BackupConsumer*>;
template class Vector<OutputStream*>;
template class Vector<RestoreOption *>;
