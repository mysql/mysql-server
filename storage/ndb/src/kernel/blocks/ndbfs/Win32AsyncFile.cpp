/* 
   Copyright (c) 2007, 2022, Oracle and/or its affiliates.

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

#include "cstring"

#include "Win32AsyncFile.hpp"

#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>

#define JAM_FILE_ID 399


Win32AsyncFile::Win32AsyncFile(Ndbfs& fs) :
  AsyncFile(fs)
{
}

void
Win32AsyncFile::removeReq(Request * request)
{
  if(!DeleteFile(theFileName.c_str())) {
    NDBFS_SET_REQUEST_ERROR(request, GetLastError());
  }
}

void
Win32AsyncFile::rmrfReq(Request * request, const char * src, bool removePath){
  if (!request->par.rmrf.directory)
  {
    // Remove file
    if (!DeleteFile(src))
    {
      DWORD dwError = GetLastError();
      if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND)
        NDBFS_SET_REQUEST_ERROR(request, dwError);
    }
    return;
  }

  char path[PATH_MAX];
  strcpy(path, src);
  strcat(path, "\\*");

  WIN32_FIND_DATA ffd;
  HANDLE hFindFile;
loop:
  hFindFile = FindFirstFile(path, &ffd);
  if (INVALID_HANDLE_VALUE == hFindFile)
  {
    DWORD dwError = GetLastError();
    if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND)
      NDBFS_SET_REQUEST_ERROR(request, dwError);
    return;
  }
  path[strlen(path) - 1] = 0; // remove '*'

  do {
    if (0 != strcmp(".", ffd.cFileName) && 0 != strcmp("..", ffd.cFileName))
    {
      int len = (int)strlen(path);
      strcat(path, ffd.cFileName);
      if(DeleteFile(path) || RemoveDirectory(path)) 
      {
        path[len] = 0;
	continue;
      }//if

      FindClose(hFindFile);
      strcat(path, "\\*");
      goto loop;
    }
  } while(FindNextFile(hFindFile, &ffd));
  
  FindClose(hFindFile);
  path[strlen(path)-1] = 0; // remove '\'
  if (strcmp(src, path) != 0)
  {
    char * t = strrchr(path, '\\');
    t[1] = '*';
    t[2] = 0;
    goto loop;
  }

  if(removePath && !RemoveDirectory(src))
    NDBFS_SET_REQUEST_ERROR(request, GetLastError());
}

void Win32AsyncFile::createDirectories()
{
  char* tmp;
  const char * name = theFileName.c_str();
  const char * base = theFileName.get_base_name();
  while((tmp = (char *)strstr(base, DIR_SEPARATOR)))
  {
    char t = tmp[0];
    tmp[0] = 0;
    CreateDirectory(name, 0);
    tmp[0] = t;
    base = tmp + sizeof(DIR_SEPARATOR);
  }
}
