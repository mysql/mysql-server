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

#ifndef MYSQL_UTILS_NODISCARD_H
#define MYSQL_UTILS_NODISCARD_H

/// The function attribute [[NODISCARD]] is a replacement for
/// [[nodiscard]] to workaround a gcc bug.
///
/// This attribute can appear before a function declaration, and makes
/// it mandatory for the caller to use the return value.  Use this on
/// functions where it is always essential to check if an error
/// occurred.
///
/// It would be better if we could use [[nodiscard]], but gcc has a
/// bug that makes it ineffective is some cases:
/// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84476
/// OTOH, gnu::warn_unused_result is unaffected by the bug.
/// Once the bug is fixed, and we drop support for compiler versions
/// that have the bug, we can replace [[NODISCARD]] by [[nodiscard]].
///
/// MSVC gives a warning for gnu::warn_unused_result because it does
/// not recognize it, so we use gnu::warn_unused_result only on gnuc.
#ifdef __GNUC__
#define NODISCARD nodiscard, gnu::warn_unused_result
#else
#define NODISCARD nodiscard
#endif

#endif  // MYSQL_UTILS_NODISCARD_H
