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

#include "Restore.hpp"
#include <getarg.h>
#include <NdbSleep.h>
#include <Vector.hpp>
#include <ndb_limits.h>
#include <NdbTCP.h>
#ifdef USE_MYSQL
#include <mysql.h>
#endif

#include <NdbOut.hpp>

NdbOut& operator<<(NdbOut& ndbout, const TupleS& tuple);
NdbOut& operator<<(NdbOut& ndbout, const LogEntry& logEntry);
NdbOut& operator<<(NdbOut& ndbout, const RestoreMetaData &);

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static const char * delimiter = ";"; // Delimiter in file dump

static int ga_nodeId = 0;
static int ga_nParallelism = 1;
static int ga_backupId = 0;
static bool ga_dont_ignore_systab_0 = false;
static myVector<class BackupConsumer *> g_consumers;

#ifdef USE_MYSQL
/**
 * mysql specific stuff:
 */
static const char* ga_user = "root";
static const char* ga_host = "localhost";
static const char* ga_socket =  "/tmp/mysql.sock";
static const char* ga_password = "";
static const char* ga_database = "";
static int ga_port = 3306;
static bool  use_mysql = false;
static MYSQL mysql;
#endif


#ifdef NDB_WIN32
static const char* ga_backupPath = ".\\";
#else
static const char* ga_backupPath = "./";
#endif

typedef struct  {
  void * ndb;
  void * restore;
  TupleS * tup;
  int    transaction;
  int    retries;
} restore_callback_t;

static const char* ga_connect_NDB = NULL;
static const char* ga_schema = NULL;
static const char* ga_catalog = NULL;



/**
 * print and restore flags
 */
static bool ga_restore = false;
static bool ga_print = false;



class BackupConsumer {
public:
  virtual bool init() { return true;}
  virtual bool table(const TableS &){return true;}
#ifdef USE_MYSQL
  virtual bool table(const TableS &, MYSQL* mysqlp) {return true;};
#endif
  virtual void tuple(const TupleS &){}
  virtual void tupleAsynch(const TupleS &, restore_callback_t * callback) {};
  //  virtual bool asynchErrorHandler(NdbConnection * trans){return true;};
  virtual void asynchExitHandler(){};
  virtual void endOfTuples(){}
  virtual void logEntry(const LogEntry &){}
  virtual void endOfLogEntrys(){}
protected:
  int create_table_string(const TableS & table, char * ,char *);
};

class BackupPrinter : public BackupConsumer 
{
  NdbOut & m_ndbout;
public:
  BackupPrinter(NdbOut & out = ndbout) : m_ndbout(out) 
  {
    m_print = false;
    m_print_log = false;
    m_print_data = false;
    m_print_meta = false;
  }
  
  virtual bool table(const TableS &);
#ifdef USE_MYSQL
  virtual bool table(const TableS &, MYSQL* mysqlp);
#endif
  virtual void tuple(const TupleS &);
  virtual void logEntry(const LogEntry &);
  virtual void endOfTuples() {};
  virtual void endOfLogEntrys();
  virtual void tupleAsynch(const TupleS &, restore_callback_t * callback);
  bool m_print;
  bool m_print_log;
  bool m_print_data;
  bool m_print_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;
  
};

class BackupRestore : public BackupConsumer 
{
public:
  BackupRestore() 
  {
    m_ndb = 0;
    m_logCount = m_dataCount = 0;
    m_restore = false;
    m_restore_meta = false;
  }
  
  virtual ~BackupRestore();

  virtual bool init();
  virtual bool table(const TableS &);
#ifdef USE_MYSQL
  virtual bool table(const TableS &, MYSQL* mysqlp);
#endif
  virtual void tuple(const TupleS &);
  virtual void tupleAsynch(const TupleS &, restore_callback_t * callback);
  virtual void asynchExitHandler();
  virtual void endOfTuples();
  virtual void logEntry(const LogEntry &);
  virtual void endOfLogEntrys();
  void connectToMysql();
  Ndb * m_ndb;
  bool m_restore;
  bool m_restore_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;
};
bool
readArguments(const int argc, const char** argv) 
{
  BackupPrinter* printer = new BackupPrinter();
  if (printer == NULL)
    return false;
  BackupRestore* restore = new BackupRestore();
  if (restore == NULL) 
  {
    delete printer;
    return false;
  }

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
#if NDB_VERSION_MAJOR >= VERSION_3X
    { "catalog", 'd', arg_string, &ga_catalog, 
      "Specifies the catalog/database where the data should be restored to. "
      "Restores only to backups taken with v.2.x and restored on >v.3.x "
      "systems. Note: system tables (if restored) defaults to sys/def/ ",
      "catalog"},
    { "schema", 's', arg_string, &ga_schema, 
      "Specifies the schema where the data should be restored to."
      "Restores only to backups taken with v.2.x and restored on >v.3.x "
      "systems. Note: system tables (if restored) defaults to sys/def/ ",
      "schema"},
#endif
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
    delete printer;
    delete restore;
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
#ifdef USE_MYSQL
  if(use_mysql) {
    ga_dont_ignore_systab_0 = false;
    ga_database = ""; //not used yet. pethaps later if we want to 
                          // restore meta data in an existing mysql database,
                          // and not just restore it to the same database
                          // as when the backup was taken.
                          // If implementing this, then the 
                          // tupleAsynch must also be changed so that the 
                          // table data is restored to the correct table.
                          // also, mysql_select_db must be set properly (ie., 
                          // ignored in codw below)
  }
#endif

  return true;
}


