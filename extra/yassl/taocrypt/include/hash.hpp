/* hash.hpp                                
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
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* hash.hpp provides a base for digest types
*/


#ifndef TAO_CRYPT_HASH_HPP
#define TAO_CRYPT_HASH_HPP

#include "misc.hpp"

namespace TaoCrypt {


// HASH
class HASH {
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
    HASHwithTransform(word32 digSz, word32 buffSz) 
        : digest_(new word32[digSz]), buffer_(new byte[buffSz]) {}
    virtual ~HASHwithTransform() { delete[] buffer_; delete[] digest_; }

    virtual ByteOrder getByteOrder()  const = 0;
    virtual word32    getPadSize()    const = 0;

    virtual void Update(const byte*, word32);
    virtual void Final(byte*);
protected:
    word32  buffLen_;
    word32  length_;    // in Bits
    word32* digest_;
    byte*   buffer_;

    virtual void Transform() = 0;
};


} // namespace

#endif // TAO_CRYPT_HASH_HPP
