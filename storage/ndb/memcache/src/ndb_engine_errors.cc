/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA 
*/

#include <ndberror.h>

ndberror_struct AppError9001_ReconfLock = 
  { ndberror_st_temporary , ndberror_cl_application , 9001, -1,
    "Could not obtain configuration read lock" 
  };

ndberror_struct AppError9002_NoNDBs =
  { ndberror_st_temporary , ndberror_cl_application , 9002, -1,
    "No Ndb Instances in freelist" 
  };

