/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/*
  These are server-only members of enum_mysql_show_type defined in plugin.h.
  The values are only meant for internal use and their use by plugins is
  not supported.
*/
, SHOW_KEY_CACHE_LONG,
SHOW_KEY_CACHE_LONGLONG,
SHOW_LONG_STATUS,
SHOW_DOUBLE_STATUS,
SHOW_HAVE,
SHOW_MY_BOOL,
SHOW_HA_ROWS,
SHOW_SYS,
SHOW_LONG_NOFLUSH,
SHOW_LONGLONG_STATUS,
SHOW_LEX_STRING,
SHOW_SIGNED_LONG
