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

#include <ndb_global.h>

#include <File.hpp>

#include <NdbOut.hpp>
#include <my_dir.h>

//
// PUBLIC
//
time_t 
File_class::mtime(const char* aFileName)
{
  MY_STAT stmp;
  time_t rc = 0;

  if (my_stat(aFileName, &stmp, MYF(0)) != NULL) {
    rc = stmp.st_mtime;
  }

  return rc;
}

bool 
File_class::exists(const char* aFileName)
{
  MY_STAT stmp;

  return (my_stat(aFileName, &stmp, MYF(0))!=NULL);
}

off_t
File_class::size(FILE* f)
{
  MY_STAT s;

  // Note that my_fstat behaves *differently* than my_stat. ARGGGHH!
  if (my_fstat(fileno(f), &s, MYF(0)))
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
  m_file(NULL), 
  m_fileMode("r")
{
}

File_class::File_class(const char* aFileName, const char* mode) :	
  m_file(NULL), 
  m_fileMode(mode)
{
  BaseString::snprintf(m_fileName, PATH_MAX, aFileName);
}

bool
File_class::open()
{
  return open(m_fileName, m_fileMode);
}

bool 
File_class::open(const char* aFileName, const char* mode) 
{
  if(m_fileName != aFileName){
    /**
     * Only copy if it's not the same string
     */
    BaseString::snprintf(m_fileName, PATH_MAX, aFileName);
  }
  m_fileMode = mode;
  bool rc = true;
  if ((m_file = ::fopen(m_fileName, m_fileMode))== NULL)
  {
    rc = false;      
  }
  
  return rc;
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

  if (m_file != NULL)
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
      ndbout_c("ERROR: Close file error in File.cpp for %s",strerror(errno));
    }   
  }  
  m_file = NULL;

  return rc;
}

int 
File_class::read(void* buf, size_t itemSize, size_t nitems) const
{
  return ::fread(buf, itemSize,  nitems, m_file);
}

int 
File_class::readChar(char* buf, long start, long length) const
{
  return ::fread((void*)&buf[start], 1, length, m_file);
}

int 
File_class::readChar(char* buf)
{
  return readChar(buf, 0, strlen(buf));
}

int 
File_class::write(const void* buf, size_t size_arg, size_t nitems)
{
  return ::fwrite(buf, size_arg, nitems, m_file);
}
 
int
File_class::writeChar(const char* buf, long start, long length)
{
  return ::fwrite((const void*)&buf[start], sizeof(char), length, m_file);
}

int 
File_class::writeChar(const char* buf)
{
  return writeChar(buf, 0, ::strlen(buf));
}

off_t
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
