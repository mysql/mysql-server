/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <Logger.hpp>
#include <NdbOut.hpp>
#include <NdbTCP.h>
#include <NdbToolsProgramExitCodes.hpp>
#include <OutputStream.hpp>
#include <Properties.hpp>
#include <Vector.hpp>
#include <ndb_global.h>
#include <ndb_limits.h>
#include <ndb_opts.h>
#include <ndb_version.h>
#include "my_getopt.h"
#include "util/ndb_openssl_evp.h" // ndb_openssl_evp::library_init()
#include "portlib/NdbTick.h"

#include "../src/ndbapi/NdbDictionaryImpl.hpp"
#include "consumer_printer.hpp"
#include "consumer_restore.hpp"
#include "my_alloc.h"

#include <NdbThread.h>

#include <my_dir.h>

#define TMP_TABLE_PREFIX "#sql"
#define TMP_TABLE_PREFIX_LEN 4

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

extern RestoreLogger restoreLogger;

static Uint32 g_tableCompabilityMask = 0;
static int ga_nodeId = 0;
static int ga_nParallelism = 128;
static int ga_backupId = 0;
bool ga_dont_ignore_systab_0 = false;
static bool ga_no_upgrade = false;
static bool ga_promote_attributes = false;
static bool ga_demote_attributes = false;

static const Uint32 BF_UNKNOWN = 0;
static const Uint32 BF_SINGLE = 1;
static const Uint32 BF_MULTI_PART = 2;

static bool g_restoring_in_parallel = true;
static const int g_max_parts = 128;

static int ga_backup_format = BF_UNKNOWN;
static int ga_part_count = 1;
static int ga_error_thread = 0;

static const char* default_backupPath = "." DIR_SEPARATOR;
static const char* ga_backupPath = default_backupPath;

static bool opt_decrypt = false;

// g_backup_password global, directly accessed in Restore.cpp.
ndb_password_state g_backup_password_state("backup", nullptr);
static ndb_password_option opt_backup_password(g_backup_password_state);
static ndb_password_from_stdin_option opt_backup_password_from_stdin(
                                           g_backup_password_state);

const char *opt_ndb_database= NULL;
const char *opt_ndb_table= NULL;
unsigned int opt_verbose;
unsigned int opt_hex_format;
bool opt_show_part_id = true;
unsigned int opt_progress_frequency;
NDB_TICKS g_report_prev;
Vector<BaseString> g_databases;
Vector<BaseString> g_tables;
Vector<BaseString> g_include_tables, g_exclude_tables;
Vector<BaseString> g_include_databases, g_exclude_databases;
Properties g_rewrite_databases;
NdbRecordPrintFormat g_ndbrecord_print_format;
unsigned int opt_no_binlog;
static bool opt_timestamp_printouts;

Ndb_cluster_connection *g_cluster_connection = NULL;

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
static void exitHandler(int code);
static void free_include_excludes_vector();

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
static bool ga_with_apply_status = false;
bool opt_include_stored_grants = false;
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
bool ga_allow_pk_changes = false;
bool ga_ignore_extended_pk_updates = false;
BaseString g_options("ndb_restore");
static int ga_num_slices = 1;
static int ga_slice_id = 0;

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
  ,OPT_REMAP_COLUMN = 'x'
  ,OPT_NODEGROUP_MAP = 'z'
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
static const char *opt_one_remap_col_arg= NULL;
static bool opt_restore_privilege_tables = false;

/**
 * ExtraTableInfo
 *
 * Container for information from user about how
 * table should be restored
 */
class ExtraTableInfo
{
public:
  ExtraTableInfo(const char* db_name,
                 const char* table_name):
    m_dbName(db_name),
    m_tableName(table_name)
  {
  }

  ~ExtraTableInfo() {}

  const BaseString m_dbName;
  const BaseString m_tableName;

  /* Arguments related to column remappings */
  Vector<BaseString> m_remapColumnArgs;
};

/**
 * ExtraRestoreInfo
 *
 * Container for information from user about
 * how to restore
 */
class ExtraRestoreInfo
{
public:
  ExtraRestoreInfo()
  {}
  ~ExtraRestoreInfo()
  {
    for (Uint32 i=0; i<m_tables.size(); i++)
    {
      delete m_tables[i];
      m_tables[i] = NULL;
    }
  }

  /**
   * findTable
   *
   * Lookup extra restore info for named table
   */
  ExtraTableInfo* findTable(const char* db_name,
                            const char* table_name)
  {
    for (Uint32 i=0; i<m_tables.size(); i++)
    {
      ExtraTableInfo* tab = m_tables[i];
      if ((strcmp(db_name, tab->m_dbName.c_str()) == 0) &&
          (strcmp(table_name, tab->m_tableName.c_str()) == 0))
      {
        return tab;
      }
    }
    return NULL;
  }

  /**
   * findOrAddTable
   *
   * Lookup or Add empty extra restore info for named table
   */
  ExtraTableInfo* findOrAddTable(const char* db_name,
                                 const char* table_name)
  {
    ExtraTableInfo* tab = findTable(db_name, table_name);
    if (tab != NULL)
    {
      return tab;
    }

    tab = new ExtraTableInfo(db_name, table_name);
    m_tables.push_back(tab);
    return tab;
  }

  Vector<ExtraTableInfo*> m_tables;
};

