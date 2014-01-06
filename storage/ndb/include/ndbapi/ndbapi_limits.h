/* Copyright (c) 2003-2005, 2007, 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#ifndef NDBAPI_LIMITS_H
#define NDBAPI_LIMITS_H

#define NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY 32
#define NDB_MAX_ATTRIBUTES_IN_INDEX NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY
#define NDB_MAX_ATTRIBUTES_IN_TABLE 128

#define NDB_MAX_TUPLE_SIZE_IN_WORDS 2013
#define NDB_MAX_KEYSIZE_IN_WORDS 1023
#define NDB_MAX_KEY_SIZE (NDB_MAX_KEYSIZE_IN_WORDS*4)
#define NDB_MAX_TUPLE_SIZE (NDB_MAX_TUPLE_SIZE_IN_WORDS*4)
#define NDB_MAX_ACTIVE_EVENTS 100

/* TUP ZATTR_BUFFER_SIZE 16384 (minus 1) minus place for getValue()s */
#define NDB_MAX_SCANFILTER_SIZE_IN_WORDS (16384 - 1 - 1024)

#endif
