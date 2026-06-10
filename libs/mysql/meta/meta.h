// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_META_META_H
#define MYSQL_META_META_H

/// @file
/// Experimental API header

// Is_same_as_all<T, U, ...> is true if all the given types are the same type.
#include "mysql/meta/is_same_as_all.h"

// Is_either<T, U...> is true if T is the same type as any of the U.
#include "mysql/meta/is_either.h"

// Is_charlike<T> is true if T is char, unsigned char, or std::byte.
// Is_pointer_to_charlike<T> is true if T is pointer to one of those types.
// Is_stringlike<T> is true if T has a `data` and a `size` member.
#include "mysql/meta/is_charlike.h"

// Is_const_ref<T> is true if T has const and reference qualifiers.
#include "mysql/meta/is_const_ref.h"

// Is_pointer<T> is equivalent to std::is_pointer_v<T>, but it is a concept,
// which is sometimes needed. Is_pointer_to<T, U> is true if T is a pointer and
// the pointed-to type is U.
#include "mysql/meta/is_pointer.h"

// Is_same_ignore_const<T, U> is true if T and U are the same or differ only in
// const-ness.
#include "mysql/meta/is_same_ignore_const.h"

// Is_specialization<T, U> is true if class T is a specialization of class
// template U, where U takes type template arguments. Is_nontype_specialization
// is the same, but works when U takes non-type template arguments.
#include "mysql/meta/is_specialization.h"

// Optional_is_same<T[, U]> is true if U is omitted or void, or T and U are the
// same type.
#include "mysql/meta/optional_is_same.h"

#endif  // ifndef MYSQL_META_META_H
