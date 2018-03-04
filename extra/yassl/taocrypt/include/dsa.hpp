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

/* dsa.hpp provides Digitial Signautre Algorithm see FIPS 186-2
*/

#ifndef TAO_CRYPT_DSA_HPP
#define TAO_CRYPT_DSA_HPP

#include "integer.hpp"


namespace TaoCrypt {

class Source;


class DSA_PublicKey {
protected:
    Integer p_;
    Integer q_;
    Integer g_;
    Integer y_;
public:
    DSA_PublicKey() {}
    explicit DSA_PublicKey(Source&);

    void Initialize(Source&);
    void Initialize(const Integer& p, const Integer& q, const Integer& g,
                    const Integer& y);
    
    const Integer& GetModulus() const;
    const Integer& GetSubGroupOrder() const;
    const Integer& GetSubGroupGenerator() const;
    const Integer& GetPublicPart() const;

    void SetModulus(const Integer&);
    void SetSubGroupOrder(const Integer&);
    void SetSubGroupGenerator(const Integer&);
    void SetPublicPart(const Integer&);

    word32 SignatureLength() const;
 
    DSA_PublicKey(const DSA_PublicKey&);
    DSA_PublicKey& operator=(const DSA_PublicKey&);

    void Swap(DSA_PublicKey& other);
};



class DSA_PrivateKey : public DSA_PublicKey {
    Integer x_;
public:
    DSA_PrivateKey() {}
    explicit DSA_PrivateKey(Source&);

    void Initialize(Source&);
    void Initialize(const Integer& p, const Integer& q, const Integer& g,
                    const Integer& y, const Integer& x);
    
    const Integer& GetPrivatePart() const;

    void SetPrivatePart(const Integer&);
private:
    DSA_PrivateKey(const DSA_PrivateKey&);            // hide copy
    DSA_PrivateKey& operator=(const DSA_PrivateKey&); // and assign
};



class DSA_Signer {
    const DSA_PrivateKey& key_;
    Integer               r_;
    Integer               s_;
public:
    explicit DSA_Signer(const DSA_PrivateKey&);

    word32 Sign(const byte* sha_digest, byte* sig, RandomNumberGenerator&);

    const Integer& GetR() const;
    const Integer& GetS() const;
private:
    DSA_Signer(const DSA_Signer&);      // hide copy
    DSA_Signer& operator=(DSA_Signer&); // and assign
};


class DSA_Verifier {
    const DSA_PublicKey& key_;
    Integer              r_;
    Integer              s_;
public:
    explicit DSA_Verifier(const DSA_PublicKey&);

    bool Verify(const byte* sha_digest, const byte* sig);

    const Integer& GetR() const;
    const Integer& GetS() const;
private:
    DSA_Verifier(const DSA_Verifier&);              // hide copy
    DSA_Verifier& operator=(const DSA_Verifier&);   // and assign
};





} // namespace

#endif // TAO_CRYPT_DSA_HPP
