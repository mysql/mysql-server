/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
*/

/* based on Wei Dai's rsa.cpp from CryptoPP */

#include "runtime.hpp"
#include "rsa.hpp"
#include "asn.hpp"
#include "modarith.hpp"



namespace TaoCrypt {


Integer RSA_PublicKey::ApplyFunction(const Integer& x) const
{
    return a_exp_b_mod_c(x, e_, n_);
}


RSA_PublicKey::RSA_PublicKey(Source& source)
{
    Initialize(source);
}


void RSA_PublicKey::Initialize(Source& source)
{
    RSA_Public_Decoder decoder(source);
    decoder.Decode(*this);
}


Integer RSA_PrivateKey::CalculateInverse(RandomNumberGenerator& rng,
                                         const Integer& x) const
{
    ModularArithmetic modn(n_);

    Integer r(rng, Integer::One(), n_ - Integer::One());
    Integer re = modn.Exponentiate(r, e_);
    re = modn.Multiply(re, x);			// blind

    // here we follow the notation of PKCS #1 and let u=q inverse mod p
    // but in ModRoot, u=p inverse mod q, so we reverse the order of p and q

    Integer y = ModularRoot(re, dq_, dp_, q_, p_, u_);
    y = modn.Divide(y, r);				    // unblind
       
    return y;
}


RSA_PrivateKey::RSA_PrivateKey(Source& source)
{
    Initialize(source);
}


void RSA_PrivateKey::Initialize(Source& source)
{
    RSA_Private_Decoder decoder(source);
    decoder.Decode(*this);
}


void RSA_BlockType2::Pad(const byte *input, word32 inputLen, byte *pkcsBlock,
                         word32 pkcsBlockLen, RandomNumberGenerator& rng) const
{
    // convert from bit length to byte length
    if (pkcsBlockLen % 8 != 0)
    {
        pkcsBlock[0] = 0;
        pkcsBlock++;
    }
    pkcsBlockLen /= 8;

    pkcsBlock[0] = 2;  // block type 2

    // pad with non-zero random bytes
    word32 padLen = pkcsBlockLen - inputLen - 1;
    rng.GenerateBlock(&pkcsBlock[1], padLen);
    for (word32 i = 1; i < padLen; i++)
        if (pkcsBlock[i] == 0) pkcsBlock[i] = 0x01;
    
    pkcsBlock[pkcsBlockLen-inputLen-1] = 0;     // separator
    memcpy(pkcsBlock+pkcsBlockLen-inputLen, input, inputLen);
}

word32 RSA_BlockType2::UnPad(const byte *pkcsBlock, unsigned int pkcsBlockLen,
                           byte *output) const
{
    bool invalid = false;
    unsigned int maxOutputLen = SaturatingSubtract(pkcsBlockLen / 8, 10U);

    // convert from bit length to byte length
    if (pkcsBlockLen % 8 != 0)
    {
        invalid = (pkcsBlock[0] != 0) || invalid;
        pkcsBlock++;
    }
    pkcsBlockLen /= 8;

    // Require block type 2.
    invalid = (pkcsBlock[0] != 2) || invalid;

    // skip past the padding until we find the separator
    unsigned i=1;
    while (i<pkcsBlockLen && pkcsBlock[i++]) { // null body
        }
    if (!(i==pkcsBlockLen || pkcsBlock[i-1]==0))
        return 0;

    unsigned int outputLen = pkcsBlockLen - i;
    invalid = (outputLen > maxOutputLen) || invalid;

    if (invalid)
        return 0;

    memcpy (output, pkcsBlock+i, outputLen);
    return outputLen;
}


void RSA_BlockType1::Pad(const byte* input, word32 inputLen, byte* pkcsBlock,
                         word32 pkcsBlockLen, RandomNumberGenerator&) const
{
    // sanity checks
    if (input == NULL || pkcsBlock == NULL)
        return;

    // convert from bit length to byte length
    if (pkcsBlockLen % 8 != 0)
    {
        pkcsBlock[0] = 0;
        pkcsBlock++;
    }
    pkcsBlockLen /= 8;

    pkcsBlock[0] = 1;  // block type 1 for SSL

    // pad with 0xff bytes
    memset(&pkcsBlock[1], 0xFF, pkcsBlockLen - inputLen - 2);

    pkcsBlock[pkcsBlockLen-inputLen-1] = 0;     // separator
    memcpy(pkcsBlock+pkcsBlockLen-inputLen, input, inputLen);
}


word32 RSA_BlockType1::UnPad(const byte* pkcsBlock, word32 pkcsBlockLen,
                             byte* output) const
{
    bool invalid = false;
    unsigned int maxOutputLen = SaturatingSubtract(pkcsBlockLen / 8, 10U);

    // convert from bit length to byte length
    if (pkcsBlockLen % 8 != 0)
    {
        invalid = (pkcsBlock[0] != 0) || invalid;
        pkcsBlock++;
    }
    pkcsBlockLen /= 8;

    // Require block type 1 for SSL.
    invalid = (pkcsBlock[0] != 1) || invalid;

    // skip past the padding until we find the separator
    unsigned i=1;
    while (i<pkcsBlockLen && pkcsBlock[i++] == 0xFF) { // null body
        }
    if (!(i==pkcsBlockLen || pkcsBlock[i-1]==0))
        return 0;

    unsigned int outputLen = pkcsBlockLen - i;
    invalid = (outputLen > maxOutputLen) || invalid;

    if (invalid)
        return 0;

    memcpy(output, pkcsBlock+i, outputLen);
    return outputLen;
}


word32 SSL_Decrypt(const RSA_PublicKey& key, const byte* sig, byte* plain)
{
    PK_Lengths lengths(key.GetModulus());
   
    ByteBlock paddedBlock(BitsToBytes(lengths.PaddedBlockBitLength()));
    Integer x = key.ApplyFunction(Integer(sig,
                                          lengths.FixedCiphertextLength()));
    if (x.ByteCount() > paddedBlock.size())
        x = Integer::Zero();	
    x.Encode(paddedBlock.get_buffer(), paddedBlock.size());
    return RSA_BlockType1().UnPad(paddedBlock.get_buffer(),
                                  lengths.PaddedBlockBitLength(), plain);
}


} // namespace
