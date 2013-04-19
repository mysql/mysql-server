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

/* dh.hpp provides Diffie-Hellman support
*/


#ifndef TAO_CRYPT_DH_HPP
#define TAO_CRYPT_DH_HPP

#include "misc.hpp"
#include "integer.hpp"

namespace TaoCrypt {


class Source;


// Diffie-Hellman
class DH {
public:
    DH() {}
    DH(Integer& p, Integer& g) : p_(p), g_(g) {}
    explicit DH(Source&);

    DH(const DH& that) : p_(that.p_), g_(that.g_) {}
    DH& operator=(const DH& that) 
    {
        DH tmp(that);
        Swap(tmp);
        return *this;
    }

    void Swap(DH& other)
    {
        p_.Swap(other.p_);
        g_.Swap(other.g_);
    }

    void Initialize(Source&);
    void Initialize(Integer& p, Integer& g)
    {
        SetP(p);
        SetG(g);
    }

    void GenerateKeyPair(RandomNumberGenerator&, byte*, byte*);
    void Agree(byte*, const byte*, const byte*, word32 otherSz = 0);

    void SetP(const Integer& p) { p_ = p; }
    void SetG(const Integer& g) { g_ = g; }

    Integer& GetP() { return p_; }
    Integer& GetG() { return g_; }

    // for p and agree
    word32 GetByteLength() const { return p_.ByteCount(); }
private:
    // group parms
    Integer p_;
    Integer g_;

    void GeneratePrivate(RandomNumberGenerator&, byte*);
    void GeneratePublic(const byte*, byte*);    
};


} // namespace

#endif // TAO_CRYPT_DH_HPP
