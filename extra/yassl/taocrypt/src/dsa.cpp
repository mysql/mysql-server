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


#include "runtime.hpp"
#include "dsa.hpp"
#include "sha.hpp"
#include "asn.hpp"
#include "modarith.hpp"


namespace TaoCrypt {


void DSA_PublicKey::Swap(DSA_PublicKey& other)
{
    p_.Swap(other.p_);
    q_.Swap(other.q_);
    g_.Swap(other.g_);
    y_.Swap(other.y_);
}


DSA_PublicKey::DSA_PublicKey(const DSA_PublicKey& other)
    : p_(other.p_), q_(other.q_), g_(other.g_), y_(other.y_)
{}


DSA_PublicKey& DSA_PublicKey::operator=(const DSA_PublicKey& that)
{
    DSA_PublicKey tmp(that);
    Swap(tmp);
    return *this;
}


DSA_PublicKey::DSA_PublicKey(Source& source)
{
    Initialize(source);
}


void DSA_PublicKey::Initialize(Source& source)
{
    DSA_Public_Decoder decoder(source);
    decoder.Decode(*this);
}


void DSA_PublicKey::Initialize(const Integer& p, const Integer& q,
                               const Integer& g, const Integer& y)
{
    p_ = p;
    q_ = q;
    g_ = g;
    y_ = y;
}
   

const Integer& DSA_PublicKey::GetModulus() const
{
    return p_;
}

const Integer& DSA_PublicKey::GetSubGroupOrder() const
{
    return q_;
}


const Integer& DSA_PublicKey::GetSubGroupGenerator() const
{
    return g_;
}


const Integer& DSA_PublicKey::GetPublicPart() const
{
    return y_;
}


void DSA_PublicKey::SetModulus(const Integer& p)
{
    p_ = p;
}


void DSA_PublicKey::SetSubGroupOrder(const Integer& q)
{
    q_ = q;
}


void DSA_PublicKey::SetSubGroupGenerator(const Integer& g)
{
    g_ = g;
}


void DSA_PublicKey::SetPublicPart(const Integer& y)
{
    y_ = y;
}


word32 DSA_PublicKey::SignatureLength() const
{
    return GetSubGroupOrder().ByteCount() * 2;  // r and s
}



DSA_PrivateKey::DSA_PrivateKey(Source& source)
{
    Initialize(source);
}


void DSA_PrivateKey::Initialize(Source& source)
{
    DSA_Private_Decoder decoder(source);
    decoder.Decode(*this);
}


void DSA_PrivateKey::Initialize(const Integer& p, const Integer& q,
                                const Integer& g, const Integer& y,
                                const Integer& x)
{
    DSA_PublicKey::Initialize(p, q, g, y);
    x_ = x;
}


const Integer& DSA_PrivateKey::GetPrivatePart() const
{
    return x_;
}


void DSA_PrivateKey::SetPrivatePart(const Integer& x)
{
    x_ = x;
}


DSA_Signer::DSA_Signer(const DSA_PrivateKey& key)
    : key_(key)
{}


word32 DSA_Signer::Sign(const byte* sha_digest, byte* sig,
                        RandomNumberGenerator& rng)
{
    const Integer& p = key_.GetModulus();
    const Integer& q = key_.GetSubGroupOrder();
    const Integer& g = key_.GetSubGroupGenerator();
    const Integer& x = key_.GetPrivatePart();
    byte* tmpPtr = sig;  // initial signature output

    Integer k(rng, 1, q - 1);

    r_ =  a_exp_b_mod_c(g, k, p);
    r_ %= q;

    Integer H(sha_digest, SHA::DIGEST_SIZE);  // sha Hash(m)

    Integer kInv = k.InverseMod(q);
    s_ = (kInv * (H + x*r_)) % q;

    if (!(!!r_ && !!s_))
        return -1;

    int rSz = r_.ByteCount();
    int tmpSz = rSz;

    while (tmpSz++ < SHA::DIGEST_SIZE) {
        *sig++ = 0;
    }
    
    r_.Encode(sig,  rSz);

    sig = tmpPtr + SHA::DIGEST_SIZE;  // advance sig output to s
    int sSz = s_.ByteCount();
    tmpSz = sSz;

    while (tmpSz++ < SHA::DIGEST_SIZE) {
        *sig++ = 0;
    }

    s_.Encode(sig, sSz);

    return 40;
}


DSA_Verifier::DSA_Verifier(const DSA_PublicKey& key)
    : key_(key)
{}


bool DSA_Verifier::Verify(const byte* sha_digest, const byte* sig)
{
    const Integer& p = key_.GetModulus();
    const Integer& q = key_.GetSubGroupOrder();
    const Integer& g = key_.GetSubGroupGenerator();
    const Integer& y = key_.GetPublicPart();

    int sz = q.ByteCount();

    r_.Decode(sig, sz);
    s_.Decode(sig + sz, sz);

    if (r_ >= q || r_ < 1 || s_ >= q || s_ < 1)
        return false;

    Integer H(sha_digest, SHA::DIGEST_SIZE);  // sha Hash(m)

    Integer w = s_.InverseMod(q);
    Integer u1 = (H  * w) % q;
    Integer u2 = (r_ * w) % q;

    // verify r == ((g^u1 * y^u2) mod p) mod q
    ModularArithmetic ma(p);
    Integer v = ma.CascadeExponentiate(g, u1, y, u2);
    v %= q;

    return r_ == v;
}




const Integer& DSA_Signer::GetR() const
{
    return r_;
}


const Integer& DSA_Signer::GetS() const
{
    return s_;
}


const Integer& DSA_Verifier::GetR() const
{
    return r_;
}


const Integer& DSA_Verifier::GetS() const
{
    return s_;
}


} // namespace
