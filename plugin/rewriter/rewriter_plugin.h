#ifndef REWRITER_PLUGIN_INCLUDED
#define REWRITER_PLUGIN_INCLUDED
/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file rewriter_plugin.h

*/

#include "my_config.h"
#include <my_global.h>
#include <mysql/plugin_audit.h>

bool refresh_rules_table();

MYSQL_PLUGIN get_rewriter_plugin_info();

#endif // REWRITER_PLUGIN_INCLUDED
