/*
   Copyright (C) 2000-2007 MySQL AB
   Use is subject to license terms

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

/* md5.hpp provides MD5 digest support, see RFC 1321
*/

#ifndef TAO_CRYPT_MD5_HPP
#define TAO_CRYPT_MD5_HPP

#include "hash.hpp"


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_MD5_ASM
#endif

namespace TaoCrypt {


// MD5 digest
class MD5 : public HASHwithTransform {
public:
    enum { BLOCK_SIZE = 64, DIGEST_SIZE = 16, PAD_SIZE = 56,
           TAO_BYTE_ORDER = LittleEndianOrder };   // in Bytes
    MD5() : HASHwithTransform(DIGEST_SIZE / sizeof(word32), BLOCK_SIZE) 
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    MD5(const MD5&);
    MD5& operator= (const MD5&);

#ifdef DO_MD5_ASM
    void Update(const byte*, word32);
#endif

    void Init();
    void Swap(MD5&);
private:
    void Transform();
    void AsmTransform(const byte* data, word32 times);
};

inline void swap(MD5& a, MD5& b)
{
    a.Swap(b);
}


} // namespace

#endif // TAO_CRYPT_MD5_HPP

