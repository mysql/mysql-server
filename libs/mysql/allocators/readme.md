\page PageLibsMysqlAllocators Library: Allocators

<!---
Copyright (c) 2024, Oracle and/or its affiliates.
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
MySQL Allocators Library
========================
-->

Code documentation: @ref GroupLibsMysqlAllocators.

## Summary

Allows libraries to allocate memory such that:
- The server can use the library in such a way that the memory is
  performance_schema-instrumented.
- Other code can use the library without performance_schema instrumentation.
- The library is never dependent on performance_schema.

## Usage

This library provides the `Memory_resource` class, which is used in different
ways depending on who you are:

- *Library author*: If your library needs to allocate memory, add a
  `Memory_resource` parameter to either the function that allocates memory or
  the constructor for the class that manages the memory. The parameter must be
  passed by value, not reference or pointer. Allocate memory through this
  object. Make the parameter default to `Memory_resource()`.

- *Library user within the MySQL server*: When using the function or class that
  takes a `Memory_resource` object, call `psi_memory_resource(Psi_memory_key)`
  to get the `Memory_resource` object to pass to the function or constructor.

- *Library user outside the MySQL server*: When using the function or class
  constructor that takes an optional `Memory_resource` parameter, allow that
  parameter to take its default value.
