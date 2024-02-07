/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_THD_ATTRIBUTES_BITS_H
#define MYSQL_THD_ATTRIBUTES_BITS_H

#define STATUS_SESSION_OK 0x0001
#define STATUS_SESSION_KILLED 0x0002
#define STATUS_SESSION_KILLED_BAD_DATA 0x0004
#define STATUS_QUERY_KILLED 0x0008
#define STATUS_QUERY_TIMEOUT 0x0010

#define STATUS_DA_EMPTY 0x0020
#define STATUS_DA_OK 0x0040
#define STATUS_DA_EOF 0x0080
#define STATUS_DA_ERROR 0x0100
#define STATUS_DA_FATAL_ERROR 0x0200
#define STATUS_DA_DISABLED 0x0400

#endif /* MYSQL_THD_ATTRIBUTES_BITS_H */
