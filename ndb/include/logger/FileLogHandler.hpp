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

#ifndef FILELOGHANDLER_H
#define FILELOGHANDLER_H

#include "LogHandler.hpp"

class File_class;

/**
 * Logs messages to a file. The log file will be archived depending on
 * the file's size or after N number of log entries. 
 * There will be only a specified number of archived logs 
 * which will be "recycled".
 *
 * The archived log file will be named as <filename>.1..N.
 * 
 *
 * @see LogHandler
 * @version #@ $Id: FileLogHandler.hpp,v 1.2 2003/09/01 10:15:53 innpeno Exp $
 */
class FileLogHandler : public LogHandler
{
public:
  /** Max number of log files to archive. */
  STATIC_CONST( MAX_NO_FILES = 6 );	
  /** Max file size of the log before archiving.  */
  STATIC_CONST( MAX_FILE_SIZE = 1024000 );
  /** Max number of log entries before archiving. */
  STATIC_CONST( MAX_LOG_ENTRIES = 10000 );

  /**
   * Default constructor.
   */
  FileLogHandler();

  /**
   * Creates a new file handler with the specified filename, 
   * max number of archived log files and max log size for each log.
   *
   * @param aFileName the log filename.
   * @param maxNoFiles the maximum number of archived log files.
   * @param maxFileSize the maximum log file size before archiving.
   * @param maxLogEntries the maximum number of log entries before checking time to archive.
   */
  FileLogHandler(const char* aFileName, 
		 int maxNoFiles = MAX_NO_FILES, 
		 long maxFileSize = MAX_FILE_SIZE,
		 unsigned int maxLogEntries = MAX_LOG_ENTRIES);

  /**
   * Destructor.
   */
  virtual ~FileLogHandler();
  
  virtual bool open();
  virtual bool close();

  virtual bool setParam(const BaseString &param, const BaseString &value);
  virtual bool checkParams();
  
protected:	
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level);
  virtual void writeMessage(const char* pMsg);
  virtual void writeFooter();
  
private:
  /** Prohibit */
  FileLogHandler(const FileLogHandler&);
  FileLogHandler operator = (const FileLogHandler&);
  bool operator == (const FileLogHandler&);

  /**
   * Returns true if it is time to create a new log file.
   */
  bool isTimeForNewFile();

  /**
   * Archives the current log file and creates a new one.
   * The archived log filename will be in the format of <filename>.N
   *
   * @return true if successful.
   */
  bool createNewFile();

  bool setFilename(const BaseString &filename);
  bool setMaxSize(const BaseString &size);
  bool setMaxFiles(const BaseString &files);
  
  int m_maxNoFiles;
  long m_maxFileSize;
  unsigned int m_maxLogEntries;
  File_class* m_pLogFile;
};

#endif
