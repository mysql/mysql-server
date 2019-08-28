/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PLUGIN_SERVER_INCLUDE
#define PLUGIN_SERVER_INCLUDE

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#ifndef HAVE_REPLICATION
#define HAVE_REPLICATION
#endif

/*
  Includes only from server include folder.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <my_global.h>
#include <my_thread.h>
#include <my_sys.h>
#include <my_stacktrace.h>
#include <my_atomic.h>

#endif /* PLUGIN_SERVER_INCLUDE */
