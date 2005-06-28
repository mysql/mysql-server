/* hash.cpp                                
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

/* hash.cpp implements a base for digest types
*/

#include "runtime.hpp"
#include <string.h>
#include <assert.h>

#include "hash.hpp"


namespace TaoCrypt {


HASHwithTransform::HASHwithTransform(word32 digSz, word32 buffSz)
{
    assert(digSz  <= MaxDigestSz);
    assert(buffSz <= MaxBufferSz);
}


// Update digest with data of size len, do in blocks
void HASHwithTransform::Update(const byte* data, word32 len)
{
    // do block size increments
    word32 blockSz = getBlockSize();
    byte*  local   = reinterpret_cast<byte*>(buffer_);

    while (len) {
        word32 add = min(len, blockSz - buffLen_);
        memcpy(&local[buffLen_], data, add);

        buffLen_ += add;
        data     += add;
        len      -= add;

        if (buffLen_ == blockSz) {
            ByteReverseIf(local, local, blockSz, getByteOrder());
            Transform();
        }
    }
}


// Final process, place digest in hash
void HASHwithTransform::Final(byte* hash)
{
    word32    blockSz   = getBlockSize();
    word32    digestSz  = getDigestSize();
    word32    padSz     = getPadSize();
    ByteOrder order     = getByteOrder();
    word32    prePadLen = length_ + buffLen_ * 8;  // in bits
    byte*     local     = reinterpret_cast<byte*>(buffer_);

    local[buffLen_++] = 0x80;  // add 1

    // pad with zeros
    if (buffLen_ > padSz) {
        while (buffLen_ < blockSz) local[buffLen_++] = 0;
        ByteReverseIf(local, local, blockSz, order);
        Transform();
    }
    while (buffLen_ < padSz) local[buffLen_++] = 0;

    ByteReverseIf(local, local, blockSz, order);
    
    word32 hiSize = 0;  // for future 64 bit length TODO:
    memcpy(&local[padSz],   order ? &hiSize : &prePadLen, sizeof(prePadLen));
    memcpy(&local[padSz+4], order ? &prePadLen : &hiSize, sizeof(prePadLen));


    Transform();
    ByteReverseIf(digest_, digest_, digestSz, order);
    memcpy(hash, digest_, digestSz);

    Init();  // reset state
}

} // namespace
