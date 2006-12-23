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

#include "FileUnitTest.hpp"
#include <File.hpp>

#include <NdbOut.hpp>

typedef bool (*TESTFUNC)(const char*);

typedef const char TESTNAME;
typedef struct 
{
  const char* name;
  TESTFUNC test;
}Tests;

static Tests testCases[] = { {"Create/Write", &FileUnitTest::testWrite}, 
                            {"Read", &FileUnitTest::testRead},
                            {"Exists", &FileUnitTest::testExists},
                            {"File Size", &FileUnitTest::testSize}, 
                            {"Rename", &FileUnitTest::testRename},
                            {"Remove", &FileUnitTest::testRemove} };

static int testFailed = 0;
                          
int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    ndbout << "Usage: filetest <filename>" << endl;
    return 0;
  }
  const char* fileName = argv[1];

  int testCount = (sizeof(testCases) / sizeof(Tests)); 
  ndbout << "Starting " << testCount << " tests..." << endl;
  for (int i = 0; i < testCount; i++)
  {
    ndbout << "-- " << " Test " << i + 1 
         << " [" << testCases[i].name << "] --" << endl;
    if (testCases[i].test(fileName))
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
FileUnitTest::testWrite(const char* aFileName)
{
  bool rc = true;
  File f;
  if (f.open(aFileName, "w"))
  {  
   	f.writeChar("ABABABABABAB ABBABAB ABBABA ABAB JKH KJHA JHHAHAH...");
   	f.writeChar("12129791242 1298371923 912738912 378129837128371128132...\n");    
    f.close();    
  }
  else
  {
    error("testWrite failed: ");
    rc = false;
  }
  return rc;
}

bool 
FileUnitTest::testRead(const char* aFileName)
{
  bool rc = true;
  // Read file
  File f;
  if (f.open(aFileName, "r"))
  {
    long size = f.size();
    ndbout << "File size = " << size << endl;
    ndbout << "Allocating buf of " << size << " bytes" << endl;
    char* buf = new char[size];
    buf[size - 1] = '\0';
    int r = 0;
    while ((r = f.readChar(buf, r, size)) > 0)
    {       
      ndbout << "Read(" << r << "):" << buf << endl;
    }   
    f.close(); 
    delete buf;
  }
  else
  {
    error("readTest failed: ");
    rc = false;
  }  
  return rc;
}

bool 
FileUnitTest::testExists(const char* aFileName)
{
  bool rc = true;
  if (File::exists(aFileName))
  {
    if (File::exists("ThisFileShouldnotbe.txt"))
    {
      rc = false;
      error("testExists failed, the file should NOT be found.");
    }
  }
  else
  {
    rc = false;
    error("testExists failed, the file should exist.");    
  }
  
  return rc;
}


bool 
FileUnitTest::testSize(const char* aFileName)
{
  bool rc = true;
  File f;
  if (f.open(aFileName, "r"))
  {
    long size = f.size();
    if (size <= 0)
    {
      rc = false;
      error("testSize failed, size is <= 0");
    }
    ndbout << "File size = " << size << endl;
  }
  else
  {
    rc = false;
    error("testSize failed, could no open file.");
  }
  f.close();
  return rc;
}

bool 
FileUnitTest::testRename(const char* aFileName)
{
  bool rc = true;
  if (File::rename(aFileName, "filetest_new.txt"))
  {
    if (!File::exists("filetest_new.txt"))
    {
      rc = false;
      error("testRename failed, new file does not exists.");
    }
    else
    {
      ndbout << "Renamed " << aFileName << " to filetest_new.txt" << endl;
    }        
  }
  else
  {
    rc = false;
    error("testRename failed, unable to rename file.");
  }
  
  return rc;
}

bool 
FileUnitTest::testRemove(const char* aFileName)
{
  bool rc = true;
  File f;
  if (f.open("filetest_new.txt", "r"))
  {
    if (!f.remove())
    {
      rc = false;
      error("testRemove failed, could not remove file.");
    }
    else
    {
      if (File::exists("filetest_new"))
      {
        rc = false;
        error("testRemove failed, file was not removed, it still exists.");
      }      
    }         
  } // (f.open("filetest_new", "r"))
  else
  {
    rc = false;
    error("testRemove failed, could not read the file.");
  }
  
 return rc;   
}

void  
FileUnitTest::error(const char* msg)
{
  testFailed++;
  ndbout << "Test failed: " << msg << endl;  
  perror("Errno msg");
}


FileUnitTest::FileUnitTest()
{
  
}

FileUnitTest::~FileUnitTest()
{

}
