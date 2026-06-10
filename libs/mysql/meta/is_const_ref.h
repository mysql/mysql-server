// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_META_IS_CONST_REF_H
#define MYSQL_META_IS_CONST_REF_H

/// @file
/// Experimental API header

#include <type_traits>  // is_reference_v

/// @addtogroup GroupLibsMysqlMeta
/// @{

namespace mysql::meta {

/// True if Type is a const reference.
template <class Type>
concept Is_const_ref =
    std::is_reference_v<Type> && std::is_const_v<std::remove_reference_t<Type>>;

}  // namespace mysql::meta

// addtogroup GroupLibsMysqlMeta
/// @}

#endif  // ifndef MYSQL_META_IS_CONST_REF_H
