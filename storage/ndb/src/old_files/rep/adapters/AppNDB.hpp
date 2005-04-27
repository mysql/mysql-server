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

#ifndef APPNDB_HPP
#define APPNDB_HPP
#include "NdbApi.hpp"

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>

#include <NdbThread.h>
#include <Vector.hpp>

#include "TableInfoPs.hpp"
#include <rep/storage/GCIContainer.hpp>
#include <rep/storage/GCIBuffer.hpp>

#include <rep/state/RepState.hpp>

extern "C" {
  void * runAppNDB_C(void *);
}

/**
 * @class AppNDB
 * @brief Connects to NDB and appliers log records into standby system
 */
class AppNDB {
public:
  /***************************************************************************
   * Constructor / Destructor / Init
   ***************************************************************************/
  AppNDB(class GCIContainer * gciContainer, class RepState * repState);
  ~AppNDB();

  void init(const char * connectString);

  GrepError::Code 
  applyBuffer(Uint32 nodeGrp, Uint32 first, Uint32 force);
  
  /**
   * Takes a table id and drops it.
   * @param tableId   Name of table to be dropped
   * @return Returns 1 = ok, -1 failed
   * 
   * @todo Fix: 0 usually means ok...
   */
  int dropTable(Uint32 tableId);
  void startApplier();
  void stopApplier(GrepError::Code err);  
private:
  /***************************************************************************
   * Methods
   ***************************************************************************/
  friend void* runAppNDB_C(void*);

  void threadMainAppNDB(void);

  /**
   * Takes a log records and does the operation specified in the log record
   * on NDB.
   * @param - lr (LogRecord)
   * @param - force true if GREP:SSCoord is in phase STARTING. 
   *	      Ignore "Execute" errors if true.
   */
  int applyLogRecord(LogRecord *  lr, bool force, Uint32 gci);

  /**
   * Applies a table based on a meta record and creates the table 
   * in NDB.
   * @param - meta record
   * @return - 0 on success, -1 if something went wrong
   */
  int applyMetaRecord(MetaRecord *  mr, Uint32 gci);

  /**
   * Takes a meta record and uses NdbDictionaryXXX::parseInfoTable 
   * and returns a table
   * @param mr - MetaRecord
   * @return - a table based on the meta record
   */
  NdbDictionary::Table* prepareMetaRecord(MetaRecord *  mr);

  /**
   * Prints out an NDB error message if a ndb operation went wrong.
   * @param msg - text explaining the error
   * @param err - NDB error type
   */
  void reportNdbError(const char * msg, const NdbError & err);
  
  /**
   * Prints out a warning message. Used if support for something
   * is not implemented.
   * @param tableName - the name of the table this warning occured on
   * @param message - warning message
   */
  void reportWarning(const char * tableName, const char * message);

  /**
   * Prints out a warning message. Used if support for something
   * is not implemented.
   * @param tableName - the name of the table this warning occured on
   * @param columnName - the name of the column this warning occured on
   * @param message - warning message
   */
  void reportWarning(const char * tableName, const char * columnName,
		     const char * message);


  /***************************************************************************
   * Variables
   ***************************************************************************/
  GCIContainer *               m_gciContainer;
  RepState *	               m_repState;

  Ndb*                         m_ndb;
  NdbDictionary::Dictionary *  m_dict;
  NodeId                       m_ownNodeId;
  bool                         m_started;
  TableInfoPs *                m_tableInfoPs;
  NdbThread*                   m_applierThread; 
  NdbCondition *	       m_cond;
  MutexVector<GCIBuffer*>      m_gciBufferList;
};

#endif
