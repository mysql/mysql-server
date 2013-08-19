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

/* hash.hpp provides a base for digest types
*/


#ifndef TAO_CRYPT_HASH_HPP
#define TAO_CRYPT_HASH_HPP

#include "misc.hpp"

namespace TaoCrypt {


// HASH
class HASH : public virtual_base {
public:
    virtual ~HASH() {}

    virtual void Update(const byte*, word32) = 0;
    virtual void Final(byte*)                = 0;

    virtual void Init() = 0;

    virtual word32 getBlockSize()  const = 0;
    virtual word32 getDigestSize() const = 0;
};


// HASH with Transform
class HASHwithTransform : public HASH {
public:
    HASHwithTransform(word32 digSz, word32 buffSz);
    virtual ~HASHwithTransform() {}
    virtual ByteOrder getByteOrder()  const = 0;
    virtual word32    getPadSize()    const = 0;

    virtual void Update(const byte*, word32);
    virtual void Final(byte*);

    word32  GetBitCountLo() const { return  loLen_ << 3; }
    word32  GetBitCountHi() const { return (loLen_ >> (8*sizeof(loLen_) - 3)) +
                                           (hiLen_ << 3); } 
    enum { MaxDigestSz = 8, MaxBufferSz = 64 };
protected:
    typedef word32 HashLengthType;
    word32          buffLen_;   // in bytes
    HashLengthType  loLen_;     // length in bytes
    HashLengthType  hiLen_;     // length in bytes
    word32          digest_[MaxDigestSz];
    word32          buffer_[MaxBufferSz / sizeof(word32)];

    virtual void Transform() = 0;

    void AddLength(word32);
};


#ifdef WORD64_AVAILABLE

// 64-bit HASH with Transform
class HASH64withTransform : public HASH {
public:
    HASH64withTransform(word32 digSz, word32 buffSz);
    virtual ~HASH64withTransform() {}
    virtual ByteOrder getByteOrder()  const = 0;
    virtual word32    getPadSize()    const = 0;

    virtual void Update(const byte*, word32);
    virtual void Final(byte*);

    word32  GetBitCountLo() const { return  loLen_ << 3; }
    word32  GetBitCountHi() const { return (loLen_ >> (8*sizeof(loLen_) - 3)) +
                                           (hiLen_ << 3); } 
    enum { MaxDigestSz = 8, MaxBufferSz = 128 };
protected:
    typedef word32 HashLengthType;
    word32          buffLen_;   // in bytes
    HashLengthType  loLen_;     // length in bytes
    HashLengthType  hiLen_;     // length in bytes
    word64          digest_[MaxDigestSz];
    word64          buffer_[MaxBufferSz / sizeof(word64)];

    virtual void Transform() = 0;

    void AddLength(word32);
};

#endif // WORD64_AVAILABLE


} // namespace

#endif // TAO_CRYPT_HASH_HPP
