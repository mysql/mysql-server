/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_OLAP_TYPES_H
#define SQL_OLAP_TYPES_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

enum olap_type { UNSPECIFIED_OLAP_TYPE, ROLLUP_TYPE, CUBE_TYPE };

inline const char *GroupByModifierString(enum olap_type olap) {
  switch (olap) {
    case ROLLUP_TYPE: {
      return "ROLLUP";
    }
    case CUBE_TYPE: {
      return "CUBE";
    }
    default: {
      return "UNDEFINED";
    }
  }
}

/* Maximum number of Group By modifier branches to be supported is 128 */
inline int GetMaximumNumGrpByColsSupported(enum olap_type olap) {
  switch (olap) {
    case ROLLUP_TYPE: {
      return 127;
    }
    case CUBE_TYPE: {
      return 7; /* (2^7) = 128*/
    }
    default: {
      return 0;
    }
  }
}

#endif  // SQL_OLAP_TYPES_H
