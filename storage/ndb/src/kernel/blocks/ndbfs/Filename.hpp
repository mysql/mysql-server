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
#include <SimulatedBlock.hpp>

class Filename
{
public:
   // filenumber is 64 bits but is split in to 4 32bits words 
  Filename();
  ~Filename();

  struct NameSpec {
    NameSpec(BaseString& f, BaseString&b) :
      fs_path(f), backup_path(b) {}
    BaseString& fs_path;
    BaseString& backup_path;
  };
  
  void set(NameSpec& spec, 
	   BlockReference, const Uint32 fileno[4], bool = false);
  void set(NameSpec& spec, 
	   SegmentedSectionPtr ptr, class SectionSegmentPool&);
  
  const char* c_str() const;     // Complete name including dirname
  const char* get_base_name() const; // Exclude fs (or backup) path
private:
  char theName[PATH_MAX];
  char * m_base_name;
};

// inline methods
inline const char* Filename::c_str() const {
  return theName;
}

inline const char* Filename::get_base_name() const {
  return m_base_name;
}

#endif




