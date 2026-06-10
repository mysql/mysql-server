\page PageLibsMysqlDebugging Library: Debugging

<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->

<!--
MySQL Library: Debugging
========================
-->

Code documentation: @ref GroupLibsMysqlDebugging.

This library provides general-purpose debugging tools.  Currently, it contains
the following:

- my_scoped_trace.h: The macro MY_SCOPED_TRACE, which can be used as a
  thread-safe alternative to SCOPED_TRACE in unittests.

- object_lifetime_tracker.h: Classes that inherit from `Object_lifetime_tracker`
  will have all invocations of constructors, assignments, and destructors logged
  to stdout. This is useful when analysing memory bugs.

- oom_test.h: This is a test utility to test out-of-memory handling.

- unittest_assertions.h: The following test assertions:

  - macro ASSERT_VOID(x): assert at compile-time that the result type from
    expression `x` is void, and execute `x`.

  - macro ASSERT_OK(x): assert that x == mysql::utils::Return_status::ok

  - macro ASSERT_ERROR(x): assert that x == mysql::utils::Return_status::error

  - test_eq(x, y): assert that x==y, y==x, !(x!=y), !(y!=x)

  - test_cmp(x, y, OP): assert that (x OP y)==(n OP 0), where OP is one
    of ==, !=, <, <=, >, >=, or <=>.
