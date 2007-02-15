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

/* sha.hpp provides SHA-1 digests, see RFC 3174
*/

#ifndef TAO_CRYPT_SHA_HPP
#define TAO_CRYPT_SHA_HPP

#include "hash.hpp"


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_SHA_ASM
#endif

namespace TaoCrypt {


// SHA-1 digest
class SHA : public HASHwithTransform {
public:
    enum { BLOCK_SIZE = 64, DIGEST_SIZE = 20, PAD_SIZE = 56,
           TAO_BYTE_ORDER = BigEndianOrder};   // in Bytes
    SHA() : HASHwithTransform(DIGEST_SIZE / sizeof(word32), BLOCK_SIZE)
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

#ifdef DO_SHA_ASM
    void Update(const byte* data, word32 len);
#endif
    void Init();

    SHA(const SHA&);
    SHA& operator= (const SHA&);

    void Swap(SHA&);
private:
    void Transform();
    void AsmTransform(const byte* data, word32 times);
};


inline void swap(SHA& a, SHA& b)
{
    a.Swap(b);
}

} // namespace


#endif // TAO_CRYPT_SHA_HPP

