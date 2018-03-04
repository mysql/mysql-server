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

/* md2.hpp provides MD2 digest support, see RFC 1319
*/

#ifndef TAO_CRYPT_MD2_HPP
#define TAO_CRYPT_MD2_HPP


#include "hash.hpp"
#include "block.hpp"


namespace TaoCrypt {


// MD2 digest
class MD2 : public HASH {
public:
    enum { BLOCK_SIZE = 16, DIGEST_SIZE = 16, PAD_SIZE = 16, X_SIZE = 48 };
    MD2();

    word32 getBlockSize()  const { return BLOCK_SIZE; }
    word32 getDigestSize() const { return DIGEST_SIZE; }

    void Update(const byte*, word32);
    void Final(byte*);

    void Init();
    void Swap(MD2&);
private:
    ByteBlock X_, C_, buffer_;
    word32    count_;           // bytes % PAD_SIZE

    MD2(const MD2&);
    MD2& operator=(const MD2&);
};

inline void swap(MD2& a, MD2& b)
{
    a.Swap(b);
}


} // namespace

#endif // TAO_CRYPT_MD2_HPP