void
clearConsumers()
{
  for(int i = 0; i<g_consumers.size(); i++)
    delete g_consumers[i];
  g_consumers.clear();
}

static bool asynchErrorHandler(NdbConnection * trans, Ndb * ndb);
static NdbConnection * asynchTrans[1024];

bool
checkSysTable(const char *tableName) 
{
  return ga_dont_ignore_systab_0 ||
    (strcmp(tableName, "SYSTAB_0") != 0 &&
     strcmp(tableName, "NDB$EVENTS_0") != 0 &&
     strcmp(tableName, "sys/def/SYSTAB_0") != 0 &&
     strcmp(tableName, "sys/def/NDB$EVENTS_0") != 0);
}


int
main(int argc, const char** argv)
{
  if (!readArguments(argc, argv))
  {
    return -1;
  }
  // Turn off table name completion
#if NDB_VERSION_MAJOR >= VERSION_3X
  Ndb::useFullyQualifiedNames(false);
#endif

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
   * check wheater we can restore the backup (right version, and if that
   * version needs catalog and schema specified.
   */
  int res  = metaData.loadContent(ga_catalog, ga_schema);

  if (res == 0) 
  {
    ndbout_c("Restore: Failed to load content");
    return -1;
  }
  if (res == -1) 
  {
    ndbout_c("Restore: The backup is from a NDB Cluster v.2.x version. "
	     "To restore this backup on a > 3.x version you must specify "
	     "catalog and schema.");
    return -1;
  }
  if (res == -2) 
  {
#ifdef NDB_VERSION
    ndbout_c("Restore: The backup is from a NDB Cluster v.3.x version "
	     "Catalog and schema are invalid parameters since they "
	     "already exist implicitly.");
#endif
#ifdef NDB_KERNEL_VERSION
    ndbout_c("Restore: The backup is from a NDB Cluster v.3.x version "
	     "It is not possible to restore a 3.x backup on v.2.x. ");
#endif
    return -1;
  }
  
  if (res == -3) 
  {
    ndbout_c("Restore: The backup contains no tables "
	     "Catalog and schema are invalid parameters. ");
    return -1;
  }


  if (!metaData.validateFooter()) 
  {
    ndbout_c("Restore: Failed to validate footer.");
    return -1;
  }


  for(int i = 0; i<g_consumers.size(); i++)
  {
    if (!g_consumers[i]->init())
    {
      clearConsumers();
      return -11;
    }

  }

  for(Uint32 i = 0; i<metaData.getNoOfTables(); i++)
  {
    if (checkSysTable(metaData[i]->getTableName()))
    {
      for(int j = 0; j<g_consumers.size(); j++)
#ifdef USE_MYSQL
	if(use_mysql) {
	  if (!g_consumers[j]->table(* metaData[i], &mysql))
	    {
	      ndbout_c("Restore: Failed to restore table: %s. "
		       "Exiting...", 
		       metaData[i]->getTableName());
	      return -11;
	    } 
	} else	  
#endif
	  if (!g_consumers[j]->table(* metaData[i]))
	  {
	    ndbout_c("Restore: Failed to restore table: %s. "
		     "Exiting...", 
		     metaData[i]->getTableName());
	    return -11;
	  } 
      
    }
  }
  


  if (ga_restore || ga_print) 
  {
      if (ga_restore) 
      {
	RestoreDataIterator dataIter(metaData);
	
	// Read data file header
	if (!dataIter.readHeader())
	{
	  ndbout << "Failed to read header of data file. Exiting..." ;
	  return -11;
	}
	
	
	while (dataIter.readFragmentHeader(res))
	{
	  const TupleS* tuple = 0;
	  while ((tuple = dataIter.getNextTuple(res)) != NULL)
	  {
	    if (checkSysTable(tuple->getTable()->getTableName()))
	    {
		  for(int i = 0; i<g_consumers.size(); i++) 
		    {
		      g_consumers[i]->tupleAsynch(* tuple, 0);
		    }
	    }
	  } while (tuple != NULL);
	   
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
	  ndbout_c("Restore: An error occured while restoring data. "
		   "Exiting...");
	  return -1;
	}
	
	
	dataIter.validateFooter(); //not implemented
	for (int i = 0; i<g_consumers.size(); i++)
	  g_consumers[i]->endOfTuples();
	
	RestoreLogIterator logIter(metaData);
	if (!logIter.readHeader())
	{
	  ndbout << "Failed to read header of data file. Exiting...";
	  return -1;
	}
	
	/**
	 * I have not touched the part below : -johan 040218
	 * except fixing return values.
	 */
	const LogEntry * logEntry = 0;
	while ((logEntry = logIter.getNextLogEntry(res)))
	{
	  if (checkSysTable(logEntry->m_table->getTableName()))
	  {
	    for(int i = 0; i<g_consumers.size(); i++)
	      g_consumers[i]->logEntry(* logEntry);
	  }
	}
	if (res < 0)
	{
	  ndbout_c("Restore: An restoring the data log"
		     "Exiting...");
	  return -1;
	}
	logIter.validateFooter(); //not implemented
	for (int i = 0; i<g_consumers.size(); i++)
	  g_consumers[i]->endOfLogEntrys();
      }
  }
  clearConsumers();
  return 1;
} // main

NdbOut &
operator<<(NdbOut& ndbout, const AttributeS& attr){
  const AttributeData & data = attr.Data;
  const AttributeDesc & desc = * attr.Desc;

  if (data.null)
  {
    ndbout << "<NULL>";
    return ndbout;
  }
  
  if (desc.arraySize > 1)
    ndbout << "[ ";
  for (Uint32 j = 0; j < desc.arraySize; j++) 
  {
    // Print strings without spaces, 
    //   (but ndbout char does not work as expected, see below)
    switch (desc.type) 
    {
    case Signed:
      switch (desc.size) 
      {
      case 8:
	ndbout << (short)data.int8_value[j];
	break;
      case 16:
	ndbout << data.int16_value[j];
	break;
      case 32:
	ndbout << data.int32_value[j];
	break;
      case 64:
	ndbout << data.int64_value[j];
	break;
      case 128:
	ndbout << "Signed sz = 128 - this is something wrong??" << endl;
	break;
      default:
	// Unknown, error
	break;
      } // switch size
      break;
    case UnSigned:
      switch (desc.size) 
      {
      case 8:
	ndbout << (short)data.u_int8_value[j];
	break;
      case 16:
	ndbout << data.u_int16_value[j];
	break;
      case 32:
	ndbout << data.u_int32_value[j];
	break;
      case 64:
	ndbout << data.u_int64_value[j];
	break;
      case 128:
	ndbout << "UnSigned sz = 128 - this is something wrong??" << endl;
	break;
      default:
	// Unknown, error
	break;
      } // switch size
      break;
    case String:
      if (desc.size == 8){ 
	NdbDictionary::Column::Type type = desc.m_table->m_dictTable->getColumn(desc.attrId)->getType();
	if(type == NdbDictionary::Column::Varchar){
    	  short len = ntohs(data.u_int16_value[0]);
  	  ndbout.print("%.*s", len, (data.string_value+2));
  	} else {
    	  ndbout << data.string_value;
	}
      } // if
      else 
      {
	ndbout << "String sz != 8 - this is something wrong??" << endl;
      }
      j = desc.arraySize;
      break;
    case Float:
      // Not yet supported to print float
      ndbout << "float";
      break;
    default:
      ndbout << "Not defined Attr Type";
    } // switch AttrType
    ndbout << " ";
  } // for ArraySize
  if (desc.arraySize > 1)
  {
    ndbout << "]";
  }
  return ndbout;
}

// Print tuple data
NdbOut& 
operator<<(NdbOut& ndbout, const TupleS& tuple)
{
  ndbout << tuple.getTable()->getTableName() << "; ";
  for (int i = 0; i < tuple.getNoOfAttributes(); i++) 
  {
    const AttributeS * attr = tuple[i];
    debug << i << " " << attr->Desc->name;
    ndbout << (* attr);
    
    if (i != (tuple.getNoOfAttributes() - 1))
      ndbout << delimiter << " ";
  } // for
  return ndbout;
}

// Print tuple data
NdbOut& 
operator<<(NdbOut& ndbout, const LogEntry& logE)
{
  switch(logE.m_type)
  {
  case LogEntry::LE_INSERT:
    ndbout << "INSERT " << logE.m_table->getTableName() << " ";
    break;
  case LogEntry::LE_DELETE:
    ndbout << "DELETE " << logE.m_table->getTableName() << " ";
    break;
  case LogEntry::LE_UPDATE:
    ndbout << "UPDATE " << logE.m_table->getTableName() << " ";
    break;
  default:
    ndbout << "Unknown log entry type (not insert, delete or update)" ;
  }
  
  for (int i = 0; i < logE.m_values.size();i++) 
  {
    const AttributeS * attr = logE.m_values[i];
    ndbout << attr->Desc->name << "=";
    ndbout << (* attr);
    if (i < (logE.m_values.size() - 1))
      ndbout << ", ";
  }
  return ndbout;
}


NdbOut & 
operator<<(NdbOut& ndbout, const TableS & table){
  ndbout << endl << "Table: " << table.getTableName() << endl;
  for (int j = 0; j < table.getNoOfAttributes(); j++) 
  {
    const AttributeDesc * desc = table[j];
    ndbout << desc->name << ": ";
    NdbDictionary::Column::Type type = table.m_dictTable->getColumn(desc->attrId)->getType();
    switch(type){
    case NdbDictionary::Column::Int:
      ndbout << "Int ";
      break;
    case NdbDictionary::Column::Unsigned:
      ndbout << "Unsigned ";
      break;
    case NdbDictionary::Column::Float:
      ndbout << "Float ";
      break;
    case NdbDictionary::Column::Decimal:
      ndbout << "Decimal ";
      break;
    case NdbDictionary::Column::Char:
      ndbout << "Char ";
      break;
    case NdbDictionary::Column::Varchar:
      ndbout << "Varchar ";
      break;
    case NdbDictionary::Column::Binary:
      ndbout << "Binary ";
      break;
    case NdbDictionary::Column::Varbinary:
      ndbout << "Varbinary ";
      break;
    case NdbDictionary::Column::Bigint:
      ndbout << "Bigint ";
      break;
    case NdbDictionary::Column::Bigunsigned:
      ndbout << "Bigunsigned ";
      break;
    case NdbDictionary::Column::Double:
      ndbout << "Double ";
      break;
    case NdbDictionary::Column::Datetime:
      ndbout << "Datetime ";
      break;
    case NdbDictionary::Column::Timespec:
      ndbout << "Timespec ";
      break;
    case NdbDictionary::Column::Undefined:      
      ndbout << "Undefined ";
      break;
    default:
      ndbout << "Unknown(" << type << ")";
    }
    ndbout << " key: "  << desc->key;
    ndbout << " array: " << desc->arraySize;
    ndbout << " size: " << desc->size << endl;
  } // for
  return ndbout;
}


#if 0
/*****************************************
 *
 * Callback function for asynchronous transactions
 *
 * Idea for error handling: Transaction objects have to be stored globally when
 *                they are prepared.
 *        In the callback function if the transaction:
 *          succeeded: delete the object from global storage
 *          failed but can be retried: execute the object that is in global storage
 *          failed but fatal: delete the object from global storage
 *
 ******************************************/
static void restoreCallback(int result,            // Result for transaction
			    NdbConnection *object, // Transaction object
			    void *anything)        // Not used
{
  static Uint32 counter = 0;
  

  debug << "restoreCallback function called " << counter << " time(s)" << endl;

  ++counter;

  if (result == -1) 
  {
      ndbout << " restoreCallback (" << counter;
      if ((counter % 10) == 1) 
      {
	  ndbout << "st";
      } // if
      else if ((counter % 10) == 2) 
      {
	ndbout << "nd";
      } // else if
      else if ((counter % 10 ) ==3) 
      {
	ndbout << "rd";
      } // else if
      else 
      {
	ndbout << "th";
      } // else
      err << " time: error detected " << object->getNdbError() << endl;
    } // if
  
} // restoreCallback
#endif



bool
BackupPrinter::table(const TableS & tab)
{
  if (m_print || m_print_meta) 
  {
    m_ndbout << tab;
    ndbout_c("Successfully printed table: %s", tab.m_dictTable->getName());
  }
  return true;
}

#ifdef USE_MYSQL
bool
BackupPrinter::table(const TableS & tab, MYSQL * mysql)
{
  if (m_print || m_print_meta) 
  {
    
    char tmpTabName[MAX_TAB_NAME_SIZE*2];
    sprintf(tmpTabName, "%s", tab.getTableName());
    char * database = strtok(tmpTabName, "/");
    char * schema   = strtok( NULL , "/");
    char * tableName    = strtok( NULL , "/");
    
    /**
     * this means that the user did not specify schema
     * and it is a v2x backup
     */
    if(database == NULL)
      return false;
    if(schema == NULL)
      return false;
    if(tableName==NULL)
      tableName = schema; 
    
    char stmtCreateDB[255];
    
    sprintf(stmtCreateDB,"CREATE DATABASE %s", database);
    ndbout_c("%s", stmtCreateDB);
    
    
    char buf [2048];
    create_table_string(tab, tableName,  buf);
    ndbout_c("%s", buf);
    
    ndbout_c("Successfully printed table: %s", tab.m_dictTable->getName());
  }
  return true;
}

#endif 

void
BackupPrinter::tuple(const TupleS & tup)
{
  if (m_print || m_print_data)
    m_ndbout << tup << endl;  
}

void
BackupPrinter::logEntry(const LogEntry & logE)
{
  if (m_print || m_print_log)
    m_ndbout << logE << endl;
  m_logCount++;
}

bool
BackupRestore::init()
{

  if (!m_restore && !m_restore_meta)
    return true;

  if (ga_connect_NDB != NULL) 
  {
    // Use connection string
    Ndb::setConnectString(ga_connect_NDB);
  }

  m_ndb = new Ndb("TEST_DB");
  if (m_ndb == NULL)
    return false;
  
  m_ndb->init(1024);
  if (m_ndb->waitUntilReady(30) != 0)
  {
    ndbout << "Failed to connect to ndb!!" << endl;
    delete m_ndb;
    return false;
  }
  ndbout << "Connected to ndb!!" << endl;

#if USE_MYSQL
  if(use_mysql) 
  {
    if ( mysql_thread_safe() == 0 ) 
    {
      ndbout << "Not thread safe mysql library..." << endl;
      exit(-1);
    }
    
    ndbout << "Connecting to MySQL..." <<endl;
    
    /**
     * nwe param:
     *  port
     *  host
     *  user
     */
    bool returnValue = true;
    mysql_init(&mysql);
    {
      int portNo = 3306;
      if ( mysql_real_connect(&mysql,
			      ga_host,
			      ga_user,
			      ga_password,
			      ga_database,
			      ga_port,
			      ga_socket,
			      0) == NULL ) 
      {
	ndbout_c("Connect failed: %s", mysql_error(&mysql));
	returnValue = false;
      }
      ndbout << "Connected to MySQL!!!" <<endl;
    }

    /*  if(returnValue){
	mysql_set_server_option(&mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON);
	}
    */
    return returnValue;
  }
#endif
  return true;
  
}

BackupRestore::~BackupRestore()
{
  if (m_ndb != 0)
    delete m_ndb;
}
#ifdef USE_MYSQL
bool
BackupRestore::table(const TableS & table, MYSQL * mysqlp){
  if (!m_restore_meta) 
  {
    return true;
  }
    
  char tmpTabName[MAX_TAB_NAME_SIZE*2];
  sprintf(tmpTabName, "%s", table.getTableName());
  char * database = strtok(tmpTabName, "/");
  char * schema   = strtok( NULL , "/");
  char * tableName    = strtok( NULL , "/");

  /**
   * this means that the user did not specify schema
   * and it is a v2x backup
   */
  if(database == NULL)
    return false;
  if(schema == NULL)
    return false;
  if(tableName==NULL)
    tableName = schema; 
  
  char stmtCreateDB[255];
  sprintf(stmtCreateDB,"CREATE DATABASE %s", database);
  
  /*ignore return value. mysql_select_db will trap errors anyways*/
  if (mysql_query(mysqlp,stmtCreateDB) == 0)
  {
    //ndbout_c("%s", stmtCreateDB);
  }

  if (mysql_select_db(&mysql, database) != 0) 
  {
    ndbout_c("Error: %s", mysql_error(&mysql));
    return false;
  }
  
  char buf [2048];
  /**
   * create table ddl
   */
  if (create_table_string(table, tableName,  buf)) 
  {
    ndbout_c("Unable to create a table definition since the "
	     "backup contains undefined types");
    return false;
  }

  //ndbout_c("%s", buf);
  
  if (mysql_query(mysqlp,buf) != 0) 
  {
      ndbout_c("Error: %s", mysql_error(&mysql));
      return false;
  } else 
  {
    ndbout_c("Successfully restored table %s into database %s", tableName, database);
  }
  
  return true;
}
#endif

int
BackupConsumer::create_table_string(const TableS & table,
				    char * tableName,
				    char *buf){
  int pos = 0;
  int pos2 = 0;
  char buf2[2048];

  pos += sprintf(buf+pos, "%s%s", "CREATE TABLE ",  tableName);
  pos += sprintf(buf+pos, "%s", "(");
  pos2 += sprintf(buf2+pos2, "%s", " primary key(");

  for (int j = 0; j < table.getNoOfAttributes(); j++) 
  {
    const AttributeDesc * desc = table[j];
    //   ndbout << desc->name << ": ";
    pos += sprintf(buf+pos, "%s%s", desc->name," ");
    NdbDictionary::Column::Type type = table.m_dictTable->getColumn(desc->attrId)->getType();
    switch(type){
    case NdbDictionary::Column::Int:
      pos += sprintf(buf+pos, "%s", "int");
      break;
    case NdbDictionary::Column::Unsigned:
      pos += sprintf(buf+pos, "%s", "int unsigned");
      break;
    case NdbDictionary::Column::Float:
      pos += sprintf(buf+pos, "%s", "float");
      break;
    case NdbDictionary::Column::Decimal:
      pos += sprintf(buf+pos, "%s", "decimal");
      break;
    case NdbDictionary::Column::Char:
      pos += sprintf(buf+pos, "%s", "char");
      break;
    case NdbDictionary::Column::Varchar:
      pos += sprintf(buf+pos, "%s", "varchar");
      break;
    case NdbDictionary::Column::Binary:
      pos += sprintf(buf+pos, "%s", "binary");
      break;
    case NdbDictionary::Column::Varbinary:
      pos += sprintf(buf+pos, "%s", "varchar binary");
      break;
    case NdbDictionary::Column::Bigint:
      pos += sprintf(buf+pos, "%s", "bigint");
      break;
    case NdbDictionary::Column::Bigunsigned:
      pos += sprintf(buf+pos, "%s", "bigint unsigned");
      break;
    case NdbDictionary::Column::Double:
      pos += sprintf(buf+pos, "%s", "double");
      break;
    case NdbDictionary::Column::Datetime:
      pos += sprintf(buf+pos, "%s", "datetime");
      break;
    case NdbDictionary::Column::Timespec:
      pos += sprintf(buf+pos, "%s", "time");
      break;
    case NdbDictionary::Column::Undefined:
      //      pos += sprintf(buf+pos, "%s", "varchar binary");
      return -1;
      break;
    default:
      //pos += sprintf(buf+pos, "%s", "varchar binary");
      return -1;
    }
    if (desc->arraySize > 1) {
      int attrSize = desc->arraySize;
      pos += sprintf(buf+pos, "%s%u%s",
		     "(",
		     attrSize,
		     ")");
    }
    if (table.m_dictTable->getColumn(desc->attrId)->getPrimaryKey()) {
      pos += sprintf(buf+pos, "%s", " not null");
      pos2 += sprintf(buf2+pos2, "%s%s", desc->name, ",");
    }
    pos += sprintf(buf+pos, "%s", ",");
  } // for
  pos2--; // remove trailing comma
  pos2 += sprintf(buf2+pos2, "%s", ")");
  //  pos--; // remove trailing comma

  pos += sprintf(buf+pos, "%s", buf2);
  pos += sprintf(buf+pos, "%s", ") type=ndbcluster");
  return 0;
}



bool
BackupRestore::table(const TableS & table){
  if (!m_restore_meta) 
  {
    return true;
  }
#ifndef restore_old_types
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if (dict->createTable(*table.m_dictTable) == -1) 
  {
    err << "Create table " << table.getTableName() << " failed: "
	<< dict->getNdbError() << endl;
    return false;
  }
  info << "Successfully restored table " << table.getTableName()<< endl ;
  return true;
#else
  NdbSchemaCon * tableTransaction = 0;
  NdbSchemaOp * tableOp = 0;
  
  tableTransaction = m_ndb->startSchemaTransaction();
  if (tableTransaction == NULL) 
  {
    err << table.getTableName() 
	<< " - BackupRestore::table cannot startSchemaTransaction: "
	<< tableTransaction->getNdbError() << endl;
    return false;
  } // if
  
  tableOp = tableTransaction->getNdbSchemaOp();
  if (tableOp == NULL) 
  {
    err << table.getTableName()
	<< " - BackupRestore::table cannot getNdbSchemaOp: "
	<< tableTransaction->getNdbError() << endl;
    m_ndb->closeSchemaTransaction(tableTransaction);
    return false;
  } // if
  
  // TODO: check for errors in table attributes. set aTupleKey
  int check = 0;
  check = tableOp->createTable(table.getTableName());
  // aTableSize = 8, Not used?
  // aTupleKey = TupleKey, go through attributes and check if there is a PK
  // and so on....
  if (check == -1) 
  {
    err << table.getTableName()
	<< " - BackupRestore::table cannot createTable: "
	<< tableTransaction->getNdbError() << endl;
    m_ndb->closeSchemaTransaction(tableTransaction);
    return false;
  } // if
  
  // Create attributes from meta data
  for (int i = 0; i < table.getNoOfAttributes(); i++) 
  {
    const AttributeDesc* desc = table[i];
    check = tableOp->createAttribute(desc->name, // Attr name
				     desc->key,  // Key type
				     desc->size, // bits
				     desc->arraySize,
				     desc->type,
				     MMBased,               // only supported
				       desc->nullable
				     // Rest is don't care for the moment
				     );
    
    if (check == -1) 
    {
      err << table.getTableName()
	    << " - RestoreDataIterator::createTable cannot createAttribute: "
	  << tableTransaction->getNdbError() << endl;
      m_ndb->closeSchemaTransaction(tableTransaction);
      return false;
    } // if
  } // for 
  
  if (tableTransaction->execute() == -1) 
  {
    err << table.getTableName()
	  << " - RestoreDataIterator::createTable cannot execute transaction: "
	<< tableTransaction->getNdbError() << endl;
    m_ndb->closeSchemaTransaction(tableTransaction);
    return false;
  } // if
  
  m_ndb->closeSchemaTransaction(tableTransaction);
  info << "Successfully created table " << table.getTableName() << endl;
  return true ;
#endif
}



/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbConnection object, and 
 *   - A pointer to an arbitrary object.)
 */

