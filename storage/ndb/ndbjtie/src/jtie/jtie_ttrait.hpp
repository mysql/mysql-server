/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_ttrait.hpp
 */

#ifndef jtie_ttrait_hpp
#define jtie_ttrait_hpp

//#include <cstring>
//#include "helpers.hpp"
//#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// infrastructure code: Java <-> C type conversion ttrait
// ---------------------------------------------------------------------------

template< typename JFT, typename CFT,
          typename JAT = JFT, typename CAT = CFT >
struct ttrait
{
    typedef JFT JF_t; // Java formal paramater/result type
    typedef JAT JA_t; // Java actual paramater/result type
    typedef CAT CA_t; // C formal paramater/result type
    typedef CFT CF_t; // C actual paramater/result type
};

/*
template< typename CFT,
          typename CAT >
struct ttrait< jobject, CFT, j_n_ByteBuffer, CAT >
{
    typedef JFT JF_t; // Java formal paramater/result type
    typedef JAT JA_t; // Java actual paramater/result type
    typedef CAT CA_t; // C formal paramater/result type
    typedef CFT CF_t; // C actual paramater/result type
};
*/

 /*
// specialization for const C formal parameter/result types
template< typename JFT, typename CFT >
struct ttrait< JFT, const CFT >
{
    typedef JFT JF_t;
    typedef JFT JA_t;
    typedef CFT CA_t;       // strip const from the actual type
    typedef const CFT CF_t; // keep const in formal type
};
 */

/*
template<>
struct ttrait< jobject, const signed int, j_n_ByteBuffer, signed int >
{
    typedef jint JF_t; // Java formal paramater/result type
    typedef jint JA_t; // Java actual paramater/result type
    typedef CAT CA_t; // C formal paramater/result type
    typedef CFT CF_t; // C actual paramater/result type
};
*/

#endif // jtie_ttrait_hpp
