#ifndef SQL_REGEXP_ERRORS_H_
#define SQL_REGEXP_ERRORS_H_

/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <unicode/parseerr.h>  // UParseError
#include <unicode/utypes.h>    // UErrorCode

namespace regexp {

bool check_icu_status(UErrorCode status,
                      const UParseError *parse_error = nullptr);

}  // namespace regexp

#endif  // SQL_REGEXP_ERRORS_H_
