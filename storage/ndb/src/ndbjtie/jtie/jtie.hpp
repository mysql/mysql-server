/*
 Copyright 2010 Sun Microsystems, Inc.
 Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_hpp
 */

#ifndef jtie_hpp
#define jtie_hpp

// ---------------------------------------------------------------------------
// JTie Template Library -- Master Include File
// ---------------------------------------------------------------------------

// API -- template types for writing an application's JNI function stubs
#include "jtie_tconv.hpp"
#include "jtie_tconv_xwidth.hpp"
#include "jtie_tconv_string.hpp"
#include "jtie_tconv_object.hpp"
#include "jtie_tconv_enum.hpp"
#include "jtie_gcalls.hpp"

// IMPL -- types & template specializations implementing the JTie API
#include "jtie_tconv_value_impl.hpp"
#include "jtie_tconv_string_impl.hpp"
#include "jtie_tconv_ptrbybb_impl.hpp"
#include "jtie_tconv_refbybb_impl.hpp"
#include "jtie_tconv_ptrbyval_impl.hpp"
#include "jtie_tconv_refbyval_impl.hpp"
#include "jtie_tconv_object_impl.hpp"
#include "jtie_tconv_enum_impl.hpp"
#include "jtie_tconv_idcache_impl.hpp"
// template default implementation must come last, after specializations
#include "jtie_tconv_impl_default.hpp"

#endif // jtie_hpp
