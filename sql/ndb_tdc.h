/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_TDC_H
#define NDB_TDC_H

/*
 Wrappers functions for working with MySQL Server's table definition cache
*/

// Close all tables
bool ndb_tdc_close_cached_tables(void);

// Close one named table
bool ndb_tdc_close_cached_table(class THD* thd,
                                const char* dbname, const char* tabname);

#endif
