/* Copyright (C) 2002 MySQL AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  RAID support for MySQL. For full comments, check raid.cc
  This is in a separate file to not cause problems on OS that can't
  put C++ files in archives.
*/

#include "mysys_priv.h"

const char *raid_type_string[]={"none","striped"};

const char *my_raid_type(int raid_type)
{
  return raid_type_string[raid_type];
}
