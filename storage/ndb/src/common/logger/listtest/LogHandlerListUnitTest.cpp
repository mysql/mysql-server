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

#include "LogHandlerListUnitTest.hpp"

#include <ConsoleLogHandler.hpp>
#include <FileLogHandler.hpp>
#include <SysLogHandler.hpp>

#include <NdbOut.hpp>

typedef bool (*TESTFUNC)(const char*);
typedef struct
{
  const char* name;
  TESTFUNC test;
}Tests;

static Tests testCases[] = { {"Add", &LogHandlerListUnitTest::testAdd},
			     {"Remove", &LogHandlerListUnitTest::testRemove},
			     {"Traverse Next", &LogHandlerListUnitTest::testTraverseNext}
                           };


int testFailed = 0;

int main(int argc, char* argv[])
{	
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
  
  return 0;  	
}

bool 
LogHandlerListUnitTest::testAdd(const char* msg)
{
  bool rc = true;
  LogHandlerList list;
  int size = 10;
  for (int i = 0; i < size; i++)
  {
    list.add(new ConsoleLogHandler());
  }
  if (list.size() != size)
  {
    rc = false;
  }
  ndbout << "List size: " << list.size() << endl;


  return rc;
}
bool 
LogHandlerListUnitTest::testRemove(const char* msg)
{
  bool rc = true;

  LogHandlerList list;
  int size = 10;
  LogHandler* pHandlers[10];
  for (int i = 0; i < size; i++)
  {
    pHandlers[i] = new ConsoleLogHandler();
    list.add(pHandlers[i]);
  }
  
  // Remove

  for (int i = 0; i < size; i++)
  {
    if (!list.remove(pHandlers[i]))
    {
      ndbout << "Could not remove handler!" << endl;
    }
    else
    {
      ndbout << "List size: " << list.size() << endl;
    }
  }

  return rc;

}
bool 
LogHandlerListUnitTest::testTraverseNext(const char* msg)
{
  bool rc = true;
  LogHandlerList list;
  int size = 10;
  LogHandler* pHandlers[10];

  for (int i = 0; i < size; i++)
  {
    char* str = new char[3];
    pHandlers[i] = new ConsoleLogHandler();
    BaseString::snprintf(str, 3, "%d", i);
    pHandlers[i]->setDateTimeFormat(str);   
    list.add(pHandlers[i]);
  }
  
  ndbout << "List size: " << list.size() << endl;
      
  LogHandler* pHandler = NULL;
  int i = 0;
  while ((pHandler = list.next()) != NULL)
  {
    ndbout << "Handler[" << i++ << "]:dateformat = " 
	 << pHandler->getDateTimeFormat() << endl;
  }

  list.removeAll();

  return rc;

}

void
LogHandlerListUnitTest::error(const char* msg)
{
  testFailed++;
  ndbout << "Test failed: " << msg << endl;  
}

LogHandlerListUnitTest::LogHandlerListUnitTest()
{
}
LogHandlerListUnitTest::~LogHandlerListUnitTest()
{
}