static void
callback(int result, NdbConnection* trans, void* aObject)
{
  restore_callback_t * cbData = (restore_callback_t *)aObject;
  if (result<0)
  {
    /**
       * Error. temporary or permanent?
       */
    if (asynchErrorHandler(trans,  (Ndb*)cbData->ndb)) 
    {
      ((Ndb*)cbData->ndb)->closeTransaction(asynchTrans[cbData->transaction]);
      cbData->retries++;
      ((BackupRestore*)cbData)->tupleAsynch( * (TupleS*)(cbData->tup), cbData);
    }
    else
    {
      ndbout_c("Restore: Failed to restore data "
	       "due to a unrecoverable error. Exiting...");
      delete (Ndb*)cbData->ndb;
      delete cbData->tup;
      delete cbData;
      exit(-1);
    }
  } 
  else 
  {
    /**
     * OK! close transaction
     */
    ((Ndb*)cbData->ndb)->closeTransaction(asynchTrans[cbData->transaction]);
    delete cbData->tup;
    delete cbData;
  }
}

static int nPreparedTransactions = 0;
void
BackupPrinter::tupleAsynch(const TupleS & tup, restore_callback_t * callback)
{
  m_dataCount++;  
  if (m_print || m_print_data)
    m_ndbout << tup << endl;  
}

