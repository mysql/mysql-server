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

/* The handshake header declares function prototypes for creating and reading
 * the various handshake messages.
 */



#ifndef yaSSL_HANDSHAKE_HPP
#define yaSSL_HANDSHAKE_HPP

#include "yassl_types.hpp"


namespace yaSSL {

// forward decls
class  SSL;
class  Finished;
class  Data;
class  Alert;
struct Hashes;

enum BufferOutput { buffered, unbuffered };

void sendClientHello(SSL&);
void sendServerHello(SSL&, BufferOutput = buffered);
void sendServerHelloDone(SSL&, BufferOutput = buffered);
void sendClientKeyExchange(SSL&, BufferOutput = buffered);
void sendServerKeyExchange(SSL&, BufferOutput = buffered);
void sendChangeCipher(SSL&, BufferOutput = buffered);
void sendFinished(SSL&, ConnectionEnd, BufferOutput = buffered);
void sendCertificate(SSL&, BufferOutput = buffered);
void sendCertificateRequest(SSL&, BufferOutput = buffered);
void sendCertificateVerify(SSL&, BufferOutput = buffered);
int  sendData(SSL&, const void*, int);
int  sendAlert(SSL& ssl, const Alert& alert);

int  receiveData(SSL&, Data&, bool peek = false); 
void processReply(SSL&);

void buildFinished(SSL&, Finished&, const opaque*);
void build_certHashes(SSL&, Hashes&);

void hmac(SSL&, byte*, const byte*, uint, ContentType, bool verify = false);
void TLS_hmac(SSL&, byte*, const byte*, uint, ContentType,
              bool verify = false);
void PRF(byte* digest, uint digLen, const byte* secret, uint secLen,
         const byte* label, uint labLen, const byte* seed, uint seedLen);

} // naemspace

#endif // yaSSL_HANDSHAKE_HPP
