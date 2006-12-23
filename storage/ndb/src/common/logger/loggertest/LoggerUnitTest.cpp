/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "LoggerUnitTest.hpp"

#include <Logger.hpp>
#include <ConsoleLogHandler.hpp>
#include <FileLogHandler.hpp>

#include <SysLogHandler.hpp>

#include <NdbOut.hpp>
#include <NdbMain.h>

typedef bool (*TESTFUNC)(const char*);
typedef struct
{
  const char* name;
  TESTFUNC test;
}Tests;

static Tests testCases[] = { {"Alert", &LoggerUnitTest::testAlert},
			     {"Critical", &LoggerUnitTest::testCritical},
			     {"Error", &LoggerUnitTest::testError},
			     {"Warning", &LoggerUnitTest::testWarning},
			     {"Info", &LoggerUnitTest::testInfo},
			     {"Debug", &LoggerUnitTest::testDebug},
			     {"Info to Critical", &LoggerUnitTest::testInfoCritical},
			     {"All", &LoggerUnitTest::testAll},
			     {"Off", &LoggerUnitTest::testOff}
                           };

static Logger logger;
int testFailed = 0;

NDB_COMMAND(loggertest, "loggertest", "loggertest -console | -file", 
	    "loggertest", 16384)
{	
  if (argc < 2)
  {
    ndbout << "Usage: loggertest -console | -file | -syslog" << endl;
    return 0;
  }

  if (strcmp(argv[1], "-console") == 0)
  {
    logger.createConsoleHandler();
  }
  else if (strcmp(argv[1], "-file") == 0)
  {
    logger.createFileHandler();
    //logger.addHandler(new FileLogHandler(argv[2]));
  }
  else if (strcmp(argv[1], "-syslog") == 0)
  {
    logger.createSyslogHandler();
  }

  logger.disable(Logger::LL_ALL);

  char str[256];
  int testCount = (sizeof(testCases) / sizeof(Tests)); 
  ndbout << "Starting " << testCount << " tests..." << endl;
  for (int i = 0; i < testCount; i++)
  {
    ndbout << "-- " << " Test " << i + 1 
         << " [" << testCases[i].name << "] --" << endl;
    BaseString::snprintf(str, 256, "%s %s %s %d", "Logging ", 
	       testCases[i].name, " message ", i);  
    if (testCases[i].test(str))
    {
      ndbout << "-- Passed --" << endl;
    }    
    else
    {
      ndbout << "-- Failed -- " << endl;
    }
    
  }
  ndbout << endl << "-- " << testCount - testFailed << " passed, " 
       << testFailed << " failed --" << endl;

  logger.removeAllHandlers();

  return 0;  	
}

bool
LoggerUnitTest::logTo(Logger::LoggerLevel from, Logger::LoggerLevel to, const char* msg)
{
  logger.enable(from, to);
  return logTo(from, msg);
}

bool
LoggerUnitTest::logTo(Logger::LoggerLevel level, const char* msg)
{	
  logger.enable(level);
  logger.alert(msg);
  logger.critical(msg);
  logger.error(msg);
  logger.warning(msg);
  logger.info(msg);
  logger.debug(msg);	
  logger.disable(level);
  return true;
}

bool
LoggerUnitTest::testAll(const char* msg)
{	
  return logTo(Logger::LL_ALL, msg);
}

bool
LoggerUnitTest::testOff(const char* msg)
{
  return logTo(Logger::LL_OFF, msg);

}

bool
LoggerUnitTest::testAlert(const char* msg)
{
  return logTo(Logger::LL_ALERT, msg);
}

bool
LoggerUnitTest::testCritical(const char* msg)
{
  return logTo(Logger::LL_CRITICAL, msg);
}

bool
LoggerUnitTest::testError(const char* msg)
{
  return logTo(Logger::LL_ERROR, msg);
}

bool
LoggerUnitTest::testWarning(const char* msg)
{
  return logTo(Logger::LL_WARNING, msg);
}

bool
LoggerUnitTest::testInfo(const char* msg)
{
  return logTo(Logger::LL_INFO, msg);
}

bool
LoggerUnitTest::testDebug(const char* msg)
{
  return logTo(Logger::LL_DEBUG, msg);
}

bool 
LoggerUnitTest::testInfoCritical(const char* msg)
{
  return logTo(Logger::LL_CRITICAL, Logger::LL_INFO, msg);
}

void
LoggerUnitTest::error(const char* msg)
{
  testFailed++;
  ndbout << "Test failed: " << msg << endl;  
}

LoggerUnitTest::LoggerUnitTest()
{
}
LoggerUnitTest::~LoggerUnitTest()
{
}
