/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifdef _MSC_VER
/* The use_jemalloc_allocations function is called from the memory management
initialization routines in the mysys and innobase libraries when building with
MSVC on Windows to decide whether to use jemalloc or std::malloc for memory
allocations.

Returning true from this function means that those libraries will use jemalloc.

Returning false from this function means that those libraries will use
std::malloc.

If the MSVC linker cannot find a function definition with this name, it will
fall back to using an alternate name (that is defined within the mysys and
innobase libraries to return false.)
*/
extern "C" bool use_jemalloc_allocations() { return true; }
#endif
