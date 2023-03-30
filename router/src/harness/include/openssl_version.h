/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef ROUTER_OPENSSL_VERSION_INCLUDED
#define ROUTER_OPENSSL_VERSION_INCLUDED

/**
 * build openssl version.
 *
 * Format
 * :  MNNFFPPS: major minor fix patch status
 *
 * major
 * :  4 bit
 *
 * minor
 * :  8 bit
 *
 * fix
 * :  8 bit
 *
 * patch
 * :  8 bit, 'a' = 0, 'b' = 1, ...
 *
 * status
 * :  4 bit, 0x0 dev, 0xf release, everything else beta
 *
 * see https://www.openssl.org/docs/manmaster/man3/OPENSSL_VERSION_NUMBER.html
 */
#define ROUTER_OPENSSL_VERSION_FULL(MAJOR, MINOR, FIX, PATCH, STATUS)      \
  (((MAJOR & 0xf) << 28) | ((MINOR & 0xff) << 20) | ((FIX & 0xff) << 12) | \
   ((PATCH & 0xff) << 4) | (STATUS & 0xf))

/**
 * build openssl version (pre-releases and stable).
 *
 * @see ROTUER_OPENSSL_VERSION_FULL
 */
#define ROUTER_OPENSSL_VERSION(MAJOR, MINOR, FIX) \
  ROUTER_OPENSSL_VERSION_FULL(MAJOR, MINOR, FIX, 0, 0x0)

/**
 * build openssl version (stable only).
 *
 * @see ROTUER_OPENSSL_VERSION_FULL
 */
#define ROUTER_OPENSSL_VERSION_STABLE(MAJOR, MINOR, FIX) \
  ROUTER_OPENSSL_VERSION_FULL(MAJOR, MINOR, FIX, 0, 0xf)

static_assert(ROUTER_OPENSSL_VERSION(1, 2, 3) == 0x10203000L, "failed");
static_assert(ROUTER_OPENSSL_VERSION(0, 9, 4) == 0x00904000L, "failed");
static_assert(ROUTER_OPENSSL_VERSION_STABLE(1, 2, 3) == 0x1020300fL, "failed");
static_assert(ROUTER_OPENSSL_VERSION_STABLE(0, 9, 4) == 0x0090400fL, "failed");

#endif
