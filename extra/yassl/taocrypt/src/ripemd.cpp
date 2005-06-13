/* ripemd.cpp                                
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


/* based on Wei Dai's ripemd.cpp from CryptoPP */

#include "runtime.hpp"
#include "ripemd.hpp"
#include "algorithm.hpp"    // mySTL::swap

namespace TaoCrypt {

void RIPEMD160::Init()
{
    digest_[0] = 0x67452301L;
    digest_[1] = 0xefcdab89L;
    digest_[2] = 0x98badcfeL;
    digest_[3] = 0x10325476L;
    digest_[4] = 0xc3d2e1f0L;

    buffLen_ = 0;
    length_  = 0;
}


RIPEMD160::RIPEMD160(const RIPEMD160& that)
    : HASHwithTransform(DIGEST_SIZE / sizeof(word32), BLOCK_SIZE) 
{ 
    buffLen_ = that.buffLen_;
    length_  = that.length_;

    memcpy(digest_, that.digest_, DIGEST_SIZE);
    memcpy(buffer_, that.buffer_, BLOCK_SIZE);
}


RIPEMD160& RIPEMD160::operator= (const RIPEMD160& that)
{
    RIPEMD160 tmp(that);
    Swap(tmp);

    return *this;
}


void RIPEMD160::Swap(RIPEMD160& other)
{
    mySTL::swap(length_,  other.length_);
    mySTL::swap(buffLen_, other.buffLen_);

    memcpy(digest_, other.digest_, DIGEST_SIZE);
    memcpy(buffer_, other.buffer_, BLOCK_SIZE);
}


// for all
#define F(x, y, z)    (x ^ y ^ z) 
#define G(x, y, z)    (z ^ (x & (y^z)))
#define H(x, y, z)    (z ^ (x | ~y))
#define I(x, y, z)    (y ^ (z & (x^y)))
#define J(x, y, z)    (x ^ (y | ~z))

#define k0 0
#define k1 0x5a827999UL
#define k2 0x6ed9eba1UL
#define k3 0x8f1bbcdcUL
#define k4 0xa953fd4eUL
#define k5 0x50a28be6UL
#define k6 0x5c4dd124UL
#define k7 0x6d703ef3UL
#define k8 0x7a6d76e9UL
#define k9 0

// for 160 and 320
#define Subround(f, a, b, c, d, e, x, s, k) \
    a += f(b, c, d) + x + k;\
    a = rotlFixed((word32)a, s) + e;\
    c = rotlFixed((word32)c, 10U)


void RIPEMD160::Transform()
{
    unsigned long a1, b1, c1, d1, e1, a2, b2, c2, d2, e2;
    a1 = a2 = digest_[0];
    b1 = b2 = digest_[1];
    c1 = c2 = digest_[2];
    d1 = d2 = digest_[3];
    e1 = e2 = digest_[4];

    Subround(F, a1, b1, c1, d1, e1, buffer_[ 0], 11, k0);
    Subround(F, e1, a1, b1, c1, d1, buffer_[ 1], 14, k0);
    Subround(F, d1, e1, a1, b1, c1, buffer_[ 2], 15, k0);
    Subround(F, c1, d1, e1, a1, b1, buffer_[ 3], 12, k0);
    Subround(F, b1, c1, d1, e1, a1, buffer_[ 4],  5, k0);
    Subround(F, a1, b1, c1, d1, e1, buffer_[ 5],  8, k0);
    Subround(F, e1, a1, b1, c1, d1, buffer_[ 6],  7, k0);
    Subround(F, d1, e1, a1, b1, c1, buffer_[ 7],  9, k0);
    Subround(F, c1, d1, e1, a1, b1, buffer_[ 8], 11, k0);
    Subround(F, b1, c1, d1, e1, a1, buffer_[ 9], 13, k0);
    Subround(F, a1, b1, c1, d1, e1, buffer_[10], 14, k0);
    Subround(F, e1, a1, b1, c1, d1, buffer_[11], 15, k0);
    Subround(F, d1, e1, a1, b1, c1, buffer_[12],  6, k0);
    Subround(F, c1, d1, e1, a1, b1, buffer_[13],  7, k0);
    Subround(F, b1, c1, d1, e1, a1, buffer_[14],  9, k0);
    Subround(F, a1, b1, c1, d1, e1, buffer_[15],  8, k0);

    Subround(G, e1, a1, b1, c1, d1, buffer_[ 7],  7, k1);
    Subround(G, d1, e1, a1, b1, c1, buffer_[ 4],  6, k1);
    Subround(G, c1, d1, e1, a1, b1, buffer_[13],  8, k1);
    Subround(G, b1, c1, d1, e1, a1, buffer_[ 1], 13, k1);
    Subround(G, a1, b1, c1, d1, e1, buffer_[10], 11, k1);
    Subround(G, e1, a1, b1, c1, d1, buffer_[ 6],  9, k1);
    Subround(G, d1, e1, a1, b1, c1, buffer_[15],  7, k1);
    Subround(G, c1, d1, e1, a1, b1, buffer_[ 3], 15, k1);
    Subround(G, b1, c1, d1, e1, a1, buffer_[12],  7, k1);
    Subround(G, a1, b1, c1, d1, e1, buffer_[ 0], 12, k1);
    Subround(G, e1, a1, b1, c1, d1, buffer_[ 9], 15, k1);
    Subround(G, d1, e1, a1, b1, c1, buffer_[ 5],  9, k1);
    Subround(G, c1, d1, e1, a1, b1, buffer_[ 2], 11, k1);
    Subround(G, b1, c1, d1, e1, a1, buffer_[14],  7, k1);
    Subround(G, a1, b1, c1, d1, e1, buffer_[11], 13, k1);
    Subround(G, e1, a1, b1, c1, d1, buffer_[ 8], 12, k1);

    Subround(H, d1, e1, a1, b1, c1, buffer_[ 3], 11, k2);
    Subround(H, c1, d1, e1, a1, b1, buffer_[10], 13, k2);
    Subround(H, b1, c1, d1, e1, a1, buffer_[14],  6, k2);
    Subround(H, a1, b1, c1, d1, e1, buffer_[ 4],  7, k2);
    Subround(H, e1, a1, b1, c1, d1, buffer_[ 9], 14, k2);
    Subround(H, d1, e1, a1, b1, c1, buffer_[15],  9, k2);
    Subround(H, c1, d1, e1, a1, b1, buffer_[ 8], 13, k2);
    Subround(H, b1, c1, d1, e1, a1, buffer_[ 1], 15, k2);
    Subround(H, a1, b1, c1, d1, e1, buffer_[ 2], 14, k2);
    Subround(H, e1, a1, b1, c1, d1, buffer_[ 7],  8, k2);
    Subround(H, d1, e1, a1, b1, c1, buffer_[ 0], 13, k2);
    Subround(H, c1, d1, e1, a1, b1, buffer_[ 6],  6, k2);
    Subround(H, b1, c1, d1, e1, a1, buffer_[13],  5, k2);
    Subround(H, a1, b1, c1, d1, e1, buffer_[11], 12, k2);
    Subround(H, e1, a1, b1, c1, d1, buffer_[ 5],  7, k2);
    Subround(H, d1, e1, a1, b1, c1, buffer_[12],  5, k2);

    Subround(I, c1, d1, e1, a1, b1, buffer_[ 1], 11, k3);
    Subround(I, b1, c1, d1, e1, a1, buffer_[ 9], 12, k3);
    Subround(I, a1, b1, c1, d1, e1, buffer_[11], 14, k3);
    Subround(I, e1, a1, b1, c1, d1, buffer_[10], 15, k3);
    Subround(I, d1, e1, a1, b1, c1, buffer_[ 0], 14, k3);
    Subround(I, c1, d1, e1, a1, b1, buffer_[ 8], 15, k3);
    Subround(I, b1, c1, d1, e1, a1, buffer_[12],  9, k3);
    Subround(I, a1, b1, c1, d1, e1, buffer_[ 4],  8, k3);
    Subround(I, e1, a1, b1, c1, d1, buffer_[13],  9, k3);
    Subround(I, d1, e1, a1, b1, c1, buffer_[ 3], 14, k3);
    Subround(I, c1, d1, e1, a1, b1, buffer_[ 7],  5, k3);
    Subround(I, b1, c1, d1, e1, a1, buffer_[15],  6, k3);
    Subround(I, a1, b1, c1, d1, e1, buffer_[14],  8, k3);
    Subround(I, e1, a1, b1, c1, d1, buffer_[ 5],  6, k3);
    Subround(I, d1, e1, a1, b1, c1, buffer_[ 6],  5, k3);
    Subround(I, c1, d1, e1, a1, b1, buffer_[ 2], 12, k3);

    Subround(J, b1, c1, d1, e1, a1, buffer_[ 4],  9, k4);
    Subround(J, a1, b1, c1, d1, e1, buffer_[ 0], 15, k4);
    Subround(J, e1, a1, b1, c1, d1, buffer_[ 5],  5, k4);
    Subround(J, d1, e1, a1, b1, c1, buffer_[ 9], 11, k4);
    Subround(J, c1, d1, e1, a1, b1, buffer_[ 7],  6, k4);
    Subround(J, b1, c1, d1, e1, a1, buffer_[12],  8, k4);
    Subround(J, a1, b1, c1, d1, e1, buffer_[ 2], 13, k4);
    Subround(J, e1, a1, b1, c1, d1, buffer_[10], 12, k4);
    Subround(J, d1, e1, a1, b1, c1, buffer_[14],  5, k4);
    Subround(J, c1, d1, e1, a1, b1, buffer_[ 1], 12, k4);
    Subround(J, b1, c1, d1, e1, a1, buffer_[ 3], 13, k4);
    Subround(J, a1, b1, c1, d1, e1, buffer_[ 8], 14, k4);
    Subround(J, e1, a1, b1, c1, d1, buffer_[11], 11, k4);
    Subround(J, d1, e1, a1, b1, c1, buffer_[ 6],  8, k4);
    Subround(J, c1, d1, e1, a1, b1, buffer_[15],  5, k4);
    Subround(J, b1, c1, d1, e1, a1, buffer_[13],  6, k4);

    Subround(J, a2, b2, c2, d2, e2, buffer_[ 5],  8, k5);
    Subround(J, e2, a2, b2, c2, d2, buffer_[14],  9, k5);
    Subround(J, d2, e2, a2, b2, c2, buffer_[ 7],  9, k5);
    Subround(J, c2, d2, e2, a2, b2, buffer_[ 0], 11, k5);
    Subround(J, b2, c2, d2, e2, a2, buffer_[ 9], 13, k5);
    Subround(J, a2, b2, c2, d2, e2, buffer_[ 2], 15, k5);
    Subround(J, e2, a2, b2, c2, d2, buffer_[11], 15, k5);
    Subround(J, d2, e2, a2, b2, c2, buffer_[ 4],  5, k5);
    Subround(J, c2, d2, e2, a2, b2, buffer_[13],  7, k5);
    Subround(J, b2, c2, d2, e2, a2, buffer_[ 6],  7, k5);
    Subround(J, a2, b2, c2, d2, e2, buffer_[15],  8, k5);
    Subround(J, e2, a2, b2, c2, d2, buffer_[ 8], 11, k5);
    Subround(J, d2, e2, a2, b2, c2, buffer_[ 1], 14, k5);
    Subround(J, c2, d2, e2, a2, b2, buffer_[10], 14, k5);
    Subround(J, b2, c2, d2, e2, a2, buffer_[ 3], 12, k5);
    Subround(J, a2, b2, c2, d2, e2, buffer_[12],  6, k5);

    Subround(I, e2, a2, b2, c2, d2, buffer_[ 6],  9, k6); 
    Subround(I, d2, e2, a2, b2, c2, buffer_[11], 13, k6);
    Subround(I, c2, d2, e2, a2, b2, buffer_[ 3], 15, k6);
    Subround(I, b2, c2, d2, e2, a2, buffer_[ 7],  7, k6);
    Subround(I, a2, b2, c2, d2, e2, buffer_[ 0], 12, k6);
    Subround(I, e2, a2, b2, c2, d2, buffer_[13],  8, k6);
    Subround(I, d2, e2, a2, b2, c2, buffer_[ 5],  9, k6);
    Subround(I, c2, d2, e2, a2, b2, buffer_[10], 11, k6);
    Subround(I, b2, c2, d2, e2, a2, buffer_[14],  7, k6);
    Subround(I, a2, b2, c2, d2, e2, buffer_[15],  7, k6);
    Subround(I, e2, a2, b2, c2, d2, buffer_[ 8], 12, k6);
    Subround(I, d2, e2, a2, b2, c2, buffer_[12],  7, k6);
    Subround(I, c2, d2, e2, a2, b2, buffer_[ 4],  6, k6);
    Subround(I, b2, c2, d2, e2, a2, buffer_[ 9], 15, k6);
    Subround(I, a2, b2, c2, d2, e2, buffer_[ 1], 13, k6);
    Subround(I, e2, a2, b2, c2, d2, buffer_[ 2], 11, k6);

    Subround(H, d2, e2, a2, b2, c2, buffer_[15],  9, k7);
    Subround(H, c2, d2, e2, a2, b2, buffer_[ 5],  7, k7);
    Subround(H, b2, c2, d2, e2, a2, buffer_[ 1], 15, k7);
    Subround(H, a2, b2, c2, d2, e2, buffer_[ 3], 11, k7);
    Subround(H, e2, a2, b2, c2, d2, buffer_[ 7],  8, k7);
    Subround(H, d2, e2, a2, b2, c2, buffer_[14],  6, k7);
    Subround(H, c2, d2, e2, a2, b2, buffer_[ 6],  6, k7);
    Subround(H, b2, c2, d2, e2, a2, buffer_[ 9], 14, k7);
    Subround(H, a2, b2, c2, d2, e2, buffer_[11], 12, k7);
    Subround(H, e2, a2, b2, c2, d2, buffer_[ 8], 13, k7);
    Subround(H, d2, e2, a2, b2, c2, buffer_[12],  5, k7);
    Subround(H, c2, d2, e2, a2, b2, buffer_[ 2], 14, k7);
    Subround(H, b2, c2, d2, e2, a2, buffer_[10], 13, k7);
    Subround(H, a2, b2, c2, d2, e2, buffer_[ 0], 13, k7);
    Subround(H, e2, a2, b2, c2, d2, buffer_[ 4],  7, k7);
    Subround(H, d2, e2, a2, b2, c2, buffer_[13],  5, k7);

    Subround(G, c2, d2, e2, a2, b2, buffer_[ 8], 15, k8);
    Subround(G, b2, c2, d2, e2, a2, buffer_[ 6],  5, k8);
    Subround(G, a2, b2, c2, d2, e2, buffer_[ 4],  8, k8);
    Subround(G, e2, a2, b2, c2, d2, buffer_[ 1], 11, k8);
    Subround(G, d2, e2, a2, b2, c2, buffer_[ 3], 14, k8);
    Subround(G, c2, d2, e2, a2, b2, buffer_[11], 14, k8);
    Subround(G, b2, c2, d2, e2, a2, buffer_[15],  6, k8);
    Subround(G, a2, b2, c2, d2, e2, buffer_[ 0], 14, k8);
    Subround(G, e2, a2, b2, c2, d2, buffer_[ 5],  6, k8);
    Subround(G, d2, e2, a2, b2, c2, buffer_[12],  9, k8);
    Subround(G, c2, d2, e2, a2, b2, buffer_[ 2], 12, k8);
    Subround(G, b2, c2, d2, e2, a2, buffer_[13],  9, k8);
    Subround(G, a2, b2, c2, d2, e2, buffer_[ 9], 12, k8);
    Subround(G, e2, a2, b2, c2, d2, buffer_[ 7],  5, k8);
    Subround(G, d2, e2, a2, b2, c2, buffer_[10], 15, k8);
    Subround(G, c2, d2, e2, a2, b2, buffer_[14],  8, k8);

    Subround(F, b2, c2, d2, e2, a2, buffer_[12],  8, k9);
    Subround(F, a2, b2, c2, d2, e2, buffer_[15],  5, k9);
    Subround(F, e2, a2, b2, c2, d2, buffer_[10], 12, k9);
    Subround(F, d2, e2, a2, b2, c2, buffer_[ 4],  9, k9);
    Subround(F, c2, d2, e2, a2, b2, buffer_[ 1], 12, k9);
    Subround(F, b2, c2, d2, e2, a2, buffer_[ 5],  5, k9);
    Subround(F, a2, b2, c2, d2, e2, buffer_[ 8], 14, k9);
    Subround(F, e2, a2, b2, c2, d2, buffer_[ 7],  6, k9);
    Subround(F, d2, e2, a2, b2, c2, buffer_[ 6],  8, k9);
    Subround(F, c2, d2, e2, a2, b2, buffer_[ 2], 13, k9);
    Subround(F, b2, c2, d2, e2, a2, buffer_[13],  6, k9);
    Subround(F, a2, b2, c2, d2, e2, buffer_[14],  5, k9);
    Subround(F, e2, a2, b2, c2, d2, buffer_[ 0], 15, k9);
    Subround(F, d2, e2, a2, b2, c2, buffer_[ 3], 13, k9);
    Subround(F, c2, d2, e2, a2, b2, buffer_[ 9], 11, k9);
    Subround(F, b2, c2, d2, e2, a2, buffer_[11], 11, k9);

    c1         = digest_[1] + c1 + d2;
    digest_[1] = digest_[2] + d1 + e2;
    digest_[2] = digest_[3] + e1 + a2;
    digest_[3] = digest_[4] + a1 + b2;
    digest_[4] = digest_[0] + b1 + c2;
    digest_[0] = c1;

    buffLen_ = 0;
    length_ += 512;
}


} // namespace TaoCrypt
