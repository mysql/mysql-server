/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MEMBER_VERSION_INCLUDED
#define	MEMBER_VERSION_INCLUDED

#include "my_global.h"

class Member_version
{
public:
  Member_version(unsigned int version);

  /**
    @return returns the member version.
  */
  uint32 get_version() const;

  /**
    @return returns the major version (Major.v.v)
  */
  uint32 get_major_version() const;
  /**
    @return returns the minor version (v.Minor.v)
  */
  uint32 get_minor_version() const;
  /**
    @return returns the minor version (v.v.Patch)
  */
  uint32 get_patch_version() const;

  bool operator== (const Member_version &other) const;
  bool operator<  (const Member_version &other) const;
  bool operator>  (const Member_version &other) const;
  bool operator>= (const Member_version &other) const;
  bool operator<= (const Member_version &other) const;


  virtual ~Member_version();
private:
  uint32 version;
};

#endif	/* MEMBER_VERSION_INCLUDED */

