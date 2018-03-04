/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
*/

/* type_traits defines fundamental types
 * see discussion in C++ Templates, $19.1
*/


#ifndef TAO_CRYPT_TYPE_TRAITS_HPP
#define TAO_CRYPT_TYPE_TRAITS_HPP

#include "types.hpp"

namespace TaoCrypt {


// primary template: in general T is not a fundamental type

template <typename T>
class IsFundamentalType {
    public:
        enum { Yes = 0, No = 1 };
};


// macro to specialize for fundamental types
#define MK_FUNDAMENTAL_TYPE(T)                  \
    template<> class IsFundamentalType<T> {     \
        public:                                 \
            enum { Yes = 1, No = 0 };           \
    };


MK_FUNDAMENTAL_TYPE(void)

MK_FUNDAMENTAL_TYPE(bool)
MK_FUNDAMENTAL_TYPE(         char)
MK_FUNDAMENTAL_TYPE(signed   char)
MK_FUNDAMENTAL_TYPE(unsigned char)

MK_FUNDAMENTAL_TYPE(signed   short)
MK_FUNDAMENTAL_TYPE(unsigned short)
MK_FUNDAMENTAL_TYPE(signed   int)
MK_FUNDAMENTAL_TYPE(unsigned int)
MK_FUNDAMENTAL_TYPE(signed   long)
MK_FUNDAMENTAL_TYPE(unsigned long)

MK_FUNDAMENTAL_TYPE(float)
MK_FUNDAMENTAL_TYPE(     double)
MK_FUNDAMENTAL_TYPE(long double)

#if defined(WORD64_AVAILABLE) && defined(WORD64_IS_DISTINCT_TYPE)
    MK_FUNDAMENTAL_TYPE(word64)
#endif


#undef MK_FUNDAMENTAL_TYPE


} // namespace

#endif // TAO_CRYPT_TYPE_TRAITS_HPP
