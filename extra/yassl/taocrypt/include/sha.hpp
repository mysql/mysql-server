/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

// SHA-256 digest
class SHA256 : public HASHwithTransform {
public:
    enum { BLOCK_SIZE = 64, DIGEST_SIZE = 32, PAD_SIZE = 56,
           TAO_BYTE_ORDER = BigEndianOrder};   // in Bytes
    SHA256() : HASHwithTransform(DIGEST_SIZE / sizeof(word32), BLOCK_SIZE)
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    void Init();

    SHA256(const SHA256&);
    SHA256& operator= (const SHA256&);

    void Swap(SHA256&);
private:
    void Transform();
};


// SHA-224 digest
class SHA224 : public HASHwithTransform {
public:
    enum { BLOCK_SIZE = 64, DIGEST_SIZE = 28, PAD_SIZE = 56,
           TAO_BYTE_ORDER = BigEndianOrder};   // in Bytes
    SHA224() : HASHwithTransform(SHA256::DIGEST_SIZE /sizeof(word32),BLOCK_SIZE)
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    void Init();

    SHA224(const SHA224&);
    SHA224& operator= (const SHA224&);

    void Swap(SHA224&);
private:
    void Transform();
};


#ifdef WORD64_AVAILABLE

// SHA-512 digest
class SHA512 : public HASH64withTransform {
public:
    enum { BLOCK_SIZE = 128, DIGEST_SIZE = 64, PAD_SIZE = 112,
           TAO_BYTE_ORDER = BigEndianOrder};   // in Bytes
    SHA512() : HASH64withTransform(DIGEST_SIZE / sizeof(word64), BLOCK_SIZE)
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    void Init();

    SHA512(const SHA512&);
    SHA512& operator= (const SHA512&);

    void Swap(SHA512&);
private:
    void Transform();
};


// SHA-384 digest
class SHA384 : public HASH64withTransform {
public:
    enum { BLOCK_SIZE = 128, DIGEST_SIZE = 48, PAD_SIZE = 112,
           TAO_BYTE_ORDER = BigEndianOrder};   // in Bytes
    SHA384() : HASH64withTransform(SHA512::DIGEST_SIZE/ sizeof(word64),
                                   BLOCK_SIZE)
                { Init(); }
    ByteOrder getByteOrder()  const { return ByteOrder(TAO_BYTE_ORDER); }
    word32    getBlockSize()  const { return BLOCK_SIZE; }
    word32    getDigestSize() const { return DIGEST_SIZE; }
    word32    getPadSize()    const { return PAD_SIZE; }

    void Init();

    SHA384(const SHA384&);
    SHA384& operator= (const SHA384&);

    void Swap(SHA384&);
private:
    void Transform();
};

enum { MAX_SHA2_DIGEST_SIZE = 64 };   // SHA512

#else

enum { MAX_SHA2_DIGEST_SIZE = 32 };   // SHA256

#endif // WORD64_AVAILABLE


} // namespace


#endif // TAO_CRYPT_SHA_HPP

