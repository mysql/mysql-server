/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**
  @file

  Internal native mysql package.

  DBMS package is designed as collections of procedures which are for
  administration.

  Not only procedure but also other type of object that implemented the package
  interface.
*/
#include "sql/package/package.h"
#include "sql/package/package_common.h"
#include "sql/sql_class.h"

/**
  @addtogroup Native Package

  @{
*/

namespace im {

/**
  Global singleton package container.

  @retval     package instance
*/
Package *Package::instance() {
  static Package container(key_memory_package);

  return &container;
}

/* Destructor of package */
Package::~Package() { clear_elements(); }

} /* namespace im */

/// @} (end of group )
