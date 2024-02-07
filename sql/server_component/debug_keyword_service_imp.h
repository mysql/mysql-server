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

#ifndef DEBUG_KEYWORD_SERVICE_IMP_H
#define DEBUG_KEYWORD_SERVICE_IMP_H

#include "mysql/components/component_implementation.h"
#include "mysql/components/services/mysql_debug_keyword_service.h"

#if !defined(NDEBUG)

/**
  An implementation of debug_keyword service methods
*/
class mysql_debug_keyword_service_imp {
 public:
  /**
    Check if a debug keyword is enabled.

    @returns Returns true if enabled.
  */
  static DEFINE_BOOL_METHOD(lookup_debug_keyword, (const char *name));
};

#endif

#endif /* DEBUG_KEYWORD_SERVICE_IMP_H */