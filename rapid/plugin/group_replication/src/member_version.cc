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

#include "member_version.h"

Member_version::Member_version(unsigned int version)
{
  this->version= version;
}

uint32
Member_version::get_version() const
{
  return this->version;
}

uint32
Member_version::get_major_version() const
{
  return this->version >> 16;
}

uint32
Member_version::get_minor_version() const
{
  return (this->version >> 8) & 0xff;
}

uint32
Member_version::get_patch_version() const
{
  return this->version & 0xff;
}

bool
Member_version::operator ==(const Member_version &other) const
{
  return get_version() == other.get_version();
}

bool
Member_version::operator <(const Member_version &other) const
{
  if (*this == other)
    return false;

  if (get_major_version() < other.get_major_version())
  {
    return true;
  }
  else if (get_major_version() > other.get_major_version())
  {
    return false;
  }
  else //major version are the same
  {
    if (get_minor_version() < other.get_minor_version())
    {
      return true;
    }
    else if (get_minor_version() > other.get_minor_version())
    {
      return false;
    }
    else //minor version are the same
    {
      if (get_patch_version() < other.get_patch_version())
      {
        return true;
      }
    }
  }

  return false;
}

bool
Member_version::operator >(const Member_version &other) const
{
  if (*this == other)
    return false;

  return !(*this < other);
}

bool
Member_version::operator >=(const Member_version &other) const
{
  return (*this == other || *this > other);
}

bool
Member_version::operator <=(const Member_version &other) const
{
  return (*this == other || *this < other);
}

Member_version::~Member_version()
{
}