void BackupRestore::tupleAsynch(const TupleS & tup, restore_callback_t * cbData)
{

  if (!m_restore) 
  {
    delete &tup;
    return;  
  }
  Uint32 retries;
  if (cbData!=0)
    retries = cbData->retries;
  else
    retries = 0;
  
  while (retries < 10) 
  {
    /**
     * start transactions
     */
    asynchTrans[nPreparedTransactions] = m_ndb->startTransaction();
    if (asynchTrans[nPreparedTransactions] == NULL) 
    {
      if (asynchErrorHandler(asynchTrans[nPreparedTransactions], m_ndb)) 
      {
	retries++;
	continue;
      }
      asynchExitHandler();
    } // if
    
    const TableS * table = tup.getTable();
    NdbOperation * op = 
      asynchTrans[nPreparedTransactions]->getNdbOperation(table->getTableName());
    
    if (op == NULL) 
    {
      if (asynchErrorHandler(asynchTrans[nPreparedTransactions], m_ndb)) 
      {
	retries++;
	continue;
      }
	asynchExitHandler();
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (asynchErrorHandler(asynchTrans[nPreparedTransactions], m_ndb))
      {
	retries++;
	continue;
      }
      asynchExitHandler();
    } // if
    
    Uint32 ret = 0;
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      const KeyType key = attr->Desc->key;
      char * dataPtr = attr->Data.string_value;
      Uint32 length = (size * arraySize) / 8;
      if (key == TupleKey) 
      {
#if NDB_VERSION_MAJOR >= VERSION3X
	/**
	 * Convert VARCHAR from v.2x to v3x representation
	 */
	if (getMajor(tup.getTable()->getBackupVersion()) < VERSION_3X && 
	   ((tup.getTable()->m_dictTable->getColumn(i)->getType() ==
	     NdbDictionary::Column::Varbinary  ) ||
	    (tup.getTable()->m_dictTable->getColumn(i)->getType() ==
	     NdbDictionary::Column::Varchar))  && !attr->Data.null) 
	{	  
	  char * src = dataPtr;
	  char var_len[2];
	  var_len[0]= *(dataPtr+length - 2);
	  var_len[1]= *(dataPtr+length - 1);
	  memmove((char*)dataPtr+2, dataPtr, length);
	  src[0] = var_len[0];
	  src[1] = var_len[1];
	  dataPtr = src;
	}	
#endif
	ret = op->equal(i, dataPtr, length);
	if (ret<0) 
	{
	  ndbout_c("Column: %d type %d",i,
		   tup.getTable()->m_dictTable->getColumn(i)->getType());

	  if (asynchErrorHandler(asynchTrans[nPreparedTransactions],m_ndb)) 
	  {
	    retries++;
	    continue;
	  }
	  asynchExitHandler();
	}
      }
    }
    
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      KeyType key = attr->Desc->key;
      char * dataPtr = attr->Data.string_value;
      Uint32 length = (size * arraySize) / 8;
#if NDB_VERSION_MAJOR >= VERSION3X
      /**
       * Convert VARCHAR from v.2x to v3x representation
	 */
      if (getMajor(tup.getTable()->getBackupVersion()) < VERSION_3X && 
	 ((tup.getTable()->m_dictTable->getColumn(i)->getType() ==
	     NdbDictionary::Column::Varbinary  ) ||
	    (tup.getTable()->m_dictTable->getColumn(i)->getType() ==
	     NdbDictionary::Column::Varchar)) && !attr->Data.null) 
	  {
	    char * src = dataPtr;
	    char var_len[2];
	    var_len[0]= *(dataPtr+length - 2);//length is last 2 bytes
	    var_len[1]= *(dataPtr+length - 1);
	    memmove((char*)dataPtr+2, dataPtr, length);
	    src[0] = var_len[0];
	    src[1] = var_len[1];
	    dataPtr = src;
	  }	
#endif
	
      if (key == NoKey && !attr->Data.null) 
	{
	  ret = op->setValue(i, dataPtr, length);
	} 
      else if (key == NoKey && attr->Data.null) 
	{
	  ret = op->setValue(i, NULL, 0);
	}
      
      if (ret<0) 
      {
	ndbout_c("Column: %d type %d",i,
		 tup.getTable()->m_dictTable->getColumn(i)->getType());

	if (asynchErrorHandler(asynchTrans[nPreparedTransactions], m_ndb)) 
	{
	  retries++;
	  continue;
	}

	
	asynchExitHandler();
      }
    }
    restore_callback_t * cb;
    if (cbData ==0) 
    {
      cb = new restore_callback_t;
      cb->retries = 0;
    }
    else
      cb =cbData;
    cb->ndb = m_ndb;    
    cb->restore = this;
    cb->tup =  (TupleS*)&tup; 
    cb->transaction = nPreparedTransactions;

    // Prepare transaction (the transaction is NOT yet sent to NDB)
    asynchTrans[nPreparedTransactions]->executeAsynchPrepare(Commit, 
							     &callback,
							     cb);
    if (nPreparedTransactions == ga_nParallelism-1) 
    {
      // send-poll all transactions
      // close transaction is done in callback
      m_ndb->sendPollNdb(3000, ga_nParallelism);
      nPreparedTransactions=0;
    } 
    else
      nPreparedTransactions++;
    m_dataCount++;  
    return;
  }
  ndbout_c("Unable to recover from errors. Exiting...");
  asynchExitHandler();
}

