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

#ifndef KEY_DESCRIPTOR_HPP
#define KEY_DESCRIPTOR_HPP

#include <ndb_types.h>
#include <ndb_limits.h>
#include "CArray.hpp"

struct KeyDescriptor
{
  KeyDescriptor () { 
    noOfKeyAttr = hasCharAttr = noOfDistrKeys = noOfVarKeys = 0; 
  }
  
  Uint8 noOfKeyAttr;
  Uint8 hasCharAttr;
  Uint8 noOfDistrKeys;
  Uint8 noOfVarKeys;
  struct KeyAttr 
  {
    Uint32 attributeDescriptor;
    CHARSET_INFO* charsetInfo;
  } keyAttr[MAX_ATTRIBUTES_IN_INDEX];
};

extern CArray<KeyDescriptor> g_key_descriptor_pool;

#endif
