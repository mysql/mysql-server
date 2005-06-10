/* md5.cpp                                
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


/* based on Wei Dai's md5.cpp from CryptoPP */

#include "runtime.hpp"
#include "md5.hpp"
#include "algorithm.hpp"    // mySTL::swap

namespace TaoCrypt {

void MD5::Init()
{
    digest_[0] = 0x67452301L;
    digest_[1] = 0xefcdab89L;
    digest_[2] = 0x98badcfeL;
    digest_[3] = 0x10325476L;

    buffLen_ = 0;
    length_  = 0;
}


MD5::MD5(const MD5& that) : HASHwithTransform(DIGEST_SIZE / sizeof(word32),
                                              BLOCK_SIZE) 
{ 
    buffLen_ = that.buffLen_;
    length_  = that.length_;

    memcpy(digest_, that.digest_, DIGEST_SIZE);
    memcpy(buffer_, that.buffer_, BLOCK_SIZE);
}

MD5& MD5::operator= (const MD5& that)
{
    MD5 tmp(that);
    Swap(tmp);

    return *this;
}


void MD5::Swap(MD5& other)
{
    mySTL::swap(length_,  other.length_);
    mySTL::swap(buffLen_, other.buffLen_);

    memcpy(digest_, other.digest_, DIGEST_SIZE);
    memcpy(buffer_, other.buffer_, BLOCK_SIZE);
}


void MD5::Transform()
{
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, data, s) \
    w = rotlFixed(w + f(x, y, z) + data, s) + x

    // Copy context->state[] to working vars 
    word32 a = digest_[0];
    word32 b = digest_[1];
    word32 c = digest_[2];
    word32 d = digest_[3];

    MD5STEP(F1, a, b, c, d, buffer_[0]  + 0xd76aa478,  7);
    MD5STEP(F1, d, a, b, c, buffer_[1]  + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, buffer_[2]  + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, buffer_[3]  + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, buffer_[4]  + 0xf57c0faf,  7);
    MD5STEP(F1, d, a, b, c, buffer_[5]  + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, buffer_[6]  + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, buffer_[7]  + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, buffer_[8]  + 0x698098d8,  7);
    MD5STEP(F1, d, a, b, c, buffer_[9]  + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, buffer_[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, buffer_[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, buffer_[12] + 0x6b901122,  7);
    MD5STEP(F1, d, a, b, c, buffer_[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, buffer_[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, buffer_[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, buffer_[1]  + 0xf61e2562,  5);
    MD5STEP(F2, d, a, b, c, buffer_[6]  + 0xc040b340,  9);
    MD5STEP(F2, c, d, a, b, buffer_[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, buffer_[0]  + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, buffer_[5]  + 0xd62f105d,  5);
    MD5STEP(F2, d, a, b, c, buffer_[10] + 0x02441453,  9);
    MD5STEP(F2, c, d, a, b, buffer_[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, buffer_[4]  + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, buffer_[9]  + 0x21e1cde6,  5);
    MD5STEP(F2, d, a, b, c, buffer_[14] + 0xc33707d6,  9);
    MD5STEP(F2, c, d, a, b, buffer_[3]  + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, buffer_[8]  + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, buffer_[13] + 0xa9e3e905,  5);
    MD5STEP(F2, d, a, b, c, buffer_[2]  + 0xfcefa3f8,  9);
    MD5STEP(F2, c, d, a, b, buffer_[7]  + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, buffer_[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, buffer_[5]  + 0xfffa3942,  4);
    MD5STEP(F3, d, a, b, c, buffer_[8]  + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, buffer_[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, buffer_[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, buffer_[1]  + 0xa4beea44,  4);
    MD5STEP(F3, d, a, b, c, buffer_[4]  + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, buffer_[7]  + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, buffer_[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, buffer_[13] + 0x289b7ec6,  4);
    MD5STEP(F3, d, a, b, c, buffer_[0]  + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, buffer_[3]  + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, buffer_[6]  + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, buffer_[9]  + 0xd9d4d039,  4);
    MD5STEP(F3, d, a, b, c, buffer_[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, buffer_[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, buffer_[2]  + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, buffer_[0]  + 0xf4292244,  6);
    MD5STEP(F4, d, a, b, c, buffer_[7]  + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, buffer_[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, buffer_[5]  + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, buffer_[12] + 0x655b59c3,  6);
    MD5STEP(F4, d, a, b, c, buffer_[3]  + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, buffer_[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, buffer_[1]  + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, buffer_[8]  + 0x6fa87e4f,  6);
    MD5STEP(F4, d, a, b, c, buffer_[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, buffer_[6]  + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, buffer_[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, buffer_[4]  + 0xf7537e82,  6);
    MD5STEP(F4, d, a, b, c, buffer_[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, buffer_[2]  + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, buffer_[9]  + 0xeb86d391, 21);
    
    // Add the working vars back into digest state[]
    digest_[0] += a;
    digest_[1] += b;
    digest_[2] += c;
    digest_[3] += d;

    // Wipe variables
    a = b = c = d = 0;

    buffLen_ = 0;
    length_ += 512;
}

} // namespace

