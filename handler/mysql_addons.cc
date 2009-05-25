/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file handler/mysql_addons.cc
This file contains functions that need to be added to
MySQL code but have not been added yet.

Whenever you add a function here submit a MySQL bug
report (feature request) with the implementation. Then
write the bug number in the comment before the
function in this file.

When MySQL commits the function it can be deleted from
here. In a perfect world this file exists but is empty.

Created November 07, 2007 Vasil Dimov
*******************************************************/

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif /* MYSQL_SERVER */

#include <mysql_priv.h>

#include "mysql_addons.h"
#include "univ.i"
