/*
 Copyright 2010 Sun Microsystems, Inc.
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
 * jtie_tconv.hpp
 */

#ifndef jtie_tconv_hpp
#define jtie_tconv_hpp

// ---------------------------------------------------------------------------
// Java <-> C type conversion trait
// ---------------------------------------------------------------------------

// XXX expand on the documentation of this important class

/**
 * This type describes aspects of the mapping of a Java and C++ type.
 *
 * As a pure trait type, this class consist of type members only and
 * is not instantiated at runtime.  The members of this class are...
 *
 * @see http://www.research.att.com/~bs/glossary.html
 */
// XXX document conversion requirements:
//   JF_t <-> JA_t conversions by cast<>
//   JA_t <-> CA_t conversions by Param<>, Target<>, Result<>
//   CA_t <-> CF_t conversions by assignment
//
template< typename JFT, typename CFT, typename JAT = JFT, typename CAT = CFT >
struct ttrait {
    typedef JFT JF_t; // Java formal parameter/result type
    typedef JAT JA_t; // Java actual parameter/result type
    typedef CFT CF_t; // C formal parameter/result type
    typedef CAT CA_t; // C actual parameter/result type

    // XXX pure trait, no data members, still declare c'tor private?
};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_hpp
