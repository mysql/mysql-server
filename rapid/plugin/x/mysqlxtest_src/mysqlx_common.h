/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _MYSQLXTEST_COMMON_H_
 #define _MYSQLXTEST_COMMON_H_


#ifdef _WIN32
 # ifdef _DLL
 #  ifdef MYSQLXTEST_EXPORTS
 #   define MYSQLXTEST_PUBLIC __declspec(dllexport)
 #  else
 #   define MYSQLXTEST_PUBLIC __declspec(dllimport)
 #  endif
 # else
 #  define MYSQLXTEST_PUBLIC
 # endif
 #else
 # define MYSQLXTEST_PUBLIC
 #endif

#ifdef NO_MYSQLXTEST
 # undef MYSQLXTEST_PUBLIC
 # define MYSQLXTEST_PUBLIC SHCORE_PUBLIC
 #endif

#endif

