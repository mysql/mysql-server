/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef XPL_PLUGIN_H
#define XPL_PLUGIN_H

#define XPL_PLUGIN_NAME "mysqlx"
#define XPL_STATUS_VARIABLE_NAME(NAME) "Mysqlx_" NAME

#define XPL_PLUGIN_VERSION_MAJOR 1
#define XPL_PLUGIN_VERSION_MINOR 0
#define XPL_PLUGIN_VERSION_PATCH 2

#define XPL_STR(VALUE_TO_STRING) #VALUE_TO_STRING
#define XPL_STR_DEF(EXPAND_DEFINITION) XPL_STR(EXPAND_DEFINITION)
#define XPL_PLUGIN_VERSION ( (XPL_PLUGIN_VERSION_MAJOR << 8) | XPL_PLUGIN_VERSION_MINOR )
#define XPL_PLUGIN_VERSION_STRING XPL_STR_DEF(XPL_PLUGIN_VERSION_MAJOR) "." XPL_STR_DEF(XPL_PLUGIN_VERSION_MINOR) "." XPL_STR_DEF(XPL_PLUGIN_VERSION_PATCH)

typedef void (*Xpl_status_variable_get)(THD *, st_mysql_show_var *, char *);

#endif /* XPL_PLUGIN_H */
