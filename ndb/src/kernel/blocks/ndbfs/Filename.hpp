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

#ifndef Filename_H
#define Filename_H

//===========================================================================
//
// .DESCRIPTION
//      Takes a 128 bits value (done as a array of four longs) and 
//      makes a filename out of it acording the following schema
//      Bits 0-31 T 
//      Bits 32-63 F
//      Bits 64-95 S
//      Bits 96-103 P
//      Bits 104-111 D
//      Bits 112-119 File Type
//      Bits 120-127 Version number of Filename
//      
//      T, is used to find/create a directory. If T = 0xFFFF then the
//      file is on top level. In that case the F is of no relevance.
//      F, same as T.
//      S, is used to find/create a filename. If S= 0xFFFF then it is ignored.
//      P, same as S
//      D, is used to find/create the root directory, this is the
//      directory before the blockname. If D= 0xFF then it is ignored.
//      File Type
//              0 => .Data
//              1 => .FragLog
//              2 => .LocLog
//              3 => .FragList
//              4 => .TableList
//              5 => .SchemaLog
//              6 => .sysfile
//              15=> ignored
//      Version number of Filename, current version is 0x1, must be
//      used for the this style of options.
//
//
//===========================================================================

#include <ndb_global.h>
#include <kernel_types.h>

class Filename
{
public:
   // filenumber is 64 bits but is split in to 4 32bits words 
   Filename();
  ~Filename();
  void set(BlockReference blockReference, 
	   const Uint32 filenumber[4], bool dir = false);
  const char* baseDirectory() const;
  const char* directory(int level);
  int levels() const;
  const char* c_str() const;

  void init(Uint32 nodeid, const char * fileSystemPath,
	    const char * backupDirPath);

private:
  int theLevelDepth;
  char theName[PATH_MAX];
  char theFileSystemDirectory[PATH_MAX];
  char theBackupDirectory[PATH_MAX];
  char *theBaseDirectory;
  char theDirectory[PATH_MAX];
};

// inline methods
inline const char* Filename::c_str() const{
  return theName;
}

inline const char* Filename::baseDirectory() const{
  return theBaseDirectory;
}

inline int Filename::levels() const{
  return theLevelDepth;
}

#endif




