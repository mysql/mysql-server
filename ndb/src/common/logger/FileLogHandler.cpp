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
#include <FileLogHandler.hpp>
#include <File.hpp>

//
// PUBLIC
//

FileLogHandler::FileLogHandler() : 
  LogHandler(),
  m_maxNoFiles(MAX_NO_FILES), 
  m_maxFileSize(MAX_FILE_SIZE),
  m_maxLogEntries(MAX_LOG_ENTRIES)

{
  m_pLogFile = new File_class("logger.log", "a+");
}

FileLogHandler::FileLogHandler(const char* aFileName, 
			       int maxNoFiles, 
			       long maxFileSize,
			       unsigned int maxLogEntries) : 
  LogHandler(),
  m_maxNoFiles(maxNoFiles), 
  m_maxFileSize(maxFileSize),
  m_maxLogEntries(maxLogEntries)
{
  m_pLogFile = new File_class(aFileName, "a+");
}

FileLogHandler::~FileLogHandler()
{
  delete m_pLogFile;
}

bool
FileLogHandler::open()
{
  bool rc = true;

  if (m_pLogFile->open())
  {
    if (isTimeForNewFile())
    {
      if (!createNewFile())
      {
	setErrorCode(errno);
	rc = false; 
      }
    }
  }
  else
  {
    setErrorCode(errno);
    rc = false;
  }

  return rc;
}

bool
FileLogHandler::close()
{
  bool rc = true;
  if (!m_pLogFile->close())
  {
    setErrorCode(errno);
    rc = false;
  }	

  return rc;
}

void 
FileLogHandler::writeHeader(const char* pCategory, Logger::LoggerLevel level)
{
  char str[LogHandler::MAX_HEADER_LENGTH];
  m_pLogFile->writeChar(getDefaultHeader(str, pCategory, level));
}

void 
FileLogHandler::writeMessage(const char* pMsg)
{
  m_pLogFile->writeChar(pMsg);
}

void 
FileLogHandler::writeFooter()
{
  static int callCount = 0;
  m_pLogFile->writeChar(getDefaultFooter());
  /**
   * The reason I also check the number of log entries instead of
   * only the log size, is that I do not want to check the file size
   * after each log entry which requires system calls and is quite slow.
   * TODO: Any better way?
   */
  if (callCount % m_maxLogEntries != 0) // Check every m_maxLogEntries
  {
    if (isTimeForNewFile())
    {
      if (!createNewFile())
      {
	// Baby one more time...
	createNewFile();
      }
    }	
    callCount = 0;
  }
  callCount++;

  // Needed on Cello since writes to the flash disk does not happen until 
  // we flush and fsync.
  m_pLogFile->flush();
}


//
// PRIVATE
//

bool 
FileLogHandler::isTimeForNewFile()
{
  return (m_pLogFile->size() >= m_maxFileSize); 
}

bool
FileLogHandler::createNewFile()
{
  bool rc = true;	
  int fileNo = 1;
  char newName[PATH_MAX];

  do
  {
    if (fileNo >= m_maxNoFiles)
    {
      fileNo = 1;
      BaseString::snprintf(newName, sizeof(newName),
		 "%s.%d", m_pLogFile->getName(), fileNo);
      break;
    }		
    BaseString::snprintf(newName, sizeof(newName),
	       "%s.%d", m_pLogFile->getName(), fileNo++); 
    
  } while (File_class::exists(newName));
  
  m_pLogFile->close();	
  if (!File_class::rename(m_pLogFile->getName(), newName))
  {		
    setErrorCode(errno);
    rc = false;
  }

  // Open again
  if (!m_pLogFile->open())
  {
    setErrorCode(errno);
    rc = false;
  }				
  
  return rc;
}

bool
FileLogHandler::setParam(const BaseString &param, const BaseString &value){
  if(param == "filename")
    return setFilename(value);
  if(param == "maxsize")
    return setMaxSize(value);
  if(param == "maxfiles")
    return setMaxFiles(value);
  return false;
}

bool
FileLogHandler::setFilename(const BaseString &filename) {
  close();
  if(m_pLogFile)
    delete m_pLogFile;
  m_pLogFile = new File_class(filename.c_str(), "a+");
  open();
  return true;
};

bool
FileLogHandler::setMaxSize(const BaseString &size) {
  char *end;
  long val = strtol(size.c_str(), &end, 0); /* XXX */
  if(size.c_str() == end)
    return false;
  if(strncasecmp("M", end, 1) == 0)
    val *= 1024*1024;
  if(strncasecmp("k", end, 1) == 0)
    val *= 1024;

  m_maxFileSize = val;

  return true;
};

bool
FileLogHandler::setMaxFiles(const BaseString &files) {
  char *end;
  long val = strtol(files.c_str(), &end, 0);
  if(files.c_str() == end)
    return false;
  m_maxNoFiles = val;

  return true;
};

bool
FileLogHandler::checkParams() {
  if(m_pLogFile == NULL)
    return false;
  return true;
}
