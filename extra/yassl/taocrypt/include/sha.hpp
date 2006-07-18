/* sha.hpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * There are special exceptions to the terms and conditions of the GPL as it
 * is applied to yaSSL. View the full text of the exception in the file
 * FLOSS-EXCEPTIONS in the directory of this software distribution.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* sha.hpp provides SHA-1 digests, see RFC 3174
*/

#ifndef TAO_CRYPT_SHA_HPP
#define TAO_CRYPT_SHA_HPP

#include "hash.hpp"

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

    void Update(const byte* data, word32 len);
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

