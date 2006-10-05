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
#include <NdbMem.h>
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

static const char *opt_nodegroup_map_str= 0;
static unsigned opt_nodegroup_map_len= 0;
static NODE_GROUP_MAP opt_nodegroup_map[MAX_NODE_GROUP_MAPS];
#define OPT_NDB_NODEGROUP_MAP 'z'

NDB_STD_OPTS_VARS;

/**
 * print and restore flags
 */
static bool ga_restore_epoch = false;
static bool ga_restore = false;
static bool ga_print = false;
static int _print = 0;
static int _print_meta = 0;
static int _print_data = 0;
static int _print_log = 0;
static int _restore_data = 0;
static int _restore_meta = 0;
static int _no_restore_disk = 0;
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
  { "no-restore-disk-objects", 'd',
    "Dont restore disk objects (tablespace/logfilegroups etc)",
    (gptr*) &_no_restore_disk, (gptr*) &_no_restore_disk,  0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "restore_epoch", 'e', 
    "Restore epoch info into the status table. Convenient on a MySQL Cluster "
    "replication slave, for starting replication. The row in "
    NDB_REP_DB "." NDB_APPLY_TABLE " with id 0 will be updated/inserted.", 
    (gptr*) &ga_restore_epoch, (gptr*) &ga_restore_epoch,  0,
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
  { "backup_path", 260, "Path to backup files",
    (gptr*) &ga_backupPath, (gptr*) &ga_backupPath, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "dont_ignore_systab_0", 'f',
    "Experimental. Do not ignore system table during restore.", 
    (gptr*) &ga_dont_ignore_systab_0, (gptr*) &ga_dont_ignore_systab_0, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "ndb-nodegroup-map", OPT_NDB_NODEGROUP_MAP,
    "Nodegroup map for ndbcluster. Syntax: list of (source_ng, dest_ng)",
    (gptr*) &opt_nodegroup_map_str,
    (gptr*) &opt_nodegroup_map_str,
    0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
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
  if (!end_ptr || number < 0 || number >= UNDEF_NODEGROUP)
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
      ng_map[i].map_array[j]= UNDEF_NODEGROUP;
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
    info << "Nodeid = " << ga_nodeId << endl;
    break;
  case 'b':
    if (ga_backupId == 0)
    {
      printf("Error in --backupid,-b setting, see --help\n");
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    info << "Backup Id = " << ga_backupId << endl;
    break;
  case OPT_NDB_NODEGROUP_MAP:
    /*
      This option is used to set a map from nodegroup in original cluster
      to nodegroup in new cluster.
    */
    opt_nodegroup_map_len= 0;
    info << "Analyse node group map" << endl;
    if (analyse_nodegroup_map(opt_nodegroup_map_str,
                              &opt_nodegroup_map[0]))
    {
      exit(NDBT_ProgramExit(NDBT_WRONGARGS));
    }
    break;
  }
  return 0;
}
bool
readArguments(int *pargc, char*** pargv) 
{
  Uint32 i;
  debug << "Load defaults" << endl;
  const char *load_default_groups[]= { "mysql_cluster","ndb_restore",0 };

  init_nodegroup_map();
  load_defaults("my",load_default_groups,pargc,pargv);
  debug << "handle_options" << endl;
  if (handle_options(pargc, pargv, my_long_options, get_one_option))
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

  BackupPrinter* printer = new BackupPrinter(opt_nodegroup_map,
                                             opt_nodegroup_map_len);
  if (printer == NULL)
    return false;

  BackupRestore* restore = new BackupRestore(opt_nodegroup_map,
                                             opt_nodegroup_map_len,
                                             ga_nParallelism);
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

  if (_no_restore_disk)
  {
    restore->m_no_restore_disk = true;
  }
  
  if (ga_restore_epoch)
  {
    restore->m_restore_epoch = true;
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
  info << "backup path = " << ga_backupPath << endl;
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
  return ga_dont_ignore_systab_0 || ! table->getSysTable();
}

static inline bool
checkSysTable(const RestoreMetaData& metaData, uint i)
{
  assert(i < metaData.getNoOfTables());
  return checkSysTable(metaData[i]);
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

  debug << "Start readArguments" << endl;
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
  if (ga_restore_epoch)
    g_options.appfmt(" -e");
  if (_no_restore_disk)
    g_options.appfmt(" -d");
  g_options.appfmt(" -p %d", ga_nParallelism);

  g_connect_string = opt_connect_str;

  /**
   * we must always load meta data, even if we will only print it to stdout
   */
  debug << "Start restoring meta data" << endl;
  RestoreMetaData metaData(ga_backupPath, ga_nodeId, ga_backupId);
  if (!metaData.readHeader())
  {
    err << "Failed to read " << metaData.getFilename() << endl << endl;
    exitHandler(NDBT_FAILED);
  }

  const BackupFormat::FileHeader & tmp = metaData.getFileHeader();
  const Uint32 version = tmp.NdbVersion;
  
  char buf[NDB_VERSION_STRING_BUF_SZ];
  info << "Ndb version in backup files: " 
	 <<  getVersionString(version, 0, buf, sizeof(buf)) << endl;

  /**
   * check wheater we can restore the backup (right version).
   */
  // in these versions there was an error in how replica info was
  // stored on disk
  if (version >= MAKE_VERSION(5,1,3) && version <= MAKE_VERSION(5,1,9))
  {
    err << "Restore program incompatible with backup versions between "
        << getVersionString(MAKE_VERSION(5,1,3), 0, buf, sizeof(buf))
        << " and "
        << getVersionString(MAKE_VERSION(5,1,9), 0, buf, sizeof(buf))
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
  int res  = metaData.loadContent();
  
  if (res == 0)
  {
    err << "Restore: Failed to load content" << endl;
    exitHandler(NDBT_FAILED);
  }
  debug << "Get no of Tables" << endl; 
  if (metaData.getNoOfTables() == 0) 
  {
    err << "The backup contains no tables" << endl;
    exitHandler(NDBT_FAILED);
  }
  debug << "Validate Footer" << endl;

  if (!metaData.validateFooter()) 
  {
    err << "Restore: Failed to validate footer." << endl;
    exitHandler(NDBT_FAILED);
  }
  debug << "Init Backup objects" << endl;
  Uint32 i;
  for(i= 0; i < g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init())
    {
      clearConsumers();
      err << "Failed to initialize consumers" << endl;
      exitHandler(NDBT_FAILED);
    }

  }
  debug << "Restore objects (tablespaces, ..)" << endl;
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
  }
  debug << "Restoring tables" << endl; 
  for(i = 0; i<metaData.getNoOfTables(); i++)
  {
    if (checkSysTable(metaData, i))
    {
      for(Uint32 j= 0; j < g_consumers.size(); j++)
	if (!g_consumers[j]->table(* metaData[i]))
	{
	  err << "Restore: Failed to restore table: ";
          err << metaData[i]->getTableName() << " ... Exiting " << endl;
	  exitHandler(NDBT_FAILED);
	} 
    }
  }
  debug << "Close tables" << endl; 
  for(i= 0; i < g_consumers.size(); i++)
    if (!g_consumers[i]->endOfTables())
    {
      err << "Restore: Failed while closing tables" << endl;
      exitHandler(NDBT_FAILED);
    } 
  debug << "Iterate over data" << endl; 
  if (ga_restore || ga_print) 
  {
    if(_restore_data || _print_data)
    {
      RestoreDataIterator dataIter(metaData, &free_data_callback);
      
      // Read data file header
      if (!dataIter.readHeader())
      {
	err << "Failed to read header of data file. Exiting..." << endl;
	exitHandler(NDBT_FAILED);
      }
      
      Uint32 fragmentId; 
      while (dataIter.readFragmentHeader(res= 0, &fragmentId))
      {
	const TupleS* tuple;
	while ((tuple = dataIter.getNextTuple(res= 1)) != 0)
	{
	  if (checkSysTable(tuple->getTable()))
	    for(Uint32 i= 0; i < g_consumers.size(); i++) 
	      g_consumers[i]->tuple(* tuple, fragmentId);
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
	if (checkSysTable(logEntry->m_table))
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
	if (checkSysTable(metaData, i))
	{
	  for(Uint32 j= 0; j < g_consumers.size(); j++)
	    if (!g_consumers[j]->finalize_table(* metaData[i]))
	    {
	      err << "Restore: Failed to finalize restore table: %s. ";
              err << "Exiting... " << metaData[i]->getTableName() << endl;
	      exitHandler(NDBT_FAILED);
	    } 
	}
      }
    }
  }

  if (ga_restore_epoch)
  {
    for (i= 0; i < g_consumers.size(); i++)
      if (!g_consumers[i]->update_apply_status(metaData))
      {
	err << "Restore: Failed to restore epoch" << endl;
	return -1;
      }
  }

  for(Uint32 i= 0; i < g_consumers.size(); i++) 
  {
    if (g_consumers[i]->has_temp_error())
    {
      clearConsumers();
      ndbout_c("\nRestore successful, but encountered temporary error, "
               "please look at configuration.");
      return NDBT_ProgramExit(NDBT_TEMPORARY);
    }               
  }

  clearConsumers();
  return NDBT_ProgramExit(NDBT_OK);
} // main

template class Vector<BackupConsumer*>;
