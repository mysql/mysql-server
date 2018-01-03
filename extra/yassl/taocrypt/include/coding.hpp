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

/* coding.hpp defines hex and base64 encoding/decoing
*/

#ifndef TAO_CRYPT_CODING_HPP
#define TAO_CRYPT_CODING_HPP

#include "misc.hpp"
#include "block.hpp"

namespace TaoCrypt {

class Source;


// Hex Encoding, see RFC 3548
class HexEncoder {
    ByteBlock encoded_;
    Source&     plain_;
public:
    explicit HexEncoder(Source& s) : plain_(s) { Encode(); }
private:
    void Encode();

    HexEncoder(const HexEncoder&);              // hide copy
    HexEncoder& operator=(const HexEncoder&);   // and assign
};


// Hex Decoding, see RFC 3548
class HexDecoder {
    ByteBlock decoded_;
    Source&     coded_;
public:
    explicit HexDecoder(Source& s) : coded_(s) { Decode(); }
private:
    void Decode();

    HexDecoder(const HexDecoder&);              // hide copy
    HexDecoder& operator=(const HexDecoder&);   // and assign
};


// Base 64 encoding, see RFC 3548
class Base64Encoder {
    ByteBlock encoded_;
    Source&     plain_;
public:
    explicit Base64Encoder(Source& s) : plain_(s) { Encode(); }
private:
    void Encode();

    Base64Encoder(const Base64Encoder&);              // hide copy
    Base64Encoder& operator=(const Base64Encoder&);   // and assign
};


// Base 64 decoding, see RFC 3548
class Base64Decoder {
    ByteBlock decoded_;
    Source&     coded_;
public:
    explicit Base64Decoder(Source& s) : coded_(s) { Decode(); }
private:
    void Decode();

    Base64Decoder(const Base64Decoder&);              // hide copy
    Base64Decoder& operator=(const Base64Decoder&);   // and assign
};


}  // namespace

#endif // TAO_CRYPT_CODING_HPP
