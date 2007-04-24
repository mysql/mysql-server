/*
   Copyright (C) 2000-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
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
