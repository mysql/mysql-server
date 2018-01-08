/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
 
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

#include <ndberror.h>

ndberror_struct AppError29001_ReconfLock = 
  { ndberror_st_temporary , ndberror_cl_application , 29001, -1,
    "Could not obtain configuration read lock", 0
  };

ndberror_struct AppError29002_NoNDBs =
  { ndberror_st_temporary , ndberror_cl_application , 29002, -1,
    "No Ndb Instances in freelist", 0
  };

ndberror_struct AppError29023_SyncClose =
  { ndberror_st_temporary , ndberror_cl_application , 29023, -1,
    "Waited for synchronous close of NDB transaction", 0 
  };

ndberror_struct AppError29024_autogrow = 
  { ndberror_st_temporary , ndberror_cl_application , 29024, -1,
    "Out of Ndb instances, growing freelist", 0 
  };
