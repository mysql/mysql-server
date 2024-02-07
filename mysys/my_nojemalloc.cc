/*****************************************************************************

Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifdef _WIN32
/* In order to use jemalloc in the mysys library, the use_jemalloc_allocations
function must return true.

This definition of the function (in the mysys library) returns false, so by
default, the mysys library will not use jemalloc for memory allocation.

Note that the linker always uses a symbol defined in an .obj file in preference
to the same symbol defined in a lib. Thus a user of the mysys library who wants
the mysys library to use jemalloc must provide an overriding definition (i.e.
link with an obj file definition) of the use_jemalloc_allocations function that
returns true.
*/
extern "C" bool use_jemalloc_allocations() { return false; }
#endif