static ExtraRestoreInfo g_extra_restore_info;

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "timestamp_printouts", NDB_OPT_NOSHORT,
    "Add a timestamp to the logger messages info, error and debug",
    (uchar**) &opt_timestamp_printouts, (uchar**) &opt_timestamp_printouts, 0,
    GET_BOOL, NO_ARG, true, 0, 0, 0, 0, 0 },
  { "connect", 'c', "same as --connect-string",
    &opt_ndb_connectstring, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodeid", 'n', "Backup files from node with id",
    &ga_nodeId, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "backup-password", NDB_OPT_NOSHORT, "Encryption password for backup file",
    nullptr, nullptr, 0,
    GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0, &opt_backup_password},
  { "backup-password-from-stdin", NDB_OPT_NOSHORT,
    "Read encryption password for backup file from stdin",
    &opt_backup_password_from_stdin.opt_value, nullptr, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_backup_password_from_stdin},
  { "backupid", 'b', "Backup id",
    &ga_backupId, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "decrypt", NDB_OPT_NOSHORT, "Decrypt file",
    &opt_decrypt, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_data", 'r', 
    "Restore table data/logs into NDB Cluster using NDBAPI", 
    &_restore_data, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_meta", 'm',
    "Restore meta data into NDB Cluster using NDBAPI",
    &_restore_meta, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-upgrade", 'u',
    "Don't upgrade array type for var attributes, which don't resize VAR data and don't change column attributes",
    &ga_no_upgrade, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "promote-attributes", 'A',
    "Allow attributes to be promoted when restoring data from backup",
    &ga_promote_attributes, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "lossy-conversions", 'L',
    "Allow lossy conversions for attributes (type demotions or integral"
    " signed/unsigned type changes) when restoring data from backup",
    &ga_demote_attributes, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "preserve-trailing-spaces", 'P',
    "Allow to preserve the tailing spaces (including paddings) When char->varchar or binary->varbinary is promoted",
    &_preserve_trailing_spaces, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-restore-disk-objects", 'd',
    "Dont restore disk objects (tablespace/logfilegroups etc)",
    &_no_restore_disk, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_epoch", 'e', 
    "Restore epoch info into the status table. Convenient for starting MySQL "
    "Cluster replication. The row in "
    NDB_REP_DB "." NDB_APPLY_TABLE " with id 0 will be updated/inserted.", 
    &ga_restore_epoch, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "with-apply-status", 'w',
    "Restore the " NDB_APPLY_TABLE " system table content from the backup.",
    &ga_with_apply_status, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-table-check", 's', "Skip table structure check during restore of data",
   &ga_skip_table_check, nullptr, nullptr,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "parallelism", 'p',
    "No of parallel transactions during restore of data."
    "(parallelism can be 1 to 1024)", 
    &ga_nParallelism, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 128, 1, 1024, 0, 1, 0 },
  { "print", NDB_OPT_NOSHORT, "Print metadata, data and log to stdout",
    &_print, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_data", NDB_OPT_NOSHORT, "Print data to stdout",
    &_print_data, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_meta", NDB_OPT_NOSHORT, "Print meta data to stdout",
    &_print_meta, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_log", NDB_OPT_NOSHORT, "Print log to stdout",
    &_print_log, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_sql_log", NDB_OPT_NOSHORT, "Print SQL log to stdout",
    &_print_sql_log, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "backup_path", NDB_OPT_NOSHORT, "Path to backup files",
    &ga_backupPath, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "dont_ignore_systab_0", 'f',
    "Do not ignore system table during --print-data.", 
    &ga_dont_ignore_systab_0, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "ndb-nodegroup-map", OPT_NODEGROUP_MAP,
    "Nodegroup specification. Not supported anymore, value will be ignored.",
    nullptr, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-enclosed-by", NDB_OPT_NOSHORT,
    "Fields are enclosed by ...",
    &opt_fields_enclosed_by, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-terminated-by", NDB_OPT_NOSHORT,
    "Fields are terminated by ...",
    &opt_fields_terminated_by, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-optionally-enclosed-by", NDB_OPT_NOSHORT,
    "Fields are optionally enclosed by ...",
    &opt_fields_optionally_enclosed_by, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "hex", NDB_OPT_NOSHORT, "print binary types in hex format", 
    &opt_hex_format, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "tab", 'T', "Creates tab separated textfile for each table to "
    "given path. (creates .txt files)",
   &tab_path, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "append", NDB_OPT_NOSHORT, "for --tab append data to file", 
    &opt_append, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "lines-terminated-by", NDB_OPT_NOSHORT, "",
    &opt_lines_terminated_by, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "progress-frequency", NDB_OPT_NOSHORT,
    "Print status uf restore periodically in given seconds", 
    &opt_progress_frequency, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 0, 0, 65535, 0, 0, 0 },
  { "no-binlog", NDB_OPT_NOSHORT,
    "If a mysqld is connected and has binary log, do not log the restored data", 
    &opt_no_binlog, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", OPT_VERBOSE,
    "verbosity", 
    &opt_verbose, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 1, 0, 255, 0, 0, 0 },
  { "include-databases", OPT_INCLUDE_DATABASES,
    "Comma separated list of databases to restore. Example: db1,db3",
    &opt_include_databases, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-databases", OPT_EXCLUDE_DATABASES,
    "Comma separated list of databases to not restore. Example: db1,db3",
    &opt_exclude_databases, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "rewrite-database", OPT_REWRITE_DATABASE,
    "A pair 'source,dest' of database names from/into which to restore. "
    "Example: --rewrite-database=oldDb,newDb",
    &opt_rewrite_database, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "include-tables", OPT_INCLUDE_TABLES, "Comma separated list of tables to "
    "restore. Table name should include database name. Example: db1.t1,db3.t1", 
    &opt_include_tables, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-tables", OPT_EXCLUDE_TABLES, "Comma separated list of tables to "
    "not restore. Table name should include database name. "
    "Example: db1.t1,db3.t1",
    &opt_exclude_tables, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore-privilege-tables", NDB_OPT_NOSHORT,
    "Restore privilege tables (after they have been moved to ndb)",
    &opt_restore_privilege_tables, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "include-stored-grants", NDB_OPT_NOSHORT,
    "Restore users and grants to ndb_sql_metadata table",
    &opt_include_stored_grants, nullptr, nullptr,
    GET_BOOL, OPT_ARG, false, 0, 0, 0, 0, 0 },
  { "exclude-missing-columns", NDB_OPT_NOSHORT,
    "Ignore columns present in backup but not in database",
    &ga_exclude_missing_columns, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-missing-tables", NDB_OPT_NOSHORT,
    "Ignore tables present in backup but not in database",
    &ga_exclude_missing_tables, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "exclude-intermediate-sql-tables", NDB_OPT_NOSHORT,
    "Do not restore intermediate tables with #sql-prefixed names",
    &opt_exclude_intermediate_sql_tables, nullptr, nullptr,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "disable-indexes", NDB_OPT_NOSHORT,
    "Disable indexes and foreign keys",
    &ga_disable_indexes, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "rebuild-indexes", NDB_OPT_NOSHORT,
    "Rebuild indexes",
    &ga_rebuild_indexes, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-unknown-objects", 256, "Skip unknown object when parsing backup",
    &ga_skip_unknown_objects, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "skip-broken-objects", 256, "Skip broken object when parsing backup",
    &ga_skip_broken_objects, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "show-part-id", 256, "Prefix log messages with backup part ID",
    &opt_show_part_id, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
#ifdef ERROR_INSERT
  { "error-insert", OPT_ERROR_INSERT, "Insert errors (testing option)",
    &_error_insert, nullptr, nullptr,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif
  { "num_slices", NDB_OPT_NOSHORT, "How many slices are being applied",
    &ga_num_slices, nullptr, nullptr, GET_UINT, REQUIRED_ARG,
    1, 1, 1024, nullptr, 0, nullptr },
  { "slice_id", NDB_OPT_NOSHORT, "My slice id",
    &ga_slice_id, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, 1023, nullptr, 0, nullptr },
  { "allow-pk-changes", NDB_OPT_NOSHORT,
    "Allow changes to the set of columns making up a table's primary key.",
    &ga_allow_pk_changes, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "ignore-extended-pk-updates", NDB_OPT_NOSHORT,
    "Ignore log entries containing updates to columns now included in an "
    "extended primary key.",
    &ga_ignore_extended_pk_updates, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "remap-column", OPT_REMAP_COLUMN, "Remap content for column while "
    "restoring, format <database>.<table>.<column>:<function>:<function_args>."
    "  <database> is remapped name, remapping applied before other conversions.",
    &opt_one_remap_col_arg, nullptr, nullptr,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  NdbStdOpt::end_of_options
};


static bool parse_remap_option(const BaseString option,
                               BaseString& db_name,
                               BaseString& tab_name,
                               BaseString& col_name,
                               BaseString& func_name,
                               BaseString& func_args,
                               BaseString& error_msg)
{
  const char* expectedFormat = "<db>.<table>.<column>:function[:args]";

  Vector<BaseString> optParts;
  const int numOptParts = option.split(optParts,
                                       BaseString(":"),
                                       3);

  if (numOptParts < 2)
  {
    error_msg.assfmt("remap-column : Badly formed option : %s.  "
                     "Expected format : %s.",
                     option.c_str(),
                     expectedFormat);
    return false;
  }

  Vector<BaseString> nameParts;
  const int numNameParts = optParts[0].split(nameParts,
                                             BaseString("."));
  if (numNameParts != 3)
  {
    error_msg.assfmt("remap-column : Badly formed column specifier : %s "
                     "in option %s.  "
                     "Expected format : %s.",
                     optParts[0].c_str(),
                     option.c_str(),
                     expectedFormat);
    return false;
  }

  /* Copy out substrings */
  db_name.assign(nameParts[0]);
  tab_name.assign(nameParts[1]);
  col_name.assign(nameParts[2]);
  func_name.assign(optParts[1]);

  if (numOptParts == 3)
  {
    func_args.assign(optParts[2]);
  }
  else
  {
    func_args.assign("");
  }

  return true;
}


static bool parse_remap_column(const char* argument)
{
  BaseString option(argument);
  BaseString db, tab, col, func, args, error_msg;

  if (!parse_remap_option(option,
                          db,
                          tab,
                          col,
                          func,
                          args,
                          error_msg))
  {
    restoreLogger.log_info("%s", error_msg.c_str());
    return false;
  }

  /* Store this remapping + arguments against the db+table name */
  ExtraTableInfo* eti = g_extra_restore_info.findOrAddTable(db.c_str(),
                                                            tab.c_str());

  /* We store the whole argument string to assist error reporting later */
  eti->m_remapColumnArgs.push_back(option);

  return true;
}

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[<path to backup files>]");
}

static bool
get_one_option(int optid, const struct my_option *opt, char *argument)
{
#ifndef NDEBUG
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
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
    }
    info.setLevel(254);
    info << "Nodeid = " << ga_nodeId << endl;
    break;
  case 'b':
    if (ga_backupId == 0)
    {
      err << "Error in --backupid,-b setting, see --help";
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
    }
    info.setLevel(254);
    info << "Backup Id = " << ga_backupId << endl;
    break;
  case OPT_NODEGROUP_MAP:
    // Support for mapping nodegroups during restore has been removed, just
    // print message saying the setting is ignored
    err << "NOTE! Support for --ndb-nodegroup-map=<string> has been removed"
        << endl;
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
  case OPT_REMAP_COLUMN:
    return (parse_remap_column(argument) ? 0 : 1);
  case NDB_OPT_NOSHORT:
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
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
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

// Exclude the legacy privilege tables from Cluster 7.x
void exclude_privilege_tables() {
  static const char *priv_tables[] = {
      "mysql.user",         "mysql.db",         "mysql.tables_priv",
      "mysql.columns_priv", "mysql.procs_priv", "mysql.proxies_priv"};

  for (size_t i = 0; i < array_elements(priv_tables); i++) {
    g_exclude_tables.push_back(priv_tables[i]);
    save_include_exclude(OPT_EXCLUDE_TABLES,
                         const_cast<char *>(priv_tables[i]));
  }
}

bool
readArguments(Ndb_opts & opts, char*** pargv)
{
  Uint32 i;
  BaseString tmp;
  debug << "Load defaults" << endl;

  debug << "handle_options" << endl;

  opts.set_usage_funcs(short_usage_sub);

  if (opts.handle_options(get_one_option))
  {
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
  }
  const bool have_password_option =
                 g_backup_password_state.have_password_option();
  if (!opt_decrypt)
  {
    if (have_password_option)
    {
      err << "Password (--backup-password) for decryption given, require "
             "also --decrypt."
          << endl;
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
    }
  }
  else
  {
    if (!have_password_option)
    {
      err << "Decrypting backup (--decrypt) requires password (--backup-password)." << endl;
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
    }
  }

  bool failed = ndb_option::post_process_options();
  if (failed)
  {
    BaseString err_msg = g_backup_password_state.get_error_message();
    if (!err_msg.empty())
    {
      err << "Error: " << err_msg.c_str() << endl;
    }
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
  }
  if (!opt_timestamp_printouts) {
    restoreLogger.set_print_timestamp(false);
  }
  if (ga_nodeId == 0)
  {
    err << "Backup file node ID not specified, please provide --nodeid" << endl;
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
  }
  if (ga_backupId == 0)
  {
    err << "Backup ID not specified, please provide --backupid" << endl;
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
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
    // Exclude privilege tables unless explicitly included
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
  if (ga_allow_pk_changes)
  {
    g_tableCompabilityMask |= TCM_ALLOW_PK_CHANGES;
  }
  if(ga_ignore_extended_pk_updates)
  {
    g_tableCompabilityMask |= TCM_IGNORE_EXTENDED_PK_UPDATES;
  }
  return true;
}

bool create_consumers(RestoreThreadData *data)
{
  BackupPrinter *printer = new BackupPrinter();
  if (printer == NULL)
    return false;

  char threadname[20];
  BaseString::snprintf(threadname, sizeof(threadname), "%d-%u-%u",
                       ga_nodeId,
                       data->m_part_id,
                       ga_slice_id);
  BackupRestore* restore = new BackupRestore(g_cluster_connection,
                                             threadname,
                                             ga_nParallelism);

  if (restore == NULL)
  {
    delete printer;
    printer = NULL;
    return false;
  }

  if (_print)
  {
    ga_print = true;
    ga_restore = true;
    printer->m_print = true;
  }
  if (_print_meta)
  {
    ga_print = true;
    printer->m_print_meta = true;
  }
  if (_print_data)
  {
    ga_print = true;
    printer->m_print_data = true;
  }
  if (_print_log)
  {
    ga_print = true;
    printer->m_print_log = true;
  }
  if (_print_sql_log)
    {
      ga_print = true;
      printer->m_print_sql_log = true;
    }

  if (_restore_data)
  {
    ga_restore = true;
    restore->m_restore = true;
  }

  if (_restore_meta)
  {
    // ndb_restore has been requested to perform some metadata work
    // like restore-meta or disable-indexes. To avoid 'object already exists'
    // errors, only restore-thread 1 will do the actual metadata-restore work.
    // So flags like restore_meta, restore_epoch and disable_indexes are set
    // only on thread 1 to indicate that it must perform this restore work.
    // While restoring metadata, some init work is done, like creating an Ndb
    // object, setting up callbacks, and loading info about all the tables into
    // the BackupConsumer.
    // The remaining threads also need this init work to be done, since later
    // phases of ndb_restore rely upon it, e.g. --restore-data needs the table
    // info. So an additional flag m_metadata_work_requested is set for all
    // the restore-threads to indicate that the init work must be done. If
    // m_metadata_work_requested = 1 and m_restore_meta = 0, the thread will
    // do only the init work, and skip the ndbapi function calls to create or
    // delete the metadata objects.
    restore->m_metadata_work_requested = true;
    if (data->m_part_id == 1)
    {
      // restore-thread 1 must perform actual work of restoring metadata
      restore->m_restore_meta = true;
      // use thread-specific flag saved in RestoreThreadData to determine
      // whether the thread should restore metadata objects
      data->m_restore_meta = true;
    }
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
    restore->m_restore_epoch_requested = true;
    if (data->m_part_id == 1)
      restore->m_restore_epoch = true;
  }

  if (ga_disable_indexes)
  {
    restore->m_metadata_work_requested = true;
    if (data->m_part_id == 1)
      restore->m_disable_indexes = true;
  }

  if (ga_rebuild_indexes)
  {
    restore->m_metadata_work_requested = true;
    if (data->m_part_id == 1)
      restore->m_rebuild_indexes = true;
  }

  if(ga_with_apply_status)
  {
    restore->m_with_apply_status = true;
    if (data->m_part_id == 1)
      restore->m_delete_epoch_tuple = true;
  }

  {
    BackupConsumer * c = printer;
    data->m_consumers.push_back(c);
  }
  {
    BackupConsumer * c = restore;
    data->m_consumers.push_back(c);
  }
  return true;
}

void
clear_consumers(RestoreThreadData *data)
{
  for(Uint32 i= 0; i<data->m_consumers.size(); i++)
    delete data->m_consumers[i];
  data->m_consumers.clear();
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
  exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
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
        exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
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

  // If --exclude lists are given, check them
  if ((g_exclude_tables.size() || opt_exclude_databases)
       && ! checkDoRestore(table))
    return false;

  // If --include lists are given, ignore the old-style table list
  if (opt_include_tables || opt_include_databases ) {
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
exclude_missing_tables(const RestoreMetaData& metaData, const Vector<BackupConsumer*> g_consumers)
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

static TableS*
find_table_spec(RestoreMetaData& metaData,
                const char* searchDbName,
                const char* searchTableName,
                bool rewrite_backup_db)
{
  for(Uint32 m = 0; m < metaData.getNoOfTables(); m++)
  {
    TableS *tableSpec= metaData[m];

    BaseString externalName = makeExternalTableName(tableSpec->getTableName());
    BaseString dbName, tabName;
    {
      Vector<BaseString> components;
      if (externalName.split(components,
                             BaseString(".")) != 2)
      {
        restoreLogger.log_info("Error processing table name from "
                               "backup %s from %s",
                               externalName.c_str(),
                               tableSpec->getTableName());
        return NULL;
      }
      dbName = components[0];
      tabName = components[1];

      if (rewrite_backup_db)
      {
        /* Check for rewrite db, as args are specified wrt new db names */
        const char* rewrite_dbname;
        if (g_rewrite_databases.get(dbName.c_str(), &rewrite_dbname))
        {
          dbName.assign(rewrite_dbname);
        }
      }
    }

    if (dbName == searchDbName &&
        tabName == searchTableName)
    {
      return tableSpec;
    }
  }

  return NULL;
}

class OffsetTransform : public ColumnTransform
{
public:
  static
  OffsetTransform* parse(const NdbDictionary::Column* col,
                         const BaseString& func_name,
                         const BaseString& func_args,
                         BaseString& error_msg)
  {
    bool sig = true;
    Uint64 bits = 0;
    switch (col->getType())
    {
    case NdbDictionary::Column::Bigint:
      sig = true;
      bits = 64;
      break;
    case NdbDictionary::Column::Bigunsigned:
      sig = false;
      bits = 64;
      break;
    case NdbDictionary::Column::Int:
      sig = true;
      bits = 32;
      break;
    case NdbDictionary::Column::Unsigned:
      sig = false;
      bits = 32;
      break;
    default:
      error_msg.assfmt("Column does not have supported integer type");
      return NULL;
    }

    /* Todo : Use ndb type traits */
    const Uint64 shift = bits - 1;
    const Uint64 max_uval = ((Uint64(1) << shift) -1) | (Uint64(1) << shift);
    const Int64 min_sval = 0 - (Uint64(1) << shift);
    const Int64 max_sval = (Uint64(1) << shift) - 1;

    Int64 offset_val;

    int cnt = sscanf(func_args.c_str(), "%lld", &offset_val);
    if (cnt != 1)
    {
      error_msg.assfmt("offset argument invalid");
      return NULL;
    }

    {
      /* Re-convert back to check for silent-saturation in sscanf */
      char numbuf[22];
      BaseString::snprintf(numbuf, sizeof(numbuf),
                           "%lld", offset_val);
      if (strncmp(func_args.c_str(), numbuf, sizeof(numbuf)) != 0)
      {
        error_msg.assfmt("Offset %s unreadable - out of range for type?",
                         func_args.c_str());
        return NULL;
      }
    }

    if (offset_val < min_sval ||
        offset_val > max_sval)
    {
      error_msg.assfmt("Offset %lld is out of range for type.",
                       offset_val);
      return NULL;
    }

    return new OffsetTransform(offset_val,
                               sig,
                               bits,
                               min_sval,
                               max_sval,
                               max_uval);
  }

private:
  Int64 m_offset_val;
  Int64 m_sig_bound;
  Uint64 m_unsig_bound;
  bool m_offset_positive;
  bool m_sig;
  Uint32 m_bits;

  OffsetTransform(Int64 offset_val,
                  bool sig,
                  Uint32 bits,
                  Int64 min_sval,
                  Int64 max_sval,
                  Uint64 max_uval):
    m_offset_val(offset_val),
    m_sig(sig),
    m_bits(bits)
  {
    m_offset_positive = offset_val >= 0;
    if (sig)
    {
      if (m_offset_positive)
      {
        m_sig_bound = max_sval - offset_val;
      }
      else
      {
        m_sig_bound = min_sval - offset_val; // - - = +
      }
    }
    else
    {
      if (m_offset_positive)
      {
        m_unsig_bound = max_uval - offset_val;
      }
      else
      {
        m_unsig_bound = (0 - offset_val);
      }
    }
  }

  ~OffsetTransform() override {}

  static Uint64 readIntoU64(const void* src, Uint32 bits)
  {
    switch(bits)
    {
    case 64:
      Uint64 dst;
      memcpy(&dst, src, 8);
      return dst;
    case 32:
    {
      Uint32 u32;
      memcpy(&u32, src, 4);
      return u32;
    }
    default:
      abort();
    }
    return 0;
  }

  static void writeFromU64(Uint64 src, void* dst, Uint32 bits)
  {
    switch(bits)
    {
    case 64:
      memcpy(dst, &src, 8);
      return;
    case 32:
    {
      Uint32 u32 = (Uint32) src;
      memcpy(dst, &u32, 4);
      return;
    }
    default:
      abort();
    }
  }

  static Int64 readIntoS64(const void* src, Uint32 bits)
  {
    switch(bits)
    {
    case 64:
      Int64 dst;
      memcpy(&dst, src, 8);
      return dst;
    case 32:
    {
      Int32 i32;
      memcpy(&i32, src, 4);
      return i32;
    }
    default:
      abort();
    }
    return 0;
  }

  static void writeFromS64(Int64 src, void* dst, Uint32 bits)
  {
    switch(bits)
    {
    case 64:
      memcpy(dst, &src, 8);
      return;
    case 32:
    {
      Int32 i32 = (Int32) src;
      memcpy(dst, &i32, 4);
      return;
    }
    default:
      abort();
    }
  }
      
  bool apply(const NdbDictionary::Column* col,
                     const void* src_data,
                     void** dst_data) override
  {
    if (src_data == NULL)
    {
      /* Offset(NULL, *) -> NULL */
      *dst_data = NULL;
      return true;
    }

    if (m_sig)
    {
      Int64 src_val = readIntoS64(src_data, m_bits);

      bool src_in_bounds = true;
      if (m_offset_positive)
      {
        src_in_bounds = (src_val <= m_sig_bound);
      }
      else
      {
        src_in_bounds = (src_val >= m_sig_bound);
      }

      if (unlikely(!src_in_bounds))
      {
        fprintf(stderr, "Offset : Source value out of bounds : adding %lld to %lld "
                "gives an out of bounds value\n",
                m_offset_val,
                src_val);

        return false;
      }

      src_val += m_offset_val;

      writeFromS64(src_val, *dst_data, m_bits);
    }
    else
    {
      /* Unsigned */
      Uint64 src_val = readIntoU64(src_data, m_bits);

      bool src_in_bounds = true;
      if (m_offset_positive)
      {
        src_in_bounds = (src_val <= m_unsig_bound);
      }
      else
      {
        src_in_bounds = (src_val >= m_unsig_bound);
      }

      if (unlikely(!src_in_bounds))
      {
        fprintf(stderr, "Offset : Source value out of bounds : adding %lld to %llu "
                "gives an out of bounds value\n",
                m_offset_val,
                src_val);

        return false;
      }

      src_val+= m_offset_val;

      writeFromU64(src_val, *dst_data, m_bits);
    }

    return true;
  }
};


static ColumnTransform*
create_column_transform(const NdbDictionary::Column* col,
                        const BaseString& func_name,
                        const BaseString& func_args,
                        BaseString& error_msg)
{
  BaseString lcfunc_name(func_name);
  lcfunc_name.ndb_tolower();

  if (lcfunc_name == "offset")
  {
    return OffsetTransform::parse(col, func_name, func_args, error_msg);
  }
  error_msg.assfmt("Function %s not defined", func_name.c_str());
  return NULL;
}

static bool
setup_one_remapping(TableS* tableSpec,
                    const BaseString& col_name,
                    const BaseString& func_name,
                    const BaseString& func_args,
                    BaseString& error_msg)
{
  const NdbDictionary::Column* col =
    tableSpec->m_dictTable->getColumn(col_name.c_str());

  if (!col)
  {
    error_msg.assfmt("Failed to find column %s in table",
                     col_name.c_str());
    return false;
  }

  AttributeDesc* ad = tableSpec->getAttributeDesc(col->getColumnNo());

  if (ad->transform != NULL)
  {
    error_msg.assfmt("Duplicate remappings on column %s",
                     col_name.c_str());
    return false;
  }

  restoreLogger.log_debug("Initialising remap function "
                          "\"%s:%s\" on column %s.%s",
                          func_name.c_str(),
                          func_args.c_str(),
                          tableSpec->m_dictTable->getName(),
                          col_name.c_str());

  ColumnTransform* ct = create_column_transform(col,
                                                func_name,
                                                func_args,
                                                error_msg);

  if (ct == NULL)
  {
    return false;
  }

  ad->transform = ct;

  return true;
}

static bool
setup_column_remappings(RestoreMetaData& metaData)
{
  for (Uint32 t = 0; t < g_extra_restore_info.m_tables.size(); t++)
  {
    const ExtraTableInfo* eti = g_extra_restore_info.m_tables[t];

    TableS* tableSpec = find_table_spec(metaData,
                                        eti->m_dbName.c_str(),
                                        eti->m_tableName.c_str(),
                                        true); // Rewrite database
    if (tableSpec)
    {
      const Vector<TableS*> blobTables = tableSpec->getBlobTables();
      const bool haveBlobPartTables = blobTables.size() > 0;

      for (Uint32 a=0; a < eti->m_remapColumnArgs.size(); a++)
      {
        BaseString db_name, tab_name, col_name, func_name, func_args, error_msg;
        if (!parse_remap_option(eti->m_remapColumnArgs[a],
                                db_name,
                                tab_name,
                                col_name,
                                func_name,
                                func_args,
                                error_msg))
        {
          /* Should never happen as arg parsed on initial read */
          restoreLogger.log_info("Unexpected - parse failed : \"%s\"",
                                 eti->m_remapColumnArgs[a].c_str());
          return false;
        }

        if (!setup_one_remapping(tableSpec,
                                 col_name,
                                 func_name,
                                 func_args,
                                 error_msg))
        {
          restoreLogger.log_info("remap_column : Failed with \"%s\" "
                                 "while processing option \"%s\"",
                                 error_msg.c_str(),
                                 eti->m_remapColumnArgs[a].c_str());
          return false;
        }

        const bool col_in_pk =
          tableSpec->m_dictTable->getColumn(col_name.c_str())->getPrimaryKey();

        if (col_in_pk &&
            haveBlobPartTables)
        {
          /* This transform should be added on the Blob part table(s) */
          for (Uint32 b=0; b < blobTables.size(); b++)
          {
            TableS* blobPartTableSpec = blobTables[b];
            const NdbDictionary::Column* mainTabBlobCol =
              tableSpec->m_dictTable->getColumn(blobPartTableSpec->getMainColumnId());

            if (unlikely(mainTabBlobCol->getBlobVersion() == NDB_BLOB_V1))
            {
              restoreLogger.log_info("remap_column : Failed as table has "
                                     "v1 Blob column %s when processing "
                                     "option %s",
                                     mainTabBlobCol->getName(),
                                     eti->m_remapColumnArgs[a].c_str());
              return false;
            }

            if (!setup_one_remapping(blobPartTableSpec,
                                     col_name.c_str(),
                                     func_name,
                                     func_args,
                                     error_msg))
            {
              restoreLogger.log_info("remap_column : Failed with error %s "
                                     "while applying remapping to blob "
                                     "parts table %s from option : %s",
                                     error_msg.c_str(),
                                     blobPartTableSpec->m_dictTable->getName(),
                                     eti->m_remapColumnArgs[a].c_str());
              return false;
            }
          }
        }
      }
    }
    else
    {
      restoreLogger.log_info("remap_column : Failed to find table in Backup "
                             "matching option : \"%s\"",
                             eti->m_remapColumnArgs[0].c_str());
      return false;
    }
  }

  return true;
}

static void
free_data_callback(void *ctx)
{
  // RestoreThreadData is passed as context object to in RestoreDataIterator
  // ctor. RestoreDataIterator calls callback function with context object
  // as parameter, so that callback can extract thread info from it.
  RestoreThreadData *data = (RestoreThreadData*)ctx;
  for(Uint32 i= 0; i < data->m_consumers.size(); i++)
    data->m_consumers[i]->tuple_free();
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
  ndb_openssl_evp::library_end();
  free_include_excludes_vector();
  exit(code);
}

static void init_restore()
{
  if (_restore_meta || _restore_data || ga_restore_epoch
                    || ga_disable_indexes || ga_rebuild_indexes)
  {
    // create one Ndb_cluster_connection to be shared by all threads
    g_cluster_connection = new Ndb_cluster_connection(opt_ndb_connectstring,
                                        opt_ndb_nodeid);
    if (g_cluster_connection == NULL)
    {
      err << "Failed to create cluster connection!!" << endl;
      exitHandler(NdbToolsProgramExitCode::FAILED);
    }
    g_cluster_connection->set_name(g_options.c_str());
    if (g_cluster_connection->connect(opt_connect_retries - 1,
            opt_connect_retry_delay, 1) != 0)
    {
      delete g_cluster_connection;
      g_cluster_connection = NULL;
      exitHandler(NdbToolsProgramExitCode::FAILED);
    }
  }
}

static void cleanup_restore()
{
  delete g_cluster_connection;
  g_cluster_connection = 0;
  free_include_excludes_vector();
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
    restoreLogger.log_info("%s %llupercent(%llu bytes)\n", prefix, (f.get_file_pos() * 100 + f.get_file_size()-1) / f.get_file_size(), f.get_file_pos());
  else
    restoreLogger.log_info("%s %llu bytes\n", prefix, f.get_file_pos());
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
      restoreLogger.log_info("Data truncation(s) detected for attribute: %s.%s",
           tname, cname);
      desc->truncation_detected = false;
    }
  }
}


/**
 * Determine whether we should skip this table fragment due to
 * operating in slice mode
 */
static bool
determine_slice_skip_fragment(TableS * table, Uint32 fragmentId, Uint32& fragmentCount)
{
  if (ga_num_slices == 1)
  {
    /* No slicing */
    return false;
  }

  /* Should we restore this fragment? */
  int fragmentRestoreSlice = 0;
  if (table->isBlobRelated())
  {
    /**
     * v2 blobs + staging tables
     * Staging tables need complete blobs restored
     * at end of slice restore
     * That requires that we restore matching main and
     * parts table fragments
     * So we must ensure that we slice deterministically
     * across main and parts tables for Blobs tables.
     * The id of the 'main' table is used to give some
     * offsetting
     */
    const Uint32 mainId = table->getMainTable() ?
      table->getMainTable()->getTableId() :  // Parts table
      table->getTableId();                   // Main table

    fragmentRestoreSlice = (mainId + fragmentId) % ga_num_slices;
  }
  else
  {
    /* For non-Blob tables we use round-robin so
     * that we can balance across a number of slices
     * different to the number of fragments
     */
    fragmentRestoreSlice = fragmentCount ++ % ga_num_slices;
  }

  restoreLogger.log_debug("Table : %s blobRelated : %u frag id : %u "
                          "slice id : %u fragmentRestoreSlice : %u "
                          "apply : %u",
                          table->m_dictTable->getName(),
                          table->isBlobRelated(),
                          fragmentId,
                          ga_slice_id,
                          fragmentRestoreSlice,
                          (fragmentRestoreSlice == ga_slice_id));

  /* If it's not for this slice, skip it */
  const bool skip_fragment = (fragmentRestoreSlice != ga_slice_id);

  /* Remember for later lookup */
  table->setSliceSkipFlag(fragmentId, skip_fragment);

  return skip_fragment;
}

/**
 * Check result of previous determination about whether
 * to skip this fragment in slice mode
 */
static
bool check_slice_skip_fragment(const TableS* table, Uint32 fragmentId)
{
  if (ga_num_slices == 1)
  {
    /* No slicing */
    return false;
  }

  return table->getSliceSkipFlag(fragmentId);
}

int do_restore(RestoreThreadData *thrdata)
{
  init_progress();

  Vector<BackupConsumer*> &g_consumers = thrdata->m_consumers;
  char threadName[15] = "";
  if (opt_show_part_id)
    BaseString::snprintf(threadName, sizeof(threadName), "[part %u] ", thrdata->m_part_id);
  restoreLogger.setThreadPrefix(threadName);

  /**
   * we must always load meta data, even if we will only print it to stdout
   */

  restoreLogger.log_debug("Start restoring meta data");

  RestoreMetaData metaData(ga_backupPath, ga_nodeId, ga_backupId, thrdata->m_part_id, ga_part_count);
#ifdef ERROR_INSERT
  if(_error_insert > 0)
  {
    metaData.error_insert(_error_insert);
  }
  for (Uint32 i = 0; i < g_consumers.size(); i++)
  {
    g_consumers[i]->error_insert(_error_insert);
  }
#endif 
  restoreLogger.log_info("[restore_metadata] Read meta data file header");

  if (!metaData.readHeader())
  {
    restoreLogger.log_error("Failed to read %s", metaData.getFilename());
    return NdbToolsProgramExitCode::FAILED;
  }

  {
    const BackupFormat::FileHeader & tmp = metaData.getFileHeader();
    const Uint32 backupFileVersion = tmp.BackupVersion;
    const Uint32 backupNdbVersion = tmp.NdbVersion;
    const Uint32 backupMySQLVersion = tmp.MySQLVersion;
  
    char buf[NDB_VERSION_STRING_BUF_SZ];
    info.setLevel(254);

    if (backupFileVersion >= NDBD_RAW_LCP)
    {
      restoreLogger.log_info("Backup from version : %s file format : %x",
                             ndbGetVersionString(backupNdbVersion, backupMySQLVersion, 0,
                                                 buf, sizeof(buf)),
                             backupFileVersion);
    }
    else
    {
      restoreLogger.log_info("Backup file format : %x",
                             backupFileVersion);
    }

    /**
     * check whether we can restore the backup (right version).
     */
    // in these versions there was an error in how replica info was
    // stored on disk
    if (backupFileVersion >= MAKE_VERSION(5,1,3) && backupFileVersion <= MAKE_VERSION(5,1,9))
    {
      char new_buf[NDB_VERSION_STRING_BUF_SZ];
      restoreLogger.log_error("Restore program incompatible with backup file versions between %s and %s"
                              ,ndbGetVersionString(MAKE_VERSION(5,1,3), 0, 0, buf, sizeof(buf))
                              ,ndbGetVersionString(MAKE_VERSION(5,1,9), 0, 0, new_buf, sizeof(new_buf))
                              );
      return NdbToolsProgramExitCode::FAILED;
    }

    if (backupFileVersion > NDB_VERSION)
    {
      restoreLogger.log_error("Restore program older than backup version. Not supported. Use new restore program");
      return NdbToolsProgramExitCode::FAILED;
    }
  }

  restoreLogger.log_debug("Load content");
  restoreLogger.log_info("[restore_metadata] Load content");

  int res  = metaData.loadContent();

  restoreLogger.log_info("Stop GCP of Backup: %u", metaData.getStopGCP());
  restoreLogger.log_info("Start GCP of Backup: %u", metaData.getStartGCP());
  
  if (res == 0)
  {
    restoreLogger.log_error("Restore: Failed to load content");
    return NdbToolsProgramExitCode::FAILED;
  }
  restoreLogger.log_debug("Get number of Tables");
  restoreLogger.log_info("[restore_metadata] Get number of Tables");
  if (metaData.getNoOfTables() == 0)
  {
    restoreLogger.log_error("The backup contains no tables");
    return NdbToolsProgramExitCode::FAILED;
  }

  if (_print_sql_log && _print_log)
  {
    restoreLogger.log_debug("Check to ensure that both print-sql-log and print-log options are not passed");
    restoreLogger.log_error("Both print-sql-log and print-log options passed. Exiting...");
    return NdbToolsProgramExitCode::FAILED;
  }

  if (_print_sql_log)
  {
    restoreLogger.log_debug("Check for tables with hidden PKs or column of type blob when print-sql-log option is passed");
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
      if (tableNameParts[2].starts_with("NDB$BLOB"))
      {
        restoreLogger.log_error("Found column of type blob with print-sql-log option set. Exiting..." );
        return NdbToolsProgramExitCode::FAILED;
      }
      /* Hidden PKs are stored with the name $PK */
      int noOfPK = table->m_dictTable->getNoOfPrimaryKeys();
      for (int j = 0; j < noOfPK; j++)
      {
        const char* pkName = table->m_dictTable->getPrimaryKey(j);
        if (strcmp(pkName, "$PK") == 0)
        {
          restoreLogger.log_error("Found hidden primary key with print-sql-log option set. Exiting...");
          return NdbToolsProgramExitCode::FAILED;
        }
      }
    }
  }

  restoreLogger.log_debug("Validate Footer");
  restoreLogger.log_info("[restore_metadata] Validate Footer");

  if (!metaData.validateFooter())
  {
    restoreLogger.log_error("Restore: Failed to validate footer.");
    return NdbToolsProgramExitCode::FAILED;
  }
  restoreLogger.log_debug("Init Backup objects");
  Uint32 i;
  for (i = 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init(g_tableCompabilityMask))
    {
      restoreLogger.log_error("Failed to initialize consumers");
      return NdbToolsProgramExitCode::FAILED;
    }
  }

  if(ga_exclude_missing_tables)
    exclude_missing_tables(metaData, thrdata->m_consumers);

  if (!setup_column_remappings(metaData))
  {
    return NdbToolsProgramExitCode::FAILED;
  }

  /* report to clusterlog if applicable */
  for (i = 0; i < g_consumers.size(); i++)
    g_consumers[i]->report_started(ga_backupId, ga_nodeId);

  /* before syncing on m_barrier, check if any threads have already exited */
  if (ga_error_thread > 0)
  {
    return NdbToolsProgramExitCode::FAILED;
  }

  if (!thrdata->m_restore_meta)
  {
    /**
     * Only thread 1 is allowed to restore metadata objects. restore_meta
     * flag is set to true on thread 1, which causes consumer-restore to
     * actually restore the metadata objects,
     * e.g. g_consumer->object(tablespace) restores the tablespace
     *
     * Remaining threads have restore_meta = false, which causes
     * consumer-restore to query metadata objects and save metadata for
     * reference by later phases of restore
     * e.g. g_consumer->object(tablespace) queries+saves tablespace metadata
     *
     * So thread 1 must finish restoring all metadata objects before any other
     * thread is allowed to start metadata restore. Use CyclicBarrier to allow
     * all threads except thread-1 to arrive at barrier. Barrier will not be
     * opened until all threads arrive at it, so all threads will wait till
     * thread 1 arrives at barrier. When thread 1 completes metadata restore,
     * it arrives at barrier, opening barrier and allowing all threads to
     * proceed to next restore-phase.
     */
    if (!thrdata->m_barrier->wait())
    {
      ga_error_thread = thrdata->m_part_id;
      return NdbToolsProgramExitCode::FAILED;
    }
  }
  restoreLogger.log_debug("Restore objects (tablespaces, ..)");
  restoreLogger.log_info("[restore_metadata] Restore objects (tablespaces, ..)");
  for (i = 0; i < metaData.getNoOfObjects(); i++)
  {
    for (Uint32 j = 0; j < g_consumers.size(); j++)
      if (!g_consumers[j]->object(metaData.getObjType(i),
                                  metaData.getObjPtr(i)))
      {
        restoreLogger.log_error(
          "Restore: Failed to restore table: %s ... Exiting",
          metaData[i]->getTableName());
        return NdbToolsProgramExitCode::FAILED;
      } 
    if (check_progress())
    {
      info.setLevel(255);
      restoreLogger.log_info(" Object create progress: %u objects out of %u",
                             i+1, metaData.getNoOfObjects());
    }
  }

  restoreLogger.log_debug("Handling index stat tables");
  for (i = 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->handle_index_stat_tables())
    {
      restoreLogger.log_error(
          "Restore: Failed to handle index stat tables ... Exiting ");
      return NdbToolsProgramExitCode::FAILED;
    }
  }

  Vector<OutputStream *> table_output(metaData.getNoOfTables());
  restoreLogger.log_debug("Restoring tables");
  restoreLogger.log_info("[restore_metadata] Restoring tables");

  for(i = 0; i<metaData.getNoOfTables(); i++)
  {
    const TableS *table= metaData[i];
    table_output.push_back(NULL);
    if (!checkDbAndTableName(table))
      continue;
    if (isSYSTAB_0(table) ||
        (strcmp(table->getTableName(), NDB_REP_DB "/def/" NDB_APPLY_TABLE) == 0
         && ga_with_apply_status))
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
          return NdbToolsProgramExitCode::FAILED;
        }
        FileOutputStream *f= new FileOutputStream(res);
        table_output[i]= f;
      }
      for (Uint32 j = 0; j < g_consumers.size(); j++)
      {
        if (!g_consumers[j]->table(* table))
        {
          restoreLogger.log_error(
            "Restore: Failed to restore table: `%s` ... Exiting ",
            table->getTableName());
          return NdbToolsProgramExitCode::FAILED;
        }
      }
    }
    else
    {
      for (Uint32 j = 0; j < g_consumers.size(); j++)
      {
        if (!g_consumers[j]->createSystable(* table))
        {
          restoreLogger.log_error(
            "Restore: Failed to restore system table: `%s` ... Exiting",
            table->getTableName());
          return NdbToolsProgramExitCode::FAILED;
        }
      }
    }
    if (check_progress())
    {
      info.setLevel(255);
      restoreLogger.log_info("Table create progress: %u tables out of %u",
           i+1, metaData.getNoOfTables());
    }
  }

  restoreLogger.log_debug("Save foreign key info");
  restoreLogger.log_info("[restore_metadata] Save foreign key info");
  for (i = 0; i < metaData.getNoOfObjects(); i++)
  {
    for (Uint32 j = 0; j < g_consumers.size(); j++)
    {
      if (!g_consumers[j]->fk(metaData.getObjType(i),
                              metaData.getObjPtr(i)))
      {
        return NdbToolsProgramExitCode::FAILED;
      }
    }
  }

  restoreLogger.log_debug("Close tables" );
  for (i = 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->endOfTables())
    {
      restoreLogger.log_error("Restore: Failed while closing tables");
      return NdbToolsProgramExitCode::FAILED;
    } 
    if (!ga_disable_indexes && !ga_rebuild_indexes)
    {
      if (!g_consumers[i]->endOfTablesFK())
      {
        restoreLogger.log_error("Restore: Failed while closing tables FKs");
        return NdbToolsProgramExitCode::FAILED;
      }
    }
  }

  /* before syncing on m_barrier, check if any threads have already exited */
  if (ga_error_thread > 0)
  {
    return NdbToolsProgramExitCode::FAILED;
  }

  if (thrdata->m_restore_meta)
  {
    // thread 1 arrives at barrier -> barrier opens -> all threads continue
    if (!thrdata->m_barrier->wait())
    {
      ga_error_thread = thrdata->m_part_id;
      return NdbToolsProgramExitCode::FAILED;
    }
  }

  /* report to clusterlog if applicable */
  for(i= 0; i < g_consumers.size(); i++)
  {
    g_consumers[i]->report_meta_data(ga_backupId, ga_nodeId);
  }
  restoreLogger.log_debug("Iterate over data");
  restoreLogger.log_info("[restore_data] Start restoring table data");
  if (ga_restore || ga_print) 
  {
    Uint32 fragmentsTotal = 0;
    Uint32 fragmentsRestored = 0;
    if(_restore_data || _print_data)
    {
      // Check table compatibility
      for (i=0; i < metaData.getNoOfTables(); i++){
        if ((checkSysTable(metaData, i) &&
            checkDbAndTableName(metaData[i])) ||
            (strcmp(metaData[i]->getTableName(), NDB_REP_DB "/def/" NDB_APPLY_TABLE) == 0
             && ga_with_apply_status))
        {
          TableS & tableS = *metaData[i]; // not const
          for(Uint32 j = 0; j < g_consumers.size(); j++)
          {
            if (!g_consumers[j]->table_compatible_check(tableS))
            {
              restoreLogger.log_error("Restore: Failed to restore data, %s table structure incompatible with backup's ... Exiting ", tableS.getTableName());
              return NdbToolsProgramExitCode::FAILED;
            } 
            if (tableS.m_staging &&
                !g_consumers[j]->prepare_staging(tableS))
            {
              restoreLogger.log_error("Restore: Failed to restore data, %s failed to prepare staging table for data conversion ... Exiting", tableS.getTableName());
              return NdbToolsProgramExitCode::FAILED;
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
          if (isBlobTable(&tableS))
          {
            for (Uint32 j = 0; j < g_consumers.size(); j++)
            {
              if (!g_consumers[j]->check_blobs(tableS))
              {
                restoreLogger.log_error(
                  "Restore: Failed to restore data, "
                  "%s table's blobs incompatible with backup's ... Exiting ",
                  tableS.getTableName());
                return NdbToolsProgramExitCode::FAILED;
              }
            }
          }
        }
      }
        
      RestoreDataIterator dataIter(metaData, &free_data_callback, (void*)thrdata);

      if (!dataIter.validateBackupFile())
      {
        restoreLogger.log_error(
          "Unable to allocate memory for BackupFile constructor");
        return NdbToolsProgramExitCode::FAILED;
      }


      if (!dataIter.validateRestoreDataIterator())
      {
          restoreLogger.log_error("Unable to allocate memory for RestoreDataIterator constructor");
          return NdbToolsProgramExitCode::FAILED;
      }

      restoreLogger.log_info("[restore_data] Read data file header");

      // Read data file header
      if (!dataIter.readHeader())
      {
        restoreLogger.log_error(
          "Failed to read header of data file. Exiting...");
        return NdbToolsProgramExitCode::FAILED;
      }

      restoreLogger.log_info("[restore_data] Restore fragments");

      Uint32 fragmentCount = 0;
      Uint32 fragmentId; 
      while (dataIter.readFragmentHeader(res= 0, &fragmentId))
      {
        TableS* table = dataIter.getCurrentTable();
        OutputStream *output = table_output[table->getLocalId()];

        /**
         * Check whether we should skip the entire fragment
         */
        bool skipFragment = true;
        if (output == NULL)
        {
          restoreLogger.log_info("  Skipping fragment");
        }
        else
        {
          fragmentsTotal++;
          skipFragment = determine_slice_skip_fragment(table,
                                                       fragmentId,
                                                       fragmentCount);
          if (skipFragment)
          {
            restoreLogger.log_info("  Skipping fragment on this slice");
          }
          else
          {
            fragmentsRestored++;
          }
        }

        /**
         * Iterate over all rows stored in the data file for
         * this fragment
         */
        const TupleS* tuple;
#ifdef ERROR_INSERT
        Uint64 rowCount = 0;
#endif
	while ((tuple = dataIter.getNextTuple(res= 1, skipFragment)) != 0)
        {
          assert(output && !skipFragment);
#ifdef ERROR_INSERT
          if ((_error_insert == NDB_RESTORE_ERROR_INSERT_SKIP_ROWS) &&
              ((++rowCount % 3) == 0))
          {
            restoreLogger.log_info("Skipping row on error insertion");
            continue;
          }
#endif
          OutputStream *tmp = ndbout.m_out;
          ndbout.m_out = output;
          for(Uint32 j= 0; j < g_consumers.size(); j++) 
          {
            if (!g_consumers[j]->tuple(* tuple, fragmentId))
            {
              restoreLogger.log_error(
                "Restore: error occurred while restoring data. Exiting...");
              // wait for async transactions to complete
              for (i= 0; i < g_consumers.size(); i++)
                g_consumers[i]->endOfTuples();
              return NdbToolsProgramExitCode::FAILED;
            }
          }
          ndbout.m_out =  tmp;
          if (check_progress())
            report_progress("Data file progress: ", dataIter);
	} // while (tuple != NULL);
	
        if (res < 0)
        {
          restoreLogger.log_error(
            "Restore: An error occurred while reading data. Exiting...");
          return NdbToolsProgramExitCode::FAILED;
        }
        if (!dataIter.validateFragmentFooter())
        {
          restoreLogger.log_error(
            "Restore: Error validating fragment footer. Exiting...");
          return NdbToolsProgramExitCode::FAILED;
        }
      }  // while (dataIter.readFragmentHeader(res))
      
      if (res < 0)
      {
        restoreLogger.log_error(
          "Restore: An error occurred while restoring data."
          "Exiting... res = %u",
          res);
        return NdbToolsProgramExitCode::FAILED;
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

      restoreLogger.log_info("[restore_log] Read log file header");

      if (!logIter.readHeader())
      {
        restoreLogger.log_error(
          "Failed to read header of data file. Exiting...");
        return NdbToolsProgramExitCode::FAILED;
      }
      
      const LogEntry * logEntry = 0;

      restoreLogger.log_info("[restore_log] Restore log entries");

      while ((logEntry = logIter.getNextLogEntry(res= 0)) != 0)
      {
        const TableS* table = logEntry->m_table;
        OutputStream *output = table_output[table->getLocalId()];
        if (!output)
          continue;
        if (check_slice_skip_fragment(table, logEntry->m_frag_id))
          continue;
        for(Uint32 j= 0; j < g_consumers.size(); j++)
        {
          if (!g_consumers[j]->logEntry(* logEntry))
          {
            restoreLogger.log_error(
              "Restore: Error restoring the data log. Exiting...");
            return NdbToolsProgramExitCode::FAILED;
          }
        }

        if (check_progress())
          report_progress("Log file progress: ", logIter);
      }
      if (res < 0)
      {
        restoreLogger.log_error(
          "Restore: Error reading the data log. Exiting... res = %d",
          res);
        return NdbToolsProgramExitCode::FAILED;
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
          for (Uint32 j = 0; j < g_consumers.size(); j++)
          {
            if (!g_consumers[j]->finalize_staging(*table))
            {
              restoreLogger.log_error(
                "Restore: Failed staging data to table: %s. Exiting...",
                table->getTableName());
              return NdbToolsProgramExitCode::FAILED;
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
        {
          continue;
        }
        for (Uint32 j = 0; j < g_consumers.size(); j++)
        {
          if (!g_consumers[j]->finalize_table(*table))
          {
            restoreLogger.log_error(
              "Restore: Failed to finalize restore table: %s. Exiting...",
              metaData[i]->getTableName());
            return NdbToolsProgramExitCode::FAILED;
          }
        }
      }
      if (ga_num_slices != 1)
      {
        restoreLogger.log_info("Restore: Slice id %u/%u restored %u/%u fragments.",
                               ga_slice_id,
                               ga_num_slices,
                               fragmentsRestored,
                               fragmentsTotal);
      };
    }
  }

  if (ga_error_thread > 0)
  {
    restoreLogger.log_error("Thread %u exits on error", thrdata->m_part_id);
    // thread 1 failed to restore metadata, exiting
    return NdbToolsProgramExitCode::FAILED;  
  }

  if(ga_with_apply_status)
  {
    /**
     * Wait for all the threads to finish restoring data before attempting to
     * delete the tuple with server_id = 0 in ndb_apply_status table.
     * Later, the appropriate data for that tuple is generated when ndb_restore
     * is with invoked with restore-epoch option.
     */
    if (!thrdata->m_barrier->wait())
    {
      ga_error_thread = thrdata->m_part_id;
      return NdbToolsProgramExitCode::FAILED;
    }
    for (i= 0; i < g_consumers.size(); i++)
    {
      if (!g_consumers[i]->delete_epoch_tuple())
      {
        restoreLogger.log_error("Restore: Failed to delete tuple with server_id=0");
        return NdbToolsProgramExitCode::FAILED;
      }
    }
  }

  if (ga_error_thread > 0)
  {
    restoreLogger.log_error("Thread %u exits on error", thrdata->m_part_id);
    return NdbToolsProgramExitCode::FAILED;
  }

  if (ga_restore_epoch)
  {
    restoreLogger.log_info("[restore_epoch] Restoring epoch");
    RestoreLogIterator logIter(metaData);

    if (!logIter.readHeader())
    {
      err << "Failed to read snapshot info from log file. Exiting..." << endl;
      return NdbToolsProgramExitCode::FAILED;
    }
    bool snapshotstart = logIter.isSnapshotstartBackup();
    for (i= 0; i < g_consumers.size(); i++)
      if (!g_consumers[i]->update_apply_status(metaData, snapshotstart))
      {
        restoreLogger.log_error("Restore: Failed to restore epoch");
        return NdbToolsProgramExitCode::FAILED;
      }
  }

  if (ga_error_thread > 0)
  {
    restoreLogger.log_error("Thread %u exits on error", thrdata->m_part_id);
    return NdbToolsProgramExitCode::FAILED;  
  }

  unsigned j;
  for(j= 0; j < g_consumers.size(); j++) 
  {
    if (g_consumers[j]->has_temp_error())
    {
      ndbout_c("\nRestore successful, but encountered temporary error, "
               "please look at configuration.");
    }               
  }

  if (ga_rebuild_indexes)
  {
    /**
     * Index rebuild should not be allowed to start until all threads have
     * finished restoring data and epoch values are sorted out.
     * Wait until all threads have arrived at barrier, then allow all
     * threads to continue. Thread 1 will then rebuild indices, while all
     * other threads do nothing.
     */
    if (!thrdata->m_barrier->wait())
    {
      ga_error_thread = thrdata->m_part_id;
      return NdbToolsProgramExitCode::FAILED;
    }
    restoreLogger.log_debug("Rebuilding indexes");
    restoreLogger.log_info("[rebuild_indexes] Rebuilding indexes");

    for(i = 0; i<metaData.getNoOfTables(); i++)
    {
      const TableS *table= metaData[i];
      if (! (checkSysTable(table) && checkDbAndTableName(table)))
        continue;
      if (isBlobTable(table) || isIndex(table))
        continue;
      for (Uint32 j = 0; j < g_consumers.size(); j++)
      {
        if (!g_consumers[j]->rebuild_indexes(* table))
        {
          return NdbToolsProgramExitCode::FAILED;
        }
      }
    }
    for (Uint32 j = 0; j < g_consumers.size(); j++)
    {
      if (!g_consumers[j]->endOfTablesFK())
      {
        return NdbToolsProgramExitCode::FAILED;
      }
    }
  }

  if (ga_error_thread > 0)
  {
    restoreLogger.log_error("Thread %u exits on error", thrdata->m_part_id);
    // thread 1 failed to restore metadata, exiting
    return NdbToolsProgramExitCode::FAILED;
  }

  /* report to clusterlog if applicable */
  for (i = 0; i < g_consumers.size(); i++)
    g_consumers[i]->report_completed(ga_backupId, ga_nodeId);

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
  return NdbToolsProgramExitCode::OK;
}  // do_restore

/* Detects the backup type (single part or multiple parts) by locating
 * the ctl file. It sets the backup format as BF_SINGLE/BF_MULTI_PART
 * for future file-handling. Also counts the parts to be restored.
 */
int detect_backup_format()
{
  // construct name of ctl file
  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  BaseString::snprintf(name, sz, "%s%sBACKUP-%u.%d.ctl",
          ga_backupPath, DIR_SEPARATOR, ga_backupId, ga_nodeId);
  MY_STAT buf;
  if(my_stat(name, &buf, 0))
  {
    // for single part, backup path leads directly to ctl file
    // File-handlers search for the files in
    // BACKUP_PATH/BACKUP-<backup_id>/
    // E.g. /usr/local/mysql/datadir/BACKUP/BACKUP-1
    ga_backup_format = BF_SINGLE;
    ga_part_count = 1;
  }
  else
  {
    // for multiple parts, backup patch has subdirectories which
    // contain ctl files
    // file-handlers search for files in
    // BACKUP_PATH/BACKUP-<backup-id>/BACKUP-<backup-id>.<part_id>/
    // E.g. /usr/local/mysql/datadir/BACKUP/BACKUP-1/BACKUP-1.2/
    ga_backup_format = BF_MULTI_PART;
    // count number of backup parts in multi-set backup
    for(ga_part_count = 1; ga_part_count <= g_max_parts; ga_part_count++)
    {
      // backup parts are named as BACKUP-<backupid>-PART-<part_id>-OF-<total_parts>
      // E.g. Part 2 of backup 3 which has 4 parts will be in the path
      //    BACKUP-3/BACKUP-3-PART-2-OF-4/
      // Try out different values of <total_parts> for PART-1 until correct total found
      // E.g. for total = 4,
      //      BACKUP-1-PART-1-OF-1 : not found, continue
      //      BACKUP-1-PART-1-OF-2 : not found, continue
      //      BACKUP-1-PART-1-OF-3 : not found, continue
      //      BACKUP-1-PART-1-OF-4 : FOUND, set ga_part_count and break
      BaseString::snprintf(name,
                           sz,
                           "%s%sBACKUP-%d-PART-1-OF-%u%sBACKUP-%u.%d.ctl",
                           ga_backupPath,
                           DIR_SEPARATOR,
                           ga_backupId,
                           ga_part_count,
                           DIR_SEPARATOR,
                           ga_backupId,
                           ga_nodeId);
      if (my_stat(name, &buf, 0))
      {
        info << "Found backup " << ga_backupId << " with " << ga_part_count
             << " backup parts" << endl;
        break;  // part found, end of parts
      }
      if (ga_part_count == g_max_parts)
      {
        err << "Failed to find backup " << ga_backupId << " in path "
            << ga_backupPath << endl;
        return NdbToolsProgramExitCode::FAILED;  // too many parts
      }
    }
  }
  return NdbToolsProgramExitCode::OK;
}  // detect_backup_format

static void* start_restore_worker(void *data)
{
  RestoreThreadData *rdata = (RestoreThreadData*)data;
  rdata->m_result = do_restore(rdata);
  if (rdata->m_result == NdbToolsProgramExitCode::FAILED)
  {
    info << "Thread " << rdata->m_part_id << " failed, exiting" << endl;
    ga_error_thread = rdata->m_part_id;
  }
  return 0;
}


int
main(int argc, char** argv)
{
  ndb_openssl_evp::library_init();
  NDB_INIT(argv[0]);

  const char *load_default_groups[]= { "mysql_cluster","ndb_restore",0 };
  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);

  if (!readArguments(opts, &argv))
  {
    exitHandler(NdbToolsProgramExitCode::FAILED);
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
  if(ga_with_apply_status)
  {
    if(!_restore_data && !_print_data && !_print_log && !_print_sql_log)
    {
      err << "--with-apply-status should only "
          << "be used along with any of the following options:" << endl;
      err << "--restore-data" << endl;
      err << "--print-data" << endl;
      err << "--print-log" << endl;
      err << "--print-sql-log" << endl;
      err << "Exiting..." << endl;
      exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
    }
    g_options.appfmt(" -w");
  }
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
  if (ga_num_slices > 1)
  {
    g_options.appfmt(" --num-slices=%u --slice-id=%u",
                     ga_num_slices,
                     ga_slice_id);
  }
  if (ga_allow_pk_changes)
    g_options.append(" --allow-pk-changes");
  if (ga_ignore_extended_pk_updates)
    g_options.append(" --ignore-extended-pk-updates");

  // determine backup format: simple or multi-part, and count parts
  int result = detect_backup_format();

  if (result != NdbToolsProgramExitCode::OK)
  {
    exitHandler(result);
  }

  init_restore();

  /* Slices */
  if (ga_num_slices < 1)
  {
    err << "Too few slices" << endl;
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
  }
  if ((ga_slice_id < 0) ||
      (ga_slice_id >= ga_num_slices))
  {
    err << "Slice id "
        << ga_slice_id
        << " out of range (0-"
        << ga_num_slices
        << ")" << endl;
    exitHandler(NdbToolsProgramExitCode::WRONG_ARGS);
  }
  else
  {
    if (ga_num_slices > 1)
    {
      printf("ndb_restore slice %d/%d\n",
             ga_slice_id,
             ga_num_slices);
    }
  }

  g_restoring_in_parallel = true;
  // check if single-threaded restore is necessary
  if (_print || _print_meta || _print_data || _print_log || _print_sql_log
             || ga_backup_format == BF_SINGLE)
  {
    g_restoring_in_parallel = false;
    for (int i=1; i<=ga_part_count; i++)
    {
     /*
      * do_restore uses its parameter 'partId' to select the backup part.
      * Each restore thread is started with a unique part ID.
      * E.g. while restoring BACKUP-2,
      * restore-thread 1 restores BACKUP-2/BACKUP-2-PART-1-OF-4,
      * restore-thread 3 restores BACKUP-2/BACKUP-2-PART-3-OF-4
      * and so on.
      * do_restore uses the backup format and partId to locate backup files.
      * The tid and backup type are passed to the file-handlers:
      * - RestoreMetadata: finds ctl file
      * - RestoreDataIterator: finds data file
      * - RestoreLogIterator: finds log file
      *
      * For BF_SINGLE, the file-handlers search for the files in
      *
      * BACKUP_PATH/BACKUP-<backup_id>/
      * E.g. /usr/local/mysql/datadir/BACKUP/BACKUP-1
      *
      * For BF_MULTI_PART, the file-handlers search in
      *
      * BACKUP_PATH/BACKUP-<backup-id>/BACKUP-<backup-id>-PART-<part_id>
      * -OF-<total>/
      * E.g. /usr/local/mysql/datadir/BACKUP/BACKUP-1/BACKUP-1-PART-2-OF-4/
      */
      CyclicBarrier barrier(1);
      RestoreThreadData thrdata(i, &barrier);
      if (!create_consumers(&thrdata))
      {
         info << "Failed to init restore thread for BACKUP-" << ga_backupId
              << "-PART-" << i << "-OF-" << ga_part_count << endl;
         ga_error_thread = i;
         break;
      }

      if (do_restore(&thrdata) == NdbToolsProgramExitCode::FAILED)
      {
        if (ga_backup_format == BF_SINGLE)
        {
          info << "Failed to restore BACKUP-" << ga_backupId << endl;
        }
        else
        {
          info << "Failed to restore BACKUP-" << ga_backupId
               << "-PART-" << i << "-OF-" << ga_part_count << endl;
        }
        ga_error_thread = i;
        clear_consumers(&thrdata);
        break;
      }
      clear_consumers(&thrdata);
    }
  }
  else
  {
   // create one restore thread per backup part
    Vector<RestoreThreadData*> thrdata;
    CyclicBarrier barrier(ga_part_count);

    /**
     * Divide data INSERT parallelism across parts, ensuring
     * each part has at least 1
     */
    ga_nParallelism /= ga_part_count;
    if (ga_nParallelism == 0)
      ga_nParallelism = 1;

    debug << "Part parallelism is " << ga_nParallelism << endl;

    for (int part_id=1; part_id<=ga_part_count; part_id++)
    {
      NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_MEAN;
      uint stack_size = 256 * 1024;
      char name[20];
      snprintf (name, sizeof(name), "restore%d", part_id);
      RestoreThreadData *data = new RestoreThreadData(part_id, &barrier);
      if (!create_consumers(data))
      {
        info << "Failed to init restore thread for BACKUP-" << ga_backupId
             << "-PART-" << part_id << "-OF-" << ga_part_count << endl;
        ga_error_thread = part_id;
        break;
      }
      NdbThread *thread = NdbThread_Create (start_restore_worker,
             (void**)(data), stack_size, name, prio);
      if(!thread)
      {
        info << "Failed to start restore thread for BACKUP-" << ga_backupId
             << "-PART-" << part_id << "-OF-" << ga_part_count << endl;
        ga_error_thread = part_id;
        break;
      }
      data->m_thread = thread;
      thrdata.push_back(data);
    }
    // join all threads
    for (Uint32 i=0; i<thrdata.size(); i++)
    {
      void *status;
      if (ga_error_thread > 0)
        barrier.cancel();

      NdbThread_WaitFor(thrdata[i]->m_thread, &status);
      NdbThread_Destroy(&thrdata[i]->m_thread);
    }
    for (int i=0; i<ga_part_count; i++)
    {
      clear_consumers(thrdata[i]);
      delete thrdata[i];
    }
    thrdata.clear();
  }

  cleanup_restore();

  if (ga_error_thread > 0)
  {
    exitHandler(NdbToolsProgramExitCode::FAILED);
  }

  ndb_openssl_evp::library_end();
  return NdbToolsProgramExitCode::OK;
}  // main

template class Vector<BackupConsumer*>;
template class Vector<OutputStream*>;
template class Vector<RestoreOption *>;
