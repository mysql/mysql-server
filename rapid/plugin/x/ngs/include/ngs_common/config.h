/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef MYSQLX_XPL_CONFIG_H
#define MYSQLX_XPL_CONFIG_H


#ifdef HAVE_YASSL
#define IS_YASSL_OR_OPENSSL(Y, O) Y
#else // HAVE_YASSL
#define IS_YASSL_OR_OPENSSL(Y, O) O
#endif // HAVE_YASSL

#if defined(HAVE_SYS_UN_H)
#define HAVE_UNIX_SOCKET(YES,NO) YES
#else
#define HAVE_UNIX_SOCKET(YES,NO) NO
#endif // defined(HAVE_SYS_UN_H)

#endif // MYSQLX_XPL_CONFIG_H
