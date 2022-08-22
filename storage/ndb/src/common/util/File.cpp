/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include <time.h>

#include <util/File.hpp>
#include <NdbOut.hpp>
#include <EventLogger.hpp>

//
// PUBLIC
//
time_t 
File_class::mtime(const char* aFileName)
{
  struct stat s;
  if (stat(aFileName, &s) != 0)
    return 0;
  return s.st_mtime;
}

bool 
File_class::exists(const char* aFileName)
{
  struct stat s;
  if (stat(aFileName, &s) != 0)
    return false;
  return true;
}

ndb_off_t
File_class::size(FILE* f)
{
  struct stat s;
  if (fstat(fileno(f), &s) != 0)
    return 0;
  return s.st_size;
}

bool 
File_class::rename(const char* currFileName, const char* newFileName)
{
  return ::rename(currFileName, newFileName) == 0 ? true : false;
}
bool 
File_class::remove(const char* aFileName)
{
  return ::remove(aFileName) == 0 ? true : false;
}

File_class::File_class() : 
  m_file(nullptr), 
  m_fileMode("r")
{
}

File_class::File_class(const char* aFileName, const char* mode) :	
  m_file(nullptr), 
  m_fileMode(mode)
{
  BaseString::snprintf(m_fileName, PATH_MAX, "%s", aFileName);
}

bool
File_class::open()
{
  return open(m_fileName, m_fileMode);
}

bool 
File_class::open(const char* aFileName, const char* mode) 
{
  assert(m_file == nullptr); // Not already open
  if(m_fileName != aFileName){
    /**
     * Only copy if it's not the same string
     */
    BaseString::snprintf(m_fileName, PATH_MAX, "%s", aFileName);
  }
  m_fileMode = mode;
  bool rc = true;
  if ((m_file = ::fopen(m_fileName, m_fileMode))== nullptr)
  {
    rc = false;      
  }
  
  return rc;
}

bool
File_class::is_open()
{
  return (m_file != nullptr);
}

File_class::~File_class()
{
  close();  
}

bool 
File_class::remove()
{
  // Close the file first!
  close();
  return File_class::remove(m_fileName);
}

bool 
File_class::close()
{
  bool rc = true;
  int retval = 0;

  if (m_file != nullptr)
  { 
    ::fflush(m_file);
    retval = ::fclose(m_file);
    while ( (retval != 0) && (errno == EINTR) ){
      retval = ::fclose(m_file);
    }
    if( retval == 0){
      rc = true;
    }
    else {
      rc = false;
      g_eventLogger->info("ERROR: Close file error in File.cpp for %s",
                          strerror(errno));
    }   
  }  
  m_file = nullptr;

  return rc;
}

int 
File_class::read(void* buf, size_t itemSize, size_t nitems) const
{
  return (int)::fread(buf, itemSize,  nitems, m_file);
}

int 
File_class::readChar(char* buf, long start, long length) const
{
  return (int)::fread((void*)&buf[start], 1, length, m_file);
}

int 
File_class::readChar(char* buf)
{
  return readChar(buf, 0, (long)strlen(buf));
}

int 
File_class::write(const void* buf, size_t size_arg, size_t nitems)
{
  return (int)::fwrite(buf, size_arg, nitems, m_file);
}
 
int
File_class::writeChar(const char* buf, long start, long length)
{
  return (int)::fwrite((const void*)&buf[start], sizeof(char), length, m_file);
}

int 
File_class::writeChar(const char* buf)
{
  return writeChar(buf, 0, (long)::strlen(buf));
}

ndb_off_t
File_class::size() const
{
  return File_class::size(m_file);
}

const char* 
File_class::getName() const
{
  return m_fileName;
}

int
File_class::flush() const
{
  return ::fflush(m_file);;
}
