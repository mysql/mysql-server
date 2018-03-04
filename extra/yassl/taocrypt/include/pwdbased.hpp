/*
   Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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


/* pwdbased.hpp defines PBKDF2 from PKCS #5
*/


#ifndef TAO_CRYPT_PWDBASED_HPP
#define TAO_CRYPT_PWDBASED_HPP

#include <string.h>
#include "misc.hpp"
#include "block.hpp"
#include "hmac.hpp"

namespace TaoCrypt {


// From PKCS #5, T must be type suitable for HMAC<T> 
template <class T>
class PBKDF2_HMAC {
public:
    word32 MaxDerivedKeyLength() const { return 0xFFFFFFFFU;} // avoid overflow

    word32 DeriveKey(byte* derived, word32 dLen, const byte* pwd, word32 pLen,
                     const byte* salt, word32 sLen, word32 iterations) const;
}; 



template <class T>
word32 PBKDF2_HMAC<T>::DeriveKey(byte* derived, word32 dLen, const byte* pwd,
                                 word32 pLen, const byte* salt, word32 sLen,
                                 word32 iterations) const
{
	if (dLen > MaxDerivedKeyLength())
        return 0;

    ByteBlock buffer(T::DIGEST_SIZE);
	HMAC<T>   hmac;

    hmac.SetKey(pwd, pLen);

	word32 i = 1;

	while (dLen > 0) {
		hmac.Update(salt, sLen);
		word32 j;
		for (j = 0; j < 4; j++) {
			byte b = i >> ((3-j)*8);
			hmac.Update(&b, 1);
		}
		hmac.Final(buffer.get_buffer());

		word32 segmentLen = min(dLen, buffer.size());
		memcpy(derived, buffer.get_buffer(), segmentLen);

		for (j = 1; j < iterations; j++) {
			hmac.Update(buffer.get_buffer(), buffer.size());
            hmac.Final(buffer.get_buffer());
			xorbuf(derived, buffer.get_buffer(), segmentLen);
		}
		derived += segmentLen;
		dLen    -= segmentLen;
		i++;
	}
	return iterations;
}




} // naemspace

#endif // TAO_CRYPT_PWDBASED_HPP
