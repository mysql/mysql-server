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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <Vector.hpp>
#include <ndb_limits.h>
#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <NDBT_ReturnCodes.h>

#include "consumer_restore.hpp"
#include "consumer_printer.hpp"

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static int ga_nodeId = 0;
static int ga_nParallelism = 128;
static int ga_backupId = 0;
static bool ga_dont_ignore_systab_0 = false;
static Vector<class BackupConsumer *> g_consumers;

static const char* ga_backupPath = "." DIR_SEPARATOR;

NDB_STD_OPTS_VARS;

/**
 * print and restore flags
 */
static bool ga_restore = false;
static bool ga_print = false;
static int _print = 0;
static int _print_meta = 0;
static int _print_data = 0;
static int _print_log = 0;
static int _restore_data = 0;
static int _restore_meta = 0;
BaseString g_options("ndb_restore");

const char *load_default_groups[]= { "mysql_cluster","ndb_restore",0 };

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_restore"),
  { "connect", 'c', "same as --connect-string",
    (gptr*) &opt_connect_str, (gptr*) &opt_connect_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodeid", 'n', "Backup files from node with id",
    (gptr*) &ga_nodeId, (gptr*) &ga_nodeId, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "backupid", 'b', "Backup id",
    (gptr*) &ga_backupId, (gptr*) &ga_backupId, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_data", 'r', 
    "Restore table data/logs into NDB Cluster using NDBAPI", 
    (gptr*) &_restore_data, (gptr*) &_restore_data,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_meta", 'm',
    "Restore meta data into NDB Cluster using NDBAPI",
    (gptr*) &_restore_meta, (gptr*) &_restore_meta,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "parallelism", 'p',
    "No of parallel transactions during restore of data."
    "(parallelism can be 1 to 1024)", 
    (gptr*) &ga_nParallelism, (gptr*) &ga_nParallelism, 0,
    GET_INT, REQUIRED_ARG, 128, 1, 1024, 0, 1, 0 },
  { "print", 256, "Print data and log to stdout",
    (gptr*) &_print, (gptr*) &_print, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_data", 257, "Print data to stdout", 
    (gptr*) &_print_data, (gptr*) &_print_data, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_meta", 258, "Print meta data to stdout",
    (gptr*) &_print_meta, (gptr*) &_print_meta,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_log", 259, "Print log to stdout",
    (gptr*) &_print_log, (gptr*) &_print_log,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "dont_ignore_systab_0", 'f',
    "Experimental. Do not ignore system table during restore.", 
    (gptr*) &ga_dont_ignore_systab_0, (gptr*) &ga_dont_ignore_systab_0, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS] [<path to backup files>]\n", my_progname);
}
static void usage()
{
  short_usage_sub();
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_restore.trace";
#endif
  ndb_std_get_one_option(optid, opt, argument);
  switch (optid) {
  case 'n':
    if (ga_nodeId == 0)
    {
      printf("Error in --nodeid,-n setting, see --help\n");
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    break;
  case 'b':
    if (ga_backupId == 0)
    {
      printf("Error in --backupid,-b setting, see --help\n");
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    break;
  }
  return 0;
}
bool
readArguments(int *pargc, char*** pargv) 
{
  load_defaults("my",load_default_groups,pargc,pargv);
  if (handle_options(pargc, pargv, my_long_options, get_one_option))
  {
    exit(NDBT_ProgramExit(NDBT_WRONGARGS));
  }

  BackupPrinter* printer = new BackupPrinter();
  if (printer == NULL)
    return false;

  BackupRestore* restore = new BackupRestore(ga_nParallelism);
  if (restore == NULL) 
  {
    delete printer;
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

  if (_restore_data)
  {
    ga_restore = true;
    restore->m_restore = true; 
  }

  if (_restore_meta)
  {
    //    ga_restore = true;
    restore->m_restore_meta = true;
  }

  {
    BackupConsumer * c = printer;
    g_consumers.push_back(c);
  }
  {
    BackupConsumer * c = restore;
    g_consumers.push_back(c);
  }
  // Set backup file path
  if (*pargv[0] != NULL) 
  {
    ga_backupPath = *pargv[0];
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

static bool
checkSysTable(const char *tableName) 
{
  return ga_dont_ignore_systab_0 ||
    (strcmp(tableName, "SYSTAB_0") != 0 &&
     strcmp(tableName, "NDB$EVENTS_0") != 0 &&
     strcmp(tableName, "sys/def/SYSTAB_0") != 0 &&
     strcmp(tableName, "sys/def/NDB$EVENTS_0") != 0);
}

static void
free_data_callback()
{
  for(Uint32 i= 0; i < g_consumers.size(); i++) 
    g_consumers[i]->tuple_free();
}

const char * g_connect_string = 0;
static void exitHandler(int code)
{
  NDBT_ProgramExit(code);
  if (opt_core)
    abort();
  else
    exit(code);
}

int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);

  if (!readArguments(&argc, &argv))
  {
    exitHandler(NDBT_FAILED);
  }

  g_options.appfmt(" -b %d", ga_backupId);
  g_options.appfmt(" -n %d", ga_nodeId);
  if (_restore_meta)
    g_options.appfmt(" -m");
  if (_restore_data)
    g_options.appfmt(" -r");
  g_options.appfmt(" -p %d", ga_nParallelism);

  g_connect_string = opt_connect_str;

  /**
   * we must always load meta data, even if we will only print it to stdout
   */
  RestoreMetaData metaData(ga_backupPath, ga_nodeId, ga_backupId);
  if (!metaData.readHeader())
  {
    ndbout << "Failed to read " << metaData.getFilename() << endl << endl;
    exitHandler(NDBT_FAILED);
  }

  const BackupFormat::FileHeader & tmp = metaData.getFileHeader();
  const Uint32 version = tmp.NdbVersion;
  
  char buf[NDB_VERSION_STRING_BUF_SZ];
  ndbout << "Ndb version in backup files: " 
	 <<  getVersionString(version, 0, buf, sizeof(buf)) << endl;
  
  /**
   * check wheater we can restore the backup (right version).
   */
  int res  = metaData.loadContent();
  
  if (res == 0)
  {
    ndbout_c("Restore: Failed to load content");
    exitHandler(NDBT_FAILED);
  }
  
  if (metaData.getNoOfTables() == 0) 
  {
    ndbout_c("Restore: The backup contains no tables ");
    exitHandler(NDBT_FAILED);
  }


  if (!metaData.validateFooter()) 
  {
    ndbout_c("Restore: Failed to validate footer.");
    exitHandler(NDBT_FAILED);
  }

  Uint32 i;
  for(i= 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init())
    {
      clearConsumers();
      exitHandler(NDBT_FAILED);
    }

  }

  for(i = 0; i<metaData.getNoOfTables(); i++)
  {
    if (checkSysTable(metaData[i]->getTableName()))
    {
      for(Uint32 j= 0; j < g_consumers.size(); j++)
	if (!g_consumers[j]->table(* metaData[i]))
	{
	  ndbout_c("Restore: Failed to restore table: %s. "
		   "Exiting...", 
		   metaData[i]->getTableName());
	  exitHandler(NDBT_FAILED);
	} 
    }
  }
  
  for(i= 0; i < g_consumers.size(); i++)
    if (!g_consumers[i]->endOfTables())
    {
      ndbout_c("Restore: Failed while closing tables");
      exitHandler(NDBT_FAILED);
    } 
  
  if (ga_restore || ga_print) 
  {
    if(_restore_data || _print_data)
    {
      RestoreDataIterator dataIter(metaData, &free_data_callback);
      
      // Read data file header
      if (!dataIter.readHeader())
      {
	ndbout << "Failed to read header of data file. Exiting..." ;
	exitHandler(NDBT_FAILED);
      }
      
      
      while (dataIter.readFragmentHeader(res= 0))
      {
	const TupleS* tuple;
	while ((tuple = dataIter.getNextTuple(res= 1)) != 0)
	{
	  if (checkSysTable(tuple->getTable()->getTableName()))
	    for(Uint32 i= 0; i < g_consumers.size(); i++) 
	      g_consumers[i]->tuple(* tuple);
	} // while (tuple != NULL);
	
	if (res < 0)
	{
	  ndbout_c("Restore: An error occured while restoring data. "
		   "Exiting...");
	  exitHandler(NDBT_FAILED);
	}
	if (!dataIter.validateFragmentFooter()) {
	  ndbout_c("Restore: Error validating fragment footer. "
		   "Exiting...");
	  exitHandler(NDBT_FAILED);
	}
      } // while (dataIter.readFragmentHeader(res))
      
      if (res < 0)
      {
	err << "Restore: An error occured while restoring data. Exiting... "
	    << "res=" << res << endl;
	exitHandler(NDBT_FAILED);
      }
      
      
      dataIter.validateFooter(); //not implemented
      
      for (i= 0; i < g_consumers.size(); i++)
	g_consumers[i]->endOfTuples();
    }

    if(_restore_data || _print_log)
    {
      RestoreLogIterator logIter(metaData);
      if (!logIter.readHeader())
      {
	err << "Failed to read header of data file. Exiting..." << endl;
	exitHandler(NDBT_FAILED);
      }
      
      const LogEntry * logEntry = 0;
      while ((logEntry = logIter.getNextLogEntry(res= 0)) != 0)
      {
	if (checkSysTable(logEntry->m_table->getTableName()))
	  for(Uint32 i= 0; i < g_consumers.size(); i++)
	    g_consumers[i]->logEntry(* logEntry);
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
    }
    
    if(_restore_data)
    {
      for(i = 0; i<metaData.getNoOfTables(); i++)
      {
	if (checkSysTable(metaData[i]->getTableName()))
	{
	  for(Uint32 j= 0; j < g_consumers.size(); j++)
	    if (!g_consumers[j]->finalize_table(* metaData[i]))
	    {
	      ndbout_c("Restore: Failed to finalize restore table: %s. "
		       "Exiting...", 
		       metaData[i]->getTableName());
	      exitHandler(NDBT_FAILED);
	    } 
	}
      }
    }
  }
  clearConsumers();
  return NDBT_ProgramExit(NDBT_OK);
} // main

template class Vector<BackupConsumer*>;
