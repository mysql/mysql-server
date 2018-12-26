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
 * jtie_tconv_impl_default.hpp
 */

#ifndef jtie_tconv_impl_default_hpp
#define jtie_tconv_impl_default_hpp

#include "helpers.hpp"

// ---------------------------------------------------------------------------
// Java <-> C type conversions
// ---------------------------------------------------------------------------

/**
 * A class template with static functions for conversion of parameter data
 * (Java <-> C++).
 */
template< typename J, typename C >
struct Param
{
private:
    // prohibit instantiation
    Param() {
        // prohibit unsupported template specializations
        is_supported_type_mapping< J, C >();
    }
};

/**
 * A class template with static functions for conversion of target objects
 * of method invocations (Java -> C++).
 */
template< typename J, typename C >
struct Target
{
private:
    // prohibit instantiation
    Target() {
        // prohibit unsupported template specializations
        is_supported_type_mapping< J, C >();
    }
};

/**
 * A class template with static functions for conversion of function call
 * or data access result data (Java <- C++).
 */
template< typename J, typename C >
struct Result
{
private:
    // prohibit instantiation
    Result() {
        // prohibit unsupported template specializations
        is_supported_type_mapping< J, C >();
    }
};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_impl_default_hpp
