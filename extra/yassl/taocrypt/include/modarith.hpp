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


/* based on Wei Dai's modarith.h from CryptoPP */


#ifndef TAO_CRYPT_MODARITH_HPP
#define TAO_CRYPT_MODARITH_HPP

#include "misc.hpp"
#include "algebra.hpp"

namespace TaoCrypt {


// ModularArithmetic
class ModularArithmetic : public AbstractRing
{
public:

    typedef int RandomizationParameter;
    typedef Integer Element;

    ModularArithmetic(const Integer &modulus = Integer::One())
        : modulus(modulus), result((word)0, modulus.reg_.size()) {}

    ModularArithmetic(const ModularArithmetic &ma)
        : AbstractRing(),
        modulus(ma.modulus), result((word)0, modulus.reg_.size()) {}

    const Integer& GetModulus() const {return modulus;}
    void SetModulus(const Integer &newModulus) 
    {   
        modulus = newModulus;
        result.reg_.resize(modulus.reg_.size());
    }

    virtual bool IsMontgomeryRepresentation() const {return false;}

    virtual Integer ConvertIn(const Integer &a) const
        {return a%modulus;}

    virtual Integer ConvertOut(const Integer &a) const
        {return a;}

    const Integer& Half(const Integer &a) const;

    bool Equal(const Integer &a, const Integer &b) const
        {return a==b;}

    const Integer& Identity() const
        {return Integer::Zero();}

    const Integer& Add(const Integer &a, const Integer &b) const;

    Integer& Accumulate(Integer &a, const Integer &b) const;

    const Integer& Inverse(const Integer &a) const;

    const Integer& Subtract(const Integer &a, const Integer &b) const;

    Integer& Reduce(Integer &a, const Integer &b) const;

    const Integer& Double(const Integer &a) const
        {return Add(a, a);}

    const Integer& MultiplicativeIdentity() const
        {return Integer::One();}

    const Integer& Multiply(const Integer &a, const Integer &b) const
        {return result1 = a*b%modulus;}

    const Integer& Square(const Integer &a) const
        {return result1 = a.Squared()%modulus;}

    bool IsUnit(const Integer &a) const
        {return Integer::Gcd(a, modulus).IsUnit();}

    const Integer& MultiplicativeInverse(const Integer &a) const
        {return result1 = a.InverseMod(modulus);}

    const Integer& Divide(const Integer &a, const Integer &b) const
        {return Multiply(a, MultiplicativeInverse(b));}

    Integer CascadeExponentiate(const Integer &x, const Integer &e1,
                                const Integer &y, const Integer &e2) const;

    void SimultaneousExponentiate(Element *results, const Element &base,
                  const Integer *exponents, unsigned int exponentsCount) const;

    unsigned int MaxElementBitLength() const
        {return (modulus-1).BitCount();}

    unsigned int MaxElementByteLength() const
        {return (modulus-1).ByteCount();}


    static const RandomizationParameter DefaultRandomizationParameter;

protected:
    Integer modulus;
    mutable Integer result, result1;

};



//! do modular arithmetics in Montgomery representation for increased speed
class MontgomeryRepresentation : public ModularArithmetic
{
public:
    MontgomeryRepresentation(const Integer &modulus);	// modulus must be odd

    bool IsMontgomeryRepresentation() const {return true;}

    Integer ConvertIn(const Integer &a) const
        {return (a<<(WORD_BITS*modulus.reg_.size()))%modulus;}

    Integer ConvertOut(const Integer &a) const;

    const Integer& MultiplicativeIdentity() const
     {return result1 = Integer::Power2(WORD_BITS*modulus.reg_.size())%modulus;}

    const Integer& Multiply(const Integer &a, const Integer &b) const;

    const Integer& Square(const Integer &a) const;

    const Integer& MultiplicativeInverse(const Integer &a) const;

    Integer CascadeExponentiate(const Integer &x, const Integer &e1,
                                const Integer &y, const Integer &e2) const
        {return AbstractRing::CascadeExponentiate(x, e1, y, e2);}

    void SimultaneousExponentiate(Element *results, const Element &base,
            const Integer *exponents, unsigned int exponentsCount) const
        {AbstractRing::SimultaneousExponentiate(results, base,
                                                exponents, exponentsCount);}

private:
    Integer u;
    mutable AlignedWordBlock workspace;
};




} // namespace

#endif // TAO_CRYPT_MODARITH_HPP
