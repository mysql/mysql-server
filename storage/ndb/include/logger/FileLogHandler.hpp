/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

  virtual const char* handler_type() {return "FILE"; };

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
		 int maxNoFiles = 6,
		 long maxFileSize = 1024000,
		 unsigned int maxLogEntries = 10000);

  /**
   * Destructor.
   */
  virtual ~FileLogHandler();
  
  virtual bool open();
  virtual bool close();

  virtual bool is_open();

  virtual bool setParam(const BaseString &param, const BaseString &value);
  virtual bool checkParams();

  virtual bool getParams(BaseString &config);

  virtual off_t getCurrentSize();
  virtual off_t getMaxSize() { return m_maxFileSize; };

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
  off_t m_maxFileSize;
  unsigned int m_maxLogEntries;
  File_class* m_pLogFile;
};

#endif
