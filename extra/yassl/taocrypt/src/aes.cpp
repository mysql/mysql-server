/* aes.cpp                                
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

/* based on Wei Dai's aes.cpp from CryptoPP */

#include "runtime.hpp"
#include "aes.hpp"
#include "stdexcept.hpp"


namespace TaoCrypt {


void AES::Process(byte* out, const byte* in, word32 sz)
{
    if (mode_ == ECB)
        ECB_Process(out, in, sz);
    else if (mode_ == CBC)
        if (dir_ == ENCRYPTION)
            CBC_Encrypt(out, in, sz);
        else
            CBC_Decrypt(out, in, sz);
}



void AES::SetKey(const byte* userKey, word32 keylen, CipherDir /*dummy*/)
{
    assert( (keylen == 16) || (keylen == 24) || (keylen == 32) );

    rounds_ = keylen/4 + 6;
    key_.New(4*(rounds_+1));

    word32 temp, *rk = key_.get_buffer();
    unsigned int i=0;

    GetUserKey(BigEndianOrder, rk, keylen/4, userKey, keylen);

    switch(keylen)
    {
    case 16:
        while (true)
        {
            temp  = rk[3];
            rk[4] = rk[0] ^
                (Te4[GETBYTE(temp, 2)] & 0xff000000) ^
                (Te4[GETBYTE(temp, 1)] & 0x00ff0000) ^
                (Te4[GETBYTE(temp, 0)] & 0x0000ff00) ^
                (Te4[GETBYTE(temp, 3)] & 0x000000ff) ^
                rcon_[i];
            rk[5] = rk[1] ^ rk[4];
            rk[6] = rk[2] ^ rk[5];
            rk[7] = rk[3] ^ rk[6];
            if (++i == 10)
                break;
            rk += 4;
        }
        break;

    case 24:
        while (true)    // for (;;) here triggers a bug in VC60 SP4 w/ Pro Pack
        {
            temp = rk[ 5];
            rk[ 6] = rk[ 0] ^
                (Te4[GETBYTE(temp, 2)] & 0xff000000) ^
                (Te4[GETBYTE(temp, 1)] & 0x00ff0000) ^
                (Te4[GETBYTE(temp, 0)] & 0x0000ff00) ^
                (Te4[GETBYTE(temp, 3)] & 0x000000ff) ^
                rcon_[i];
            rk[ 7] = rk[ 1] ^ rk[ 6];
            rk[ 8] = rk[ 2] ^ rk[ 7];
            rk[ 9] = rk[ 3] ^ rk[ 8];
            if (++i == 8)
                break;
            rk[10] = rk[ 4] ^ rk[ 9];
            rk[11] = rk[ 5] ^ rk[10];
            rk += 6;
        }
        break;

    case 32:
        while (true)
        {
            temp = rk[ 7];
            rk[ 8] = rk[ 0] ^
                (Te4[GETBYTE(temp, 2)] & 0xff000000) ^
                (Te4[GETBYTE(temp, 1)] & 0x00ff0000) ^
                (Te4[GETBYTE(temp, 0)] & 0x0000ff00) ^
                (Te4[GETBYTE(temp, 3)] & 0x000000ff) ^
                rcon_[i];
            rk[ 9] = rk[ 1] ^ rk[ 8];
            rk[10] = rk[ 2] ^ rk[ 9];
            rk[11] = rk[ 3] ^ rk[10];
            if (++i == 7)
                break;
            temp = rk[11];
            rk[12] = rk[ 4] ^
                (Te4[GETBYTE(temp, 3)] & 0xff000000) ^
                (Te4[GETBYTE(temp, 2)] & 0x00ff0000) ^
                (Te4[GETBYTE(temp, 1)] & 0x0000ff00) ^
                (Te4[GETBYTE(temp, 0)] & 0x000000ff);
            rk[13] = rk[ 5] ^ rk[12];
            rk[14] = rk[ 6] ^ rk[13];
            rk[15] = rk[ 7] ^ rk[14];

            rk += 8;
        }
        break;
    }

    if (dir_ == DECRYPTION)
    {
        unsigned int i, j;
        rk = key_.get_buffer();

        /* invert the order of the round keys: */
        for (i = 0, j = 4*rounds_; i < j; i += 4, j -= 4) {
            temp = rk[i    ]; rk[i    ] = rk[j    ]; rk[j    ] = temp;
            temp = rk[i + 1]; rk[i + 1] = rk[j + 1]; rk[j + 1] = temp;
            temp = rk[i + 2]; rk[i + 2] = rk[j + 2]; rk[j + 2] = temp;
            temp = rk[i + 3]; rk[i + 3] = rk[j + 3]; rk[j + 3] = temp;
        }
        // apply the inverse MixColumn transform to all round keys but the
        // first and the last:
        for (i = 1; i < rounds_; i++) {
            rk += 4;
            rk[0] =
                Td0[Te4[GETBYTE(rk[0], 3)] & 0xff] ^
                Td1[Te4[GETBYTE(rk[0], 2)] & 0xff] ^
                Td2[Te4[GETBYTE(rk[0], 1)] & 0xff] ^
                Td3[Te4[GETBYTE(rk[0], 0)] & 0xff];
            rk[1] =
                Td0[Te4[GETBYTE(rk[1], 3)] & 0xff] ^
                Td1[Te4[GETBYTE(rk[1], 2)] & 0xff] ^
                Td2[Te4[GETBYTE(rk[1], 1)] & 0xff] ^
                Td3[Te4[GETBYTE(rk[1], 0)] & 0xff];
            rk[2] =
                Td0[Te4[GETBYTE(rk[2], 3)] & 0xff] ^
                Td1[Te4[GETBYTE(rk[2], 2)] & 0xff] ^
                Td2[Te4[GETBYTE(rk[2], 1)] & 0xff] ^
                Td3[Te4[GETBYTE(rk[2], 0)] & 0xff];
            rk[3] =
                Td0[Te4[GETBYTE(rk[3], 3)] & 0xff] ^
                Td1[Te4[GETBYTE(rk[3], 2)] & 0xff] ^
                Td2[Te4[GETBYTE(rk[3], 1)] & 0xff] ^
                Td3[Te4[GETBYTE(rk[3], 0)] & 0xff];
        }
    }
}


typedef BlockGetAndPut<word32, BigEndian> gpBlock;

void AES::ProcessAndXorBlock(const byte* in, const byte* xOr, byte* out) const
{
    if (dir_ == ENCRYPTION)
        encrypt(in, xOr, out);
    else
        decrypt(in, xOr, out);
}


void AES::encrypt(const byte* inBlock, const byte* xorBlock,
                  byte* outBlock) const
{
    word32 s0, s1, s2, s3, t0, t1, t2, t3;
    const word32 *rk = key_.get_buffer();

    /*
     * map byte array block to cipher state
     * and add initial round key:
     */
    gpBlock::Get(inBlock)(s0)(s1)(s2)(s3);
    s0 ^= rk[0];
    s1 ^= rk[1];
    s2 ^= rk[2];
    s3 ^= rk[3];
    /*
     * Nr - 1 full rounds:
     */
    unsigned int r = rounds_ >> 1;
    for (;;) {
        t0 =
            Te0[GETBYTE(s0, 3)] ^
            Te1[GETBYTE(s1, 2)] ^
            Te2[GETBYTE(s2, 1)] ^
            Te3[GETBYTE(s3, 0)] ^
            rk[4];
        t1 =
            Te0[GETBYTE(s1, 3)] ^
            Te1[GETBYTE(s2, 2)] ^
            Te2[GETBYTE(s3, 1)] ^
            Te3[GETBYTE(s0, 0)] ^
            rk[5];
        t2 =
            Te0[GETBYTE(s2, 3)] ^
            Te1[GETBYTE(s3, 2)] ^
            Te2[GETBYTE(s0, 1)] ^
            Te3[GETBYTE(s1, 0)] ^
            rk[6];
        t3 =
            Te0[GETBYTE(s3, 3)] ^
            Te1[GETBYTE(s0, 2)] ^
            Te2[GETBYTE(s1, 1)] ^
            Te3[GETBYTE(s2, 0)] ^
            rk[7];

        rk += 8;
        if (--r == 0) {
            break;
        }

        s0 =
            Te0[GETBYTE(t0, 3)] ^
            Te1[GETBYTE(t1, 2)] ^
            Te2[GETBYTE(t2, 1)] ^
            Te3[GETBYTE(t3, 0)] ^
            rk[0];
        s1 =
            Te0[GETBYTE(t1, 3)] ^
            Te1[GETBYTE(t2, 2)] ^
            Te2[GETBYTE(t3, 1)] ^
            Te3[GETBYTE(t0, 0)] ^
            rk[1];
        s2 =
            Te0[GETBYTE(t2, 3)] ^
            Te1[GETBYTE(t3, 2)] ^
            Te2[GETBYTE(t0, 1)] ^
            Te3[GETBYTE(t1, 0)] ^
            rk[2];
        s3 =
            Te0[GETBYTE(t3, 3)] ^
            Te1[GETBYTE(t0, 2)] ^
            Te2[GETBYTE(t1, 1)] ^
            Te3[GETBYTE(t2, 0)] ^
            rk[3];
    }
    /*
     * apply last round and
     * map cipher state to byte array block:
     */

    s0 =
        (Te4[GETBYTE(t0, 3)] & 0xff000000) ^
        (Te4[GETBYTE(t1, 2)] & 0x00ff0000) ^
        (Te4[GETBYTE(t2, 1)] & 0x0000ff00) ^
        (Te4[GETBYTE(t3, 0)] & 0x000000ff) ^
        rk[0];
    s1 =
        (Te4[GETBYTE(t1, 3)] & 0xff000000) ^
        (Te4[GETBYTE(t2, 2)] & 0x00ff0000) ^
        (Te4[GETBYTE(t3, 1)] & 0x0000ff00) ^
        (Te4[GETBYTE(t0, 0)] & 0x000000ff) ^
        rk[1];
    s2 =
        (Te4[GETBYTE(t2, 3)] & 0xff000000) ^
        (Te4[GETBYTE(t3, 2)] & 0x00ff0000) ^
        (Te4[GETBYTE(t0, 1)] & 0x0000ff00) ^
        (Te4[GETBYTE(t1, 0)] & 0x000000ff) ^
        rk[2];
    s3 =
        (Te4[GETBYTE(t3, 3)] & 0xff000000) ^
        (Te4[GETBYTE(t0, 2)] & 0x00ff0000) ^
        (Te4[GETBYTE(t1, 1)] & 0x0000ff00) ^
        (Te4[GETBYTE(t2, 0)] & 0x000000ff) ^
        rk[3];

    gpBlock::Put(xorBlock, outBlock)(s0)(s1)(s2)(s3);

}


void AES::decrypt(const byte* inBlock, const byte* xorBlock,
                  byte* outBlock) const
{
    word32 s0, s1, s2, s3, t0, t1, t2, t3;
    const word32* rk = key_.get_buffer();

    /*
     * map byte array block to cipher state
     * and add initial round key:
     */
    gpBlock::Get(inBlock)(s0)(s1)(s2)(s3);
    s0 ^= rk[0];
    s1 ^= rk[1];
    s2 ^= rk[2];
    s3 ^= rk[3];
    /*
     * Nr - 1 full rounds:
     */
    unsigned int r = rounds_ >> 1;
    for (;;) {
        t0 =
            Td0[GETBYTE(s0, 3)] ^
            Td1[GETBYTE(s3, 2)] ^
            Td2[GETBYTE(s2, 1)] ^
            Td3[GETBYTE(s1, 0)] ^
            rk[4];
        t1 =
            Td0[GETBYTE(s1, 3)] ^
            Td1[GETBYTE(s0, 2)] ^
            Td2[GETBYTE(s3, 1)] ^
            Td3[GETBYTE(s2, 0)] ^
            rk[5];
        t2 =
            Td0[GETBYTE(s2, 3)] ^
            Td1[GETBYTE(s1, 2)] ^
            Td2[GETBYTE(s0, 1)] ^
            Td3[GETBYTE(s3, 0)] ^
            rk[6];
        t3 =
            Td0[GETBYTE(s3, 3)] ^
            Td1[GETBYTE(s2, 2)] ^
            Td2[GETBYTE(s1, 1)] ^
            Td3[GETBYTE(s0, 0)] ^
            rk[7];

        rk += 8;
        if (--r == 0) {
            break;
        }

        s0 =
            Td0[GETBYTE(t0, 3)] ^
            Td1[GETBYTE(t3, 2)] ^
            Td2[GETBYTE(t2, 1)] ^
            Td3[GETBYTE(t1, 0)] ^
            rk[0];
        s1 =
            Td0[GETBYTE(t1, 3)] ^
            Td1[GETBYTE(t0, 2)] ^
            Td2[GETBYTE(t3, 1)] ^
            Td3[GETBYTE(t2, 0)] ^
            rk[1];
        s2 =
            Td0[GETBYTE(t2, 3)] ^
            Td1[GETBYTE(t1, 2)] ^
            Td2[GETBYTE(t0, 1)] ^
            Td3[GETBYTE(t3, 0)] ^
            rk[2];
        s3 =
            Td0[GETBYTE(t3, 3)] ^
            Td1[GETBYTE(t2, 2)] ^
            Td2[GETBYTE(t1, 1)] ^
            Td3[GETBYTE(t0, 0)] ^
            rk[3];
    }
    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Td4[GETBYTE(t0, 3)] & 0xff000000) ^
        (Td4[GETBYTE(t3, 2)] & 0x00ff0000) ^
        (Td4[GETBYTE(t2, 1)] & 0x0000ff00) ^
        (Td4[GETBYTE(t1, 0)] & 0x000000ff) ^
    rk[0];
    s1 =
        (Td4[GETBYTE(t1, 3)] & 0xff000000) ^
        (Td4[GETBYTE(t0, 2)] & 0x00ff0000) ^
        (Td4[GETBYTE(t3, 1)] & 0x0000ff00) ^
        (Td4[GETBYTE(t2, 0)] & 0x000000ff) ^
        rk[1];
    s2 =
        (Td4[GETBYTE(t2, 3)] & 0xff000000) ^
        (Td4[GETBYTE(t1, 2)] & 0x00ff0000) ^
        (Td4[GETBYTE(t0, 1)] & 0x0000ff00) ^
        (Td4[GETBYTE(t3, 0)] & 0x000000ff) ^
        rk[2];
    s3 =
        (Td4[GETBYTE(t3, 3)] & 0xff000000) ^
        (Td4[GETBYTE(t2, 2)] & 0x00ff0000) ^
        (Td4[GETBYTE(t1, 1)] & 0x0000ff00) ^
        (Td4[GETBYTE(t0, 0)] & 0x000000ff) ^
        rk[3];

    gpBlock::Put(xorBlock, outBlock)(s0)(s1)(s2)(s3);
}



} // namespace

