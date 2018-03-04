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

/* md4.hpp provides MD4 digest support
 * WANRING: MD4 is considered insecure, only use if you have to, e.g., yaSSL
 * libcurl supports needs this for NTLM authentication
*/

#ifndef TAO_CRYPT_MD4_HPP
#define TAO_CRYPT_MD4_HPP

#include "hash.hpp"

namespace TaoCrypt {


// MD4 digest
class MD4 : public HASHwithTransform {
public:
    enum { BLOCK_SIZE = 64, DIGEST_SIZE = 16, PAD_SIZE = 56,
           TAO_BYTE_ORDER = LittleEndianOrder };   // in Bytes
    MD4() : HASHwithTransform(DIGEST_SIZE / sizeof(word32), BLOCK_SIZE) 
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    MD4(const MD4&);
    MD4& operator= (const MD4&);

    void Init();
    void Swap(MD4&);
private:
    void Transform();
};

inline void swap(MD4& a, MD4& b)
{
    a.Swap(b);
}


} // namespace

#endif // TAO_CRYPT_MD4_HPP

