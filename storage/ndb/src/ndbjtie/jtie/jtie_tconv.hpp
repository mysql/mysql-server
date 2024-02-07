/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 Use is subject to license terms.

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
 * @see https://www.stroustrup.com/glossary.html
 */
// XXX document conversion requirements:
//   JF_t <-> JA_t conversions by cast<>
//   JA_t <-> CA_t conversions by Param<>, Target<>, Result<>
//   CA_t <-> CF_t conversions by assignment
//
template <typename JFT, typename CFT, typename JAT = JFT, typename CAT = CFT>
struct ttrait {
  typedef JFT JF_t;  // Java formal parameter/result type
  typedef JAT JA_t;  // Java actual parameter/result type
  typedef CFT CF_t;  // C formal parameter/result type
  typedef CAT CA_t;  // C actual parameter/result type

  // XXX pure trait, no data members, still declare c'tor private?
};

// ---------------------------------------------------------------------------

#endif  // jtie_tconv_hpp
