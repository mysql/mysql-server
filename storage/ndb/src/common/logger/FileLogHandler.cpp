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

#include <ndb_global.h>
#include <FileLogHandler.hpp>
#include <util/File.hpp>

//
// PUBLIC
//
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
FileLogHandler::is_open()
{
  return m_pLogFile->is_open();
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
FileLogHandler::writeHeader(const char* pCategory, Logger::LoggerLevel level,
                            time_t now)
{
  char str[MAX_HEADER_LENGTH];
  m_pLogFile->writeChar(getDefaultHeader(str, pCategory, level, now));
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

off_t FileLogHandler::getCurrentSize()
{
  return m_pLogFile->size();
}

bool
FileLogHandler::createNewFile()
{
  bool rc = true;	
  int fileNo = 1;
  char newName[PATH_MAX];
  time_t newMtime, preMtime = 0;

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
    newMtime = File_class::mtime(newName);
    if (newMtime < preMtime) 
    {
      break;
    }
    else
    {
      preMtime = newMtime;
    }
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
  setErrorStr("Invalid parameter");
  return false;
}

bool FileLogHandler::getParams(BaseString &config)
{
  config.assfmt("FILE:filename=%s,maxsize=%llu,maxfiles=%u",
                m_pLogFile->getName(),
                (Uint64)m_maxFileSize,
                m_maxNoFiles);
  return true;
}

bool
FileLogHandler::setFilename(const BaseString &filename) {
  close();
  if(m_pLogFile)
    delete m_pLogFile;
  m_pLogFile = new File_class(filename.c_str(), "a+");
  return open();
}

bool
FileLogHandler::setMaxSize(const BaseString &size) {
  char *end;
  long val = strtol(size.c_str(), &end, 0); /* XXX */
  if(size.c_str() == end || val < 0)
  {
    setErrorStr("Invalid file size");
    return false;
  }
  if(end[0] == 'M')
    val *= 1024*1024;
  if(end[0] == 'k')
    val *= 1024;

  m_maxFileSize = val;

  return true;
}

bool
FileLogHandler::setMaxFiles(const BaseString &files) {
  char *end;
  long val = strtol(files.c_str(), &end, 0);
  if(files.c_str() == end || val < 1)
  {
    setErrorStr("Invalid maximum number of files");
    return false;
  }
  m_maxNoFiles = val;

  return true;
}

bool
FileLogHandler::checkParams() {
  if(m_pLogFile == NULL)
  {
    setErrorStr("Log file cannot be null.");
    return false;
  }
  return true;
}
