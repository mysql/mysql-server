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

#include <getarg.h>
#include <Vector.hpp>
#include <ndb_limits.h>
#include <NdbTCP.h>
#include <NdbOut.hpp>

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

static const char* ga_connect_NDB = NULL;

/**
 * print and restore flags
 */
static bool ga_restore = false;
static bool ga_print = false;
bool
readArguments(const int argc, const char** argv) 
{

  int _print = 0;
  int _print_meta = 0;
  int _print_data = 0;
  int _print_log = 0;
  int _restore_data = 0;
  int _restore_meta = 0;
  
  
  struct getargs args[] = 
  {
    { "connect", 'c', arg_string, &ga_connect_NDB, 
      "NDB Cluster connection", "\"nodeid=<api id>;host=<hostname:port>\""},
    { "nodeid", 'n', arg_integer, &ga_nodeId, 
      "Backup files from node", "db node id"},
    { "backupid", 'b',arg_integer, &ga_backupId, "Backup id", "backup id"},
    { "print", '\0', arg_flag, &_print, 
      "Print data and log to stdout", "print data and log"},
    { "print_data", '\0', arg_flag, &_print_data, 
      "Print data to stdout", "print data"},
    { "print_meta", '\0', arg_flag, &_print_meta, 
      "Print meta data to stdout", "print meta data"},
    { "print_log", '\0', arg_flag, &_print_log, 
      "Print log to stdout", "print log"},
    { "restore_data", 'r', arg_flag, &_restore_data, 
      "Restore table data/logs into NDB Cluster using NDBAPI", 
      "Restore table data/log"},
    { "restore_meta", 'm', arg_flag, &_restore_meta, 
      "Restore meta data into NDB Cluster using NDBAPI", "Restore meta data"},
    { "parallelism", 'p', arg_integer, &ga_nParallelism, 
      "No of parallel transactions during restore of data."
      "(parallelism can be 1 to 1024)", 
      "Parallelism"},
#ifdef USE_MYSQL
    { "use_mysql", '\0', arg_flag, &use_mysql,
      "Restore meta data via mysql. Systab will be ignored. Data is restored "
      "using NDBAPI.", "use mysql"},
    {  "user", '\0', arg_string, &ga_user, "MySQL user", "Default: root"},
    {  "password", '\0', arg_string, &ga_password, "MySQL user's password", 
       "Default: \"\" "},
    {  "host", '\0', arg_string, &ga_host, "Hostname of MySQL server", 
       "Default: localhost"},
    {  "socket", '\0', arg_string, &ga_socket, "Path to  MySQL server socket file", 
       "Default: /tmp/mysql.sock"},
    {  "port", '\0', arg_integer, &ga_port, "Port number of MySQL server", 
       "Default: 3306"},
#endif
    { "dont_ignore_systab_0", 'f', arg_flag, &ga_dont_ignore_systab_0, 
      "Experimental. Do not ignore system table during restore.", 
      "dont_ignore_systab_0"}
    
  };
  
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;

  if (getarg(args, num_args, argc, argv, &optind) || 
      ga_nodeId == 0  ||
      ga_backupId == 0 ||
      ga_nParallelism  < 1 ||
      ga_nParallelism >1024) 
  {
    arg_printusage(args, num_args, argv[0], "<path to backup files>\n");
    return false;
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

  /**
   * Got segmentation fault when using the printer's attributes directly
   * in getargs... Do not have the time to found out why... this is faster...
   */
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
  if (argv[optind] != NULL) 
  {
    ga_backupPath = argv[optind];
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

int
main(int argc, const char** argv)
{
  if (!readArguments(argc, argv))
  {
    return -1;
  }

  if (ga_connect_NDB != NULL) 
  {
    // Use connection string
    Ndb::setConnectString(ga_connect_NDB);
  }

  /**
   * we must always load meta data, even if we will only print it to stdout
   */
  RestoreMetaData metaData(ga_backupPath, ga_nodeId, ga_backupId);
  if (!metaData.readHeader())
  {
    ndbout << "Failed to read " << metaData.getFilename() << endl << endl;
    return -1;
  }
  /**
   * check wheater we can restore the backup (right version).
   */
  int res  = metaData.loadContent();

  if (res == 0)
  {
    ndbout_c("Restore: Failed to load content");
    return -1;
  }
  
  if (metaData.getNoOfTables() == 0) 
  {
    ndbout_c("Restore: The backup contains no tables ");
    return -1;
  }


  if (!metaData.validateFooter()) 
  {
    ndbout_c("Restore: Failed to validate footer.");
    return -1;
  }

  Uint32 i;
  for(i= 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init())
    {
      clearConsumers();
      return -11;
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
	  return -11;
	} 
    }
  }
  
  for(i= 0; i < g_consumers.size(); i++)
    if (!g_consumers[i]->endOfTables())
    {
      ndbout_c("Restore: Failed while closing tables");
      return -11;
    } 
  
  if (ga_restore || ga_print) 
  {
    if (ga_restore) 
    {
      RestoreDataIterator dataIter(metaData, &free_data_callback);
      
      // Read data file header
      if (!dataIter.readHeader())
      {
	ndbout << "Failed to read header of data file. Exiting..." ;
	return -11;
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
	  return -1;
	}
	if (!dataIter.validateFragmentFooter()) {
	  ndbout_c("Restore: Error validating fragment footer. "
		   "Exiting...");
	  return -1;
	}
      } // while (dataIter.readFragmentHeader(res))
      
      if (res < 0)
      {
	err << "Restore: An error occured while restoring data. Exiting... res=" << res << endl;
	return -1;
      }
      
      
      dataIter.validateFooter(); //not implemented
      
      for (i= 0; i < g_consumers.size(); i++)
	g_consumers[i]->endOfTuples();

      RestoreLogIterator logIter(metaData);
      if (!logIter.readHeader())
      {
	err << "Failed to read header of data file. Exiting..." << endl;
	return -1;
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
	err << "Restore: An restoring the data log. Exiting... res=" << res << endl;
	return -1;
      }
      logIter.validateFooter(); //not implemented
      for (i= 0; i < g_consumers.size(); i++)
	g_consumers[i]->endOfLogEntrys();
    }
  }
  clearConsumers();
  return 0;
} // main

template class Vector<BackupConsumer*>;
