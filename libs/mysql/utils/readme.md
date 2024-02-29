\page PageLibsMysqlUtils Library: Utils

<!---
Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms, as
designated in a particular file or component or in included license
documentation. The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
-->


<!--
MySQL Library: Utils
====================
-->

Code documentation: @ref GroupLibsMysqlUtils.

This is a header-only library, containing various utility functions, such as:

- bounded_arithmetic.h: multiplication and addition, capped at a max value
- concat.h: vararg function to concatenate many values to a string
- deprecate_header.h: macro deprecating a header
- enumeration_utils.h: to_underlying (backported from C++23), to_enumeration
- error.h: definition of Error - base class for (C++) error handling
- is_specialization.h: utility to determine if a template is a specialization
  of another.
- nodiscard.h: replacement for [[nodiscard]] that works around gcc bug
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84476
- return_status.h: enum { ok, error }