void BackupRestore::asynchExitHandler() 
{
  if (m_ndb != NULL)
    delete m_ndb;
  exit(-1);
}
/**
 * returns true if is recoverable,
 * Error handling based on hugo
 *  false if it is an  error that generates an abort.
 */
static
bool asynchErrorHandler(NdbConnection * trans, Ndb* ndb) 
{
  
  NdbError error = trans->getNdbError();
  ndb->closeTransaction(trans);
  switch(error.status)
  {
  case NdbError::Success:
      return false;
      // ERROR!
      break;
      
  case NdbError::TemporaryError:
    NdbSleep_MilliSleep(10);
    return true;
    // RETRY
    break;
    
  case NdbError::UnknownResult:
    ndbout << error << endl;
    return false;
    // ERROR!
    break;
    
  default:
  case NdbError::PermanentError:
    switch (error.code)
    {
    case 499:
    case 250:
      NdbSleep_MilliSleep(10);
      return true; //temp errors?
    default:
      break;
    }
    //ERROR
    ndbout << error << endl;
    return false;
    break;
  }
  return false;
}



void
BackupRestore::tuple(const TupleS & tup)
{
  if (!m_restore)
    return;
  while (1) 
  {
    NdbConnection * trans = m_ndb->startTransaction();
    if (trans == NULL) 
    {
      // Deep shit, TODO: handle the error
      ndbout << "Cannot start transaction" << endl;
      exit(-1);
    } // if
    
    const TableS * table = tup.getTable();
    NdbOperation * op = trans->getNdbOperation(table->getTableName());
    if (op == NULL) 
    {
      ndbout << "Cannot get operation: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    } // if
    
    // TODO: check return value and handle error
    if (op->writeTuple() == -1) 
    {
      ndbout << "writeTuple call failed: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    } // if
    
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      KeyType key = attr->Desc->key;
      const char * dataPtr = attr->Data.string_value;
      
      const Uint32 length = (size * arraySize) / 8;
      if (key == TupleKey) 
      {
	  op->equal(i, dataPtr, length);
      }
    }
    
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      KeyType key = attr->Desc->key;
      const char * dataPtr = attr->Data.string_value;
      
      const Uint32 length = (size * arraySize) / 8;
      if (key == NoKey && !attr->Data.null) 
      {
	op->setValue(i, dataPtr, length);
      } 
      else if (key == NoKey && attr->Data.null) 
      {
	op->setValue(i, NULL, 0);
      }
    }
    int ret = trans->execute(Commit);
    if (ret != 0)
    {
      ndbout << "execute failed: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    }
    m_ndb->closeTransaction(trans);
    if (ret == 0)
      break;
  }
  m_dataCount++;
}

void
BackupRestore::endOfTuples()
{
  if (!m_restore)
    return;
  // Send all transactions to NDB 
  m_ndb->sendPreparedTransactions(0);  
  // Poll all transactions
  m_ndb->pollNdb(3000, nPreparedTransactions);
  // Close all transactions
  //  for (int i = 0; i < nPreparedTransactions; i++) 
  // m_ndb->closeTransaction(asynchTrans[i]);
  nPreparedTransactions=0;
}

void
BackupRestore::logEntry(const LogEntry & tup)
{
  if (!m_restore)
    return;

  NdbConnection * trans = m_ndb->startTransaction();
  if (trans == NULL) 
  {
    // Deep shit, TODO: handle the error
    ndbout << "Cannot start transaction" << endl;
    exit(-1);
  } // if
  
  const TableS * table = tup.m_table;
  NdbOperation * op = trans->getNdbOperation(table->getTableName());
  if (op == NULL) 
  {
    ndbout << "Cannot get operation: ";
    ndbout << trans->getNdbError() << endl;
    exit(-1);
  } // if
  
  int check = 0;
  switch(tup.m_type)
  {
  case LogEntry::LE_INSERT:
    check = op->insertTuple();
    break;
  case LogEntry::LE_UPDATE:
    check = op->updateTuple();
    break;
  case LogEntry::LE_DELETE:
    check = op->deleteTuple();
    break;
  default:
    ndbout << "Log entry has wrong operation type."
	   << " Exiting...";
    exit(-1);
  }
  
  for (int i = 0; i < tup.m_values.size(); i++) 
  {
    const AttributeS * attr = tup.m_values[i];
    int size = attr->Desc->size;
    int arraySize = attr->Desc->arraySize;
    KeyType key = attr->Desc->key;
    const char * dataPtr = attr->Data.string_value;
    
    const Uint32 length = (size / 8) * arraySize;
    if (key == TupleKey) 
    {
      op->equal(attr->Desc->attrId, dataPtr, length);
    } 
    else if (key == NoKey) 
    {
      op->setValue(attr->Desc->attrId, dataPtr, length);
    }
  }
  
#if 1
  trans->execute(Commit);
#else
  const int ret = trans->execute(Commit);
  // Both insert update and delete can fail during log running
  // and it's ok
  
  if (ret != 0)
  {
    ndbout << "execute failed: ";
    ndbout << trans->getNdbError() << endl;
    exit(-1);
  }
#endif
  
  m_ndb->closeTransaction(trans);
  m_logCount++;
}

void
BackupRestore::endOfLogEntrys()
{
  if (ga_restore) 
  {
    ndbout << "Restored " << m_dataCount << " tuples and "
	     << m_logCount << " log entries" << endl;
  }
}

void
BackupPrinter::endOfLogEntrys()
{
  if (m_print || m_print_log) 
  {
    ndbout << "Printed " << m_dataCount << " tuples and "
	   << m_logCount << " log entries" 
	   << " to stdout." << endl;
  }
}




