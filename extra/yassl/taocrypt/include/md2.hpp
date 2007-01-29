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

