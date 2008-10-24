/* Copyright (C) 2008 Sun Microsystems, Inc.

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

#include "DirIterator.hpp"
#include <stdio.h>

#ifndef __WIN__

#include <dirent.h>

class DirIteratorImpl {
  DIR* m_dirp;

public:
  DirIteratorImpl():
    m_dirp(NULL) {};

  ~DirIteratorImpl() {
    closedir(m_dirp);
  }

  int open(const char* path){
    if ((m_dirp = opendir(path)) == NULL){
      return -1;
    }
    return 0;
  }

  const char* next_file(void){
    struct dirent* dp;
    while ((dp = readdir(m_dirp)) != NULL &&
           dp->d_type != DT_REG)
      ;
    return dp ? dp->d_name : NULL;
  }
};

#else

#include <BaseString.hpp>

class DirIteratorImpl {
  bool m_first;
  WIN32_FIND_DATA m_find_data;
  HANDLE m_find_handle;

  bool is_dir(const WIN32_FIND_DATA find_data) const {
    return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
  }

public:
  DirIteratorImpl():
    m_first(true),
    m_find_handle(INVALID_HANDLE_VALUE) {};

  ~DirIteratorImpl() {
    FindClose(m_find_handle);
  }

  int open(const char* path){
    BaseString path_buf;
    path_buf.assfmt("%s\\*", path);
    m_find_handle = FindFirstFile(path_buf.c_str(), &m_find_data);
    if(m_find_handle == INVALID_HANDLE_VALUE)
    {
      if (GetLastError() == ERROR_FILE_NOT_FOUND)
        m_first= false; // Will do a seek in 'next_file' and return NULL
      else
       return -1;
    }
    return 0;
  }

  const char* next_file(void){
	  while(m_first || FindNextFile(m_find_handle, &m_find_data))
    {
      m_first= false;
      
	    if (!is_dir(m_find_data))
        return m_find_data.cFileName;
    }
    return NULL;    
  }

};

#endif


DirIterator::DirIterator() :
  m_impl(*new DirIteratorImpl())
{
};

DirIterator::~DirIterator()
{
  delete &m_impl;
}


int DirIterator::open(const char* path)
{
  return m_impl.open(path);
}

const char* DirIterator::next_file(void)
{
  return m_impl.next_file();
}

