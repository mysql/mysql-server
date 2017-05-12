/*
   Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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

/* asn.cpp implements ASN1 BER, PublicKey, and x509v3 decoding 
*/

#include "runtime.hpp"
#include "asn.hpp"
#include "file.hpp"
#include "integer.hpp"
#include "rsa.hpp"
#include "dsa.hpp"
#include "dh.hpp"
#include "md5.hpp"
#include "md2.hpp"
#include "sha.hpp"
#include "coding.hpp"
#include <time.h>     // gmtime();
#include "memory.hpp" // some auto_ptr don't have reset, also need auto_array


namespace TaoCrypt {

// like atoi but only use first byte
word32 btoi(byte b)
{
    return b - 0x30;
}


// two byte date/time, add to value
void GetTime(int *value, const byte* date, int& i)
{
    *value += btoi(date[i++]) * 10;
    *value += btoi(date[i++]);
}


bool ASN1_TIME_extract(const unsigned char* date, unsigned char format,
                       tm *t)
{
  int i = 0;
  memset(t, 0, sizeof (tm));

  if (format != UTC_TIME && format != GENERALIZED_TIME)
    return false;

  if (format == UTC_TIME) {
    if (btoi(date[0]) >= 5)
      t->tm_year = 1900;
    else
      t->tm_year = 2000;
  }
  else  { // format == GENERALIZED_TIME
    t->tm_year += btoi(date[i++]) * 1000;
    t->tm_year += btoi(date[i++]) * 100;
  }

  GetTime(&t->tm_year, date, i);     t->tm_year -= 1900; // adjust
  GetTime(&t->tm_mon,  date, i);     t->tm_mon  -= 1;    // adjust
  GetTime(&t->tm_mday, date, i);
  GetTime(&t->tm_hour, date, i);
  GetTime(&t->tm_min,  date, i);
  GetTime(&t->tm_sec,  date, i);

  if (date[i] != 'Z')     // only Zulu supported for this profile
    return false;
  return true;
}


namespace { // locals


// to the second
bool operator>(tm& a, tm& b)
{
    if (a.tm_year > b.tm_year)
        return true;

    if (a.tm_year == b.tm_year && a.tm_mon > b.tm_mon)
        return true;
    
    if (a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday >b.tm_mday)
        return true;

    if (a.tm_year == b.tm_year && a.tm_mon == b.tm_mon &&
        a.tm_mday == b.tm_mday && a.tm_hour > b.tm_hour)
        return true;

    if (a.tm_year == b.tm_year && a.tm_mon == b.tm_mon &&
        a.tm_mday == b.tm_mday && a.tm_hour == b.tm_hour &&
        a.tm_min > b.tm_min)
        return true;

    if (a.tm_year == b.tm_year && a.tm_mon == b.tm_mon &&
        a.tm_mday == b.tm_mday && a.tm_hour == b.tm_hour &&
        a.tm_min  == b.tm_min  && a.tm_sec > b.tm_sec)
        return true;

    return false;
}


bool operator<(tm& a, tm&b)
{
    return (b>a);
}


// Make sure before and after dates are valid
bool ValidateDate(const byte* date, byte format, CertDecoder::DateType dt)
{
    tm certTime;

    if (!ASN1_TIME_extract(date, format, &certTime))
        return false;

    time_t ltime = time(0);
    tm* localTime = gmtime(&ltime);

    if (dt == CertDecoder::BEFORE) {
        if (*localTime < certTime)
            return false;
    }
    else
        if (*localTime > certTime)
            return false;

    return true;
}


class BadCertificate {};

} // local namespace



// used by Integer as well
word32 GetLength(Source& source)
{
    word32 length = 0;

    byte b = source.next();
    if (b >= LONG_LENGTH) {        
        word32 bytes = b & 0x7F;

        if (source.IsLeft(bytes) == false) return 0;

        while (bytes--) {
            b = source.next();
            length = (length << 8) | b;
        }
    }
    else
        length = b;

    if (source.IsLeft(length) == false) return 0;

    return length;
}


word32 SetLength(word32 length, byte* output)
{
    word32 i = 0;

    if (length < LONG_LENGTH)
        output[i++] = length;
    else {
        output[i++] = BytePrecision(length) | 0x80;
      
        for (int j = BytePrecision(length); j; --j) {
            output[i] = length >> (j - 1) * 8;
            i++;
        }
    }
    return i;
}


PublicKey::PublicKey(const byte* k, word32 s) : key_(0), sz_(0)
{
    if (s) {
        SetSize(s);
        SetKey(k);
    }
}


void PublicKey::SetSize(word32 s)
{
    sz_ = s;
    key_ = NEW_TC byte[sz_];
}


void PublicKey::SetKey(const byte* k)
{
    memcpy(key_, k, sz_);
}


void PublicKey::AddToEnd(const byte* data, word32 len)
{
    mySTL::auto_array<byte> tmp(NEW_TC byte[sz_ + len]);

    memcpy(tmp.get(), key_, sz_);
    memcpy(tmp.get() + sz_, data, len);

    byte* del = 0;
    STL::swap(del, key_);
    tcArrayDelete(del);

    key_ = tmp.release();
    sz_ += len;
}


Signer::Signer(const byte* k, word32 kSz, const char* n, const byte* h)
    : key_(k, kSz)
{
    size_t sz = strlen(n);
    memcpy(name_, n, sz);
    name_[sz] = 0;

    memcpy(hash_, h, SHA::DIGEST_SIZE);
}

Signer::~Signer()
{
}


Error BER_Decoder::GetError()
{ 
    return source_.GetError(); 
}


Integer& BER_Decoder::GetInteger(Integer& integer)
{
    if (!source_.GetError().What())
        integer.Decode(source_);
    return integer;
}

  
// Read a Sequence, return length
word32 BER_Decoder::GetSequence()
{
    if (source_.GetError().What()) return 0;

    byte b = source_.next();
    if (b != (SEQUENCE | CONSTRUCTED)) {
        source_.SetError(SEQUENCE_E);
        return 0;
    }

    return GetLength(source_);
}


// Read a Sequence, return length
word32 BER_Decoder::GetSet()
{
    if (source_.GetError().What()) return 0;

    byte b = source_.next();
    if (b != (SET | CONSTRUCTED)) {
        source_.SetError(SET_E);
        return 0;
    }

    return GetLength(source_);
}


// Read Version, return it
word32 BER_Decoder::GetVersion()
{
    if (source_.GetError().What()) return 0;

    byte b = source_.next();
    if (b != INTEGER) {
        source_.SetError(INTEGER_E);
        return 0;
    }

    b = source_.next();
    if (b != 0x01) {
        source_.SetError(VERSION_E);
        return 0;
    }

    return source_.next();
}


// Read ExplicitVersion, return it or 0 if not there (not an error)
word32 BER_Decoder::GetExplicitVersion()
{
    if (source_.GetError().What()) return 0;

    byte b = source_.next();

    if (b == (CONTEXT_SPECIFIC | CONSTRUCTED)) { // not an error if not here
        source_.next();
        return GetVersion();
    }
    else 
        source_.prev(); // put back
  
    return 0;
}


// Decode a BER encoded RSA Private Key
void RSA_Private_Decoder::Decode(RSA_PrivateKey& key)
{
    ReadHeader();
    if (source_.GetError().What()) return;
    // public
    key.SetModulus(GetInteger(Integer().Ref()));
    key.SetPublicExponent(GetInteger(Integer().Ref()));

    // private
    key.SetPrivateExponent(GetInteger(Integer().Ref()));
    key.SetPrime1(GetInteger(Integer().Ref()));
    key.SetPrime2(GetInteger(Integer().Ref()));
    key.SetModPrime1PrivateExponent(GetInteger(Integer().Ref()));
    key.SetModPrime2PrivateExponent(GetInteger(Integer().Ref()));
    key.SetMultiplicativeInverseOfPrime2ModPrime1(GetInteger(Integer().Ref()));
}


void RSA_Private_Decoder::ReadHeader()
{
    GetSequence();
    GetVersion();
}


// Decode a BER encoded DSA Private Key
void DSA_Private_Decoder::Decode(DSA_PrivateKey& key)
{
    ReadHeader();
    if (source_.GetError().What()) return;
    // group parameters
    key.SetModulus(GetInteger(Integer().Ref()));
    key.SetSubGroupOrder(GetInteger(Integer().Ref()));
    key.SetSubGroupGenerator(GetInteger(Integer().Ref()));

    // key
    key.SetPublicPart(GetInteger(Integer().Ref()));
    key.SetPrivatePart(GetInteger(Integer().Ref()));   
}


void DSA_Private_Decoder::ReadHeader()
{
    GetSequence();
    GetVersion();
}


// Decode a BER encoded RSA Public Key
void RSA_Public_Decoder::Decode(RSA_PublicKey& key)
{
    ReadHeader();
    if (source_.GetError().What()) return;

    ReadHeaderOpenSSL();   // may or may not be
    if (source_.GetError().What()) return;

    // public key
    key.SetModulus(GetInteger(Integer().Ref()));
    key.SetPublicExponent(GetInteger(Integer().Ref()));
}


// Read OpenSSL format public header
void RSA_Public_Decoder::ReadHeaderOpenSSL()
{
    byte b = source_.next();  // peek
    source_.prev();

    if (b != INTEGER) { // have OpenSSL public format
        GetSequence();
        b = source_.next();
        if (b != OBJECT_IDENTIFIER) {
            source_.SetError(OBJECT_ID_E);
            return;
        }

        word32 len = GetLength(source_);
        source_.advance(len);

        b = source_.next();
        if (b == TAG_NULL) {   // could have NULL tag and 0 terminator, may not 
            b = source_.next();
            if (b != 0) {
                source_.SetError(EXPECT_0_E);
                return; 
            }
        }
        else
            source_.prev();   // put back

        b = source_.next();
        if (b != BIT_STRING) {   
            source_.SetError(BIT_STR_E);
            return; 
        }

        len = GetLength(source_); 
        b = source_.next();
        if (b != 0)           // could have 0
            source_.prev();   // put back
        
        GetSequence();
    }
}


void RSA_Public_Decoder::ReadHeader()
{
    GetSequence();
}


// Decode a BER encoded DSA Public Key
void DSA_Public_Decoder::Decode(DSA_PublicKey& key)
{
    ReadHeader();
    if (source_.GetError().What()) return;

    // group parameters
    key.SetModulus(GetInteger(Integer().Ref()));
    key.SetSubGroupOrder(GetInteger(Integer().Ref()));
    key.SetSubGroupGenerator(GetInteger(Integer().Ref()));

    // key
    key.SetPublicPart(GetInteger(Integer().Ref()));
}


void DSA_Public_Decoder::ReadHeader()
{
    GetSequence();
}


void DH_Decoder::ReadHeader()
{
    GetSequence();
}


// Decode a BER encoded Diffie-Hellman Key
void DH_Decoder::Decode(DH& key)
{
    ReadHeader();
    if (source_.GetError().What()) return;

    // group parms
    key.SetP(GetInteger(Integer().Ref()));
    key.SetG(GetInteger(Integer().Ref()));
}


CertDecoder::CertDecoder(Source& s, bool decode, SignerList* signers,
                         bool noVerify, CertType ct)
    : BER_Decoder(s), certBegin_(0), sigIndex_(0), sigLength_(0), subCnPos_(-1),
      subCnLen_(0), issCnPos_(-1), issCnLen_(0), signature_(0),
      verify_(!noVerify)
{
    issuer_[0] = 0;
    subject_[0] = 0;

    if (decode)
        Decode(signers, ct);

}


CertDecoder::~CertDecoder()
{
    tcArrayDelete(signature_);
}


// process certificate header, set signature offset
void CertDecoder::ReadHeader()
{
    if (source_.GetError().What()) return;

    GetSequence();  // total
    certBegin_ = source_.get_index();

    sigIndex_ = GetSequence();  // this cert
    sigIndex_ += source_.get_index();

    GetExplicitVersion(); // version
    GetInteger(Integer().Ref());  // serial number
}


// Decode a x509v3 Certificate
void CertDecoder::Decode(SignerList* signers, CertType ct)
{
    if (source_.GetError().What()) return;
    DecodeToKey();
    if (source_.GetError().What()) return;

    if (source_.get_index() != sigIndex_)
        source_.set_index(sigIndex_);

    word32 confirmOID = GetAlgoId();
    GetSignature();
    if (source_.GetError().What()) return;

    if ( confirmOID != signatureOID_ ) {
        source_.SetError(SIG_OID_E);
        return;
    }
    
    if (ct != CA && verify_ && !ValidateSignature(signers))
        source_.SetError(SIG_OTHER_E);
}


void CertDecoder::DecodeToKey()
{
    ReadHeader();
    signatureOID_ = GetAlgoId();
    GetName(ISSUER);   
    GetValidity();
    GetName(SUBJECT);   
    GetKey();
}


// Read public key
void CertDecoder::GetKey()
{
    if (source_.GetError().What()) return;

    GetSequence();    
    keyOID_ = GetAlgoId();

    if (keyOID_ == RSAk) {
        byte b = source_.next();
        if (b != BIT_STRING) {
            source_.SetError(BIT_STR_E);
            return;
        }
        b = source_.next();      // length, future
        b = source_.next(); 
        while(b != 0)
            b = source_.next();
    }
    else if (keyOID_ == DSAk)
        ;   // do nothing
    else {
        source_.SetError(UNKNOWN_OID_E);
        return;
    }

    StoreKey();
    if (keyOID_ == DSAk)
        AddDSA();
}


// Save public key
void CertDecoder::StoreKey()
{
    if (source_.GetError().What()) return;

    word32 read = source_.get_index();
    word32 length = GetSequence();

    read = source_.get_index() - read;
    length += read;

    if (source_.GetError().What()) return;
    while (read--) source_.prev();

    if (source_.IsLeft(length) == false) return;
    key_.SetSize(length);
    key_.SetKey(source_.get_current());
    source_.advance(length);
}


// DSA has public key after group
void CertDecoder::AddDSA()
{
    if (source_.GetError().What()) return;

    byte b = source_.next();
    if (b != BIT_STRING) {
        source_.SetError(BIT_STR_E);
        return;
    }
    b = source_.next();      // length, future
    b = source_.next(); 
    while(b != 0)
        b = source_.next();

    word32 idx = source_.get_index();
    b = source_.next();
    if (b != INTEGER) {
        source_.SetError(INTEGER_E);
        return;
    }

    word32 length = GetLength(source_);
    length += source_.get_index() - idx;

    if (source_.IsLeft(length) == false) return;

    key_.AddToEnd(source_.get_buffer() + idx, length);    
}


// process algo OID by summing, return it
word32 CertDecoder::GetAlgoId()
{
    if (source_.GetError().What()) return 0;
    word32 length = GetSequence();

    if (source_.GetError().What()) return 0;
    
    byte b = source_.next();
    if (b != OBJECT_IDENTIFIER) {
        source_.SetError(OBJECT_ID_E);
        return 0;
    }

    length = GetLength(source_);
    if (source_.IsLeft(length) == false) return 0;

    word32 oid = 0;
    while(length--)
        oid += source_.next();        // just sum it up for now

    // could have NULL tag and 0 terminator, but may not
    b = source_.next();
    if (b == TAG_NULL) {
        b = source_.next();
        if (b != 0) {
            source_.SetError(EXPECT_0_E);
            return 0;
        }
    }
    else
        // go back, didn't have it
        b = source_.prev();

    return oid;
}


// read cert signature, store in signature_
word32 CertDecoder::GetSignature()
{
    if (source_.GetError().What()) return 0;
    byte b = source_.next();

    if (b != BIT_STRING) {
        source_.SetError(BIT_STR_E);
        return 0;
    }

    sigLength_ = GetLength(source_);
    if (sigLength_ <= 1 || source_.IsLeft(sigLength_) == false) {
        source_.SetError(CONTENT_E);
        return 0;
    }
  
    b = source_.next();
    if (b != 0) {
        source_.SetError(EXPECT_0_E);
        return 0;
    }
    sigLength_--;

    signature_ = NEW_TC byte[sigLength_];
    memcpy(signature_, source_.get_current(), sigLength_);
    source_.advance(sigLength_);

    return sigLength_;
}


// read cert digest, store in signature_
word32 CertDecoder::GetDigest()
{
    if (source_.GetError().What()) return 0;
    byte b = source_.next();

    if (b != OCTET_STRING) {
        source_.SetError(OCTET_STR_E);
        return 0;
    }

    sigLength_ = GetLength(source_);

    signature_ = NEW_TC byte[sigLength_];
    memcpy(signature_, source_.get_current(), sigLength_);
    source_.advance(sigLength_);

    return sigLength_;
}


// memory length checked add tag to buffer
char* CertDecoder::AddTag(char* ptr, const char* buf_end, const char* tag_name,
                          word32 tag_name_length, word32 tag_value_length)
{
    if (ptr + tag_name_length + tag_value_length > buf_end) {
        source_.SetError(CONTENT_E);
        return 0;
    }

    memcpy(ptr, tag_name, tag_name_length);
    ptr += tag_name_length;

    memcpy(ptr, source_.get_current(), tag_value_length);
    ptr += tag_value_length;

    return ptr;
}


// process NAME, either issuer or subject
void CertDecoder::GetName(NameType nt)
{
    if (source_.GetError().What()) return;

    SHA    sha;
    word32 length = GetSequence();  // length of all distinguished names

    if (length >= ASN_NAME_MAX)
        return;
    if (source_.IsLeft(length) == false) return;
    length += source_.get_index();
    
    char* ptr;
    char* buf_end;

    if (nt == ISSUER) {
        ptr = issuer_;
        buf_end = ptr + sizeof(issuer_) - 1;   // 1 byte for trailing 0
    }
    else {
        ptr = subject_;
        buf_end = ptr + sizeof(subject_) - 1;  // 1 byte for trailing 0
    }

    while (source_.get_index() < length) {
        GetSet();
        if (source_.GetError().What() == SET_E) {
            source_.SetError(NO_ERROR_E);  // extensions may only have sequence 
            source_.prev();
        }
        GetSequence();

        byte b = source_.next();
        if (b != OBJECT_IDENTIFIER) {
            source_.SetError(OBJECT_ID_E);
            return;
        }

        word32 oidSz = GetLength(source_);
        if (source_.IsLeft(oidSz) == false) return;

        byte joint[2];
        if (source_.IsLeft(sizeof(joint)) == false) return;
        memcpy(joint, source_.get_current(), sizeof(joint));

        // v1 name types
        if (joint[0] == 0x55 && joint[1] == 0x04) {
            source_.advance(2);
            byte   id      = source_.next();  
            b              = source_.next();    // strType
            word32 strLen  = GetLength(source_);

            if (source_.IsLeft(strLen) == false) return;

            switch (id) {
            case COMMON_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/CN=", 4, strLen)))
                    return;
                if (nt == ISSUER) {
                    issCnPos_ = (int)(ptr - strLen - issuer_);
                    issCnLen_ = (int)strLen;
                } else {
                    subCnPos_ = (int)(ptr - strLen - subject_);
                    subCnLen_ = (int)strLen;
                }
                break;
            case SUR_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/SN=", 4, strLen)))
                    return;
                break;
            case COUNTRY_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/C=", 3, strLen)))
                    return;
                break;
            case LOCALITY_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/L=", 3, strLen)))
                    return;
                break;
            case STATE_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/ST=", 4, strLen)))
                    return;
                break;
            case ORG_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/O=", 3, strLen)))
                    return;
                break;
            case ORGUNIT_NAME:
                if (!(ptr = AddTag(ptr, buf_end, "/OU=", 4, strLen)))
                    return;
                break;
            }

            sha.Update(source_.get_current(), strLen);
            source_.advance(strLen);
        }
        else { 
            bool email = false;
            if (joint[0] == 0x2a && joint[1] == 0x86)  // email id hdr
                email = true;

            source_.advance(oidSz + 1);
            word32 length = GetLength(source_);
            if (source_.IsLeft(length) == false) return;

            if (email) {
                if (!(ptr = AddTag(ptr, buf_end, "/emailAddress=", 14, length)))
                    return; 
            }

            source_.advance(length);
        }
    }

    *ptr = 0;

    if (nt == ISSUER)
        sha.Final(issuerHash_);
    else
        sha.Final(subjectHash_);
}


// process a Date, either BEFORE or AFTER
void CertDecoder::GetDate(DateType dt)
{
    if (source_.GetError().What()) return;

    byte b = source_.next();
    if (b != UTC_TIME && b != GENERALIZED_TIME) {
        source_.SetError(TIME_E);
        return;
    }

    word32 length = GetLength(source_);
    if (source_.IsLeft(length) == false) return;

    byte date[MAX_DATE_SZ];
    if (length > MAX_DATE_SZ || length < MIN_DATE_SZ) {
        source_.SetError(DATE_SZ_E);
        return;
    }

    memcpy(date, source_.get_current(), length);
    source_.advance(length);

    if (!ValidateDate(date, b, dt) && verify_) {
        if (dt == BEFORE)
            source_.SetError(BEFORE_DATE_E);
        else
            source_.SetError(AFTER_DATE_E);
    }

    // save for later use
    if (dt == BEFORE) {
        memcpy(beforeDate_, date, length);
        beforeDate_[length] = 0;
        beforeDateType_= b;
    }
    else {  // after
        memcpy(afterDate_, date, length);
        afterDate_[length] = 0;
        afterDateType_= b;
    }       
}


void CertDecoder::GetValidity()
{
    if (source_.GetError().What()) return;

    GetSequence();
    GetDate(BEFORE);
    GetDate(AFTER);
}


bool CertDecoder::ValidateSelfSignature()
{
    Source pub(key_.GetKey(), key_.size());
    return ConfirmSignature(pub);
}


// extract compare signature hash from plain and place into digest
void CertDecoder::GetCompareHash(const byte* plain, word32 sz, byte* digest,
                                 word32 digSz)
{
    if (source_.GetError().What()) return;

    Source s(plain, sz);
    CertDecoder dec(s, false);

    dec.GetSequence();
    dec.GetAlgoId();
    dec.GetDigest();

    if (dec.sigLength_ > digSz) {
        source_.SetError(SIG_LEN_E);
        return;
    }

    memcpy(digest, dec.signature_, dec.sigLength_);
}


// validate signature signed by someone else
bool CertDecoder::ValidateSignature(SignerList* signers)
{
    if (!signers)
        return false;

    SignerList::iterator first = signers->begin();
    SignerList::iterator last  = signers->end();

    while (first != last) {
        if ( memcmp(issuerHash_, (*first)->GetHash(), SHA::DIGEST_SIZE) == 0) {
      
            const PublicKey& iKey = (*first)->GetPublicKey();
            Source pub(iKey.GetKey(), iKey.size());
            return ConfirmSignature(pub);
        }   
        ++first;
    }
    return false;
}


// confirm certificate signature
bool CertDecoder::ConfirmSignature(Source& pub)
{
    HashType ht;
    mySTL::auto_ptr<HASH> hasher;

    if (signatureOID_ == MD5wRSA) {
        hasher.reset(NEW_TC MD5);
        ht = MD5h;
    }
    else if (signatureOID_ == MD2wRSA) {
        hasher.reset(NEW_TC MD2);
        ht = MD2h;
    }
    else if (signatureOID_ == SHAwRSA || signatureOID_ == SHAwDSA) {
        hasher.reset(NEW_TC SHA);
        ht = SHAh;
    }
    else if (signatureOID_ == SHA256wRSA || signatureOID_ == SHA256wDSA) {
        hasher.reset(NEW_TC SHA256);
        ht = SHA256h;
    }
#ifdef WORD64_AVAILABLE
    else if (signatureOID_ == SHA384wRSA) {
        hasher.reset(NEW_TC SHA384);
        ht = SHA384h;
    }
    else if (signatureOID_ == SHA512wRSA) {
        hasher.reset(NEW_TC SHA512);
        ht = SHA512h;
    }
#endif
    else {
        source_.SetError(UNKOWN_SIG_E);
        return false;
    }

    byte digest[MAX_SHA2_DIGEST_SIZE];      // largest size

    hasher->Update(source_.get_buffer() + certBegin_, sigIndex_ - certBegin_);
    hasher->Final(digest);

    if (keyOID_ == RSAk) {
        // put in ASN.1 signature format
        Source build;
        Signature_Encoder(digest, hasher->getDigestSize(), ht, build);

        RSA_PublicKey pubKey(pub);
        RSAES_Encryptor enc(pubKey);

        if (pubKey.FixedCiphertextLength() != sigLength_) {
            source_.SetError(SIG_LEN_E);
            return false;
        }

        return enc.SSL_Verify(build.get_buffer(), build.size(), signature_);
    }
    else  { // DSA
        // extract r and s from sequence
        byte seqDecoded[DSA_SIG_SZ];
        memset(seqDecoded, 0, sizeof(seqDecoded));
        DecodeDSA_Signature(seqDecoded, signature_, sigLength_);

        DSA_PublicKey pubKey(pub);
        DSA_Verifier  ver(pubKey);

        return ver.Verify(digest, seqDecoded);
    }
}


Signature_Encoder::Signature_Encoder(const byte* dig, word32 digSz,
                                     HashType digOID, Source& source)
{
    // build bottom up

    // Digest
    byte digArray[MAX_DIGEST_SZ];
    word32 digestSz = SetDigest(dig, digSz, digArray);

    // AlgoID
    byte algoArray[MAX_ALGO_SZ];
    word32 algoSz = SetAlgoID(digOID, algoArray);

    // Sequence
    byte seqArray[MAX_SEQ_SZ];
    word32 seqSz = SetSequence(digestSz + algoSz, seqArray);

    source.grow(seqSz + algoSz + digestSz);  // make sure enough room
    source.add(seqArray,  seqSz);
    source.add(algoArray, algoSz);
    source.add(digArray,  digestSz);
}



word32 Signature_Encoder::SetDigest(const byte* d, word32 dSz, byte* output)
{
    output[0] = OCTET_STRING;
    output[1] = dSz;
    memcpy(&output[2], d, dSz);
    
    return dSz + 2;
}



word32 DER_Encoder::SetAlgoID(HashType aOID, byte* output)
{
    // adding TAG_NULL and 0 to end
    static const byte shaAlgoID[] = { 0x2b, 0x0e, 0x03, 0x02, 0x1a,
                                      0x05, 0x00 };
    static const byte md5AlgoID[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
                                      0x02, 0x05, 0x05, 0x00  };
    static const byte md2AlgoID[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
                                      0x02, 0x02, 0x05, 0x00};
    static const byte sha256AlgoID[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
                                         0x04, 0x02, 0x01, 0x05, 0x00 };
    static const byte sha384AlgoID[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
                                         0x04, 0x02, 0x02, 0x05, 0x00 };
    static const byte sha512AlgoID[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
                                         0x04, 0x02, 0x03, 0x05, 0x00 };
    int algoSz = 0;
    const byte* algoName = 0;

    switch (aOID) {
    case SHAh:
        algoSz = sizeof(shaAlgoID);
        algoName = shaAlgoID;
        break;

    case SHA256h:
        algoSz = sizeof(sha256AlgoID);
        algoName = sha256AlgoID;
        break;

    case SHA384h:
        algoSz = sizeof(sha384AlgoID);
        algoName = sha384AlgoID;
        break;

    case SHA512h:
        algoSz = sizeof(sha512AlgoID);
        algoName = sha512AlgoID;
        break;

    case MD2h:
        algoSz = sizeof(md2AlgoID);
        algoName = md2AlgoID;
        break;

    case MD5h:
        algoSz = sizeof(md5AlgoID);
        algoName = md5AlgoID;
        break;

    default:
        error_.SetError(UNKOWN_HASH_E);
        return 0;
    }


    byte ID_Length[MAX_LENGTH_SZ];
    word32 idSz = SetLength(algoSz - 2, ID_Length); // don't include TAG_NULL/0

    byte seqArray[MAX_SEQ_SZ + 1];  // add object_id to end
    word32 seqSz = SetSequence(idSz + algoSz + 1, seqArray);
    seqArray[seqSz++] = OBJECT_IDENTIFIER;

    memcpy(output, seqArray, seqSz);
    memcpy(output + seqSz, ID_Length, idSz);
    memcpy(output + seqSz + idSz, algoName, algoSz);

    return seqSz + idSz + algoSz;
}


word32 SetSequence(word32 len, byte* output)
{
  
    output[0] = SEQUENCE | CONSTRUCTED;
    return SetLength(len, output + 1) + 1;
}


word32 EncodeDSA_Signature(const byte* signature, byte* output)
{
    Integer r(signature, 20);
    Integer s(signature + 20, 20);

    return EncodeDSA_Signature(r, s, output);
}


word32 EncodeDSA_Signature(const Integer& r, const Integer& s, byte* output)
{
    word32 rSz = r.ByteCount();
    word32 sSz = s.ByteCount();

    byte rLen[MAX_LENGTH_SZ + 1];
    byte sLen[MAX_LENGTH_SZ + 1];

    rLen[0] = INTEGER;
    sLen[0] = INTEGER;

    word32 rLenSz = SetLength(rSz, &rLen[1]) + 1;
    word32 sLenSz = SetLength(sSz, &sLen[1]) + 1;

    byte seqArray[MAX_SEQ_SZ];

    word32 seqSz = SetSequence(rLenSz + rSz + sLenSz + sSz, seqArray);
    
    // seq
    memcpy(output, seqArray, seqSz);
    // r
    memcpy(output + seqSz, rLen, rLenSz);
    r.Encode(output + seqSz + rLenSz, rSz);
    // s
    memcpy(output + seqSz + rLenSz + rSz, sLen, sLenSz);
    s.Encode(output + seqSz + rLenSz + rSz + sLenSz, sSz);

    return seqSz + rLenSz + rSz + sLenSz + sSz;
}


// put sequence encoded dsa signature into decoded in 2 20 byte integers
word32 DecodeDSA_Signature(byte* decoded, const byte* encoded, word32 sz)
{
    Source source(encoded, sz);

    if (source.next() != (SEQUENCE | CONSTRUCTED)) {
        source.SetError(SEQUENCE_E);
        return 0;
    }

    GetLength(source);  // total

    // r
    if (source.next() != INTEGER) {
        source.SetError(INTEGER_E);
        return 0;
    }
    word32 rLen = GetLength(source);
    if (rLen != 20) {
        while (rLen > 20 && source.remaining() > 0) {  // zero's at front, eat
            source.next();
            --rLen;
        }
        if (rLen < 20) { // add zero's to front so 20 bytes
            word32 tmpLen = rLen;
            while (tmpLen < 20) {
            decoded[0] = 0;
            decoded++;
                tmpLen++;
        }
        }
    }
    memcpy(decoded, source.get_buffer() + source.get_index(), rLen);
    source.advance(rLen);

    // s
    if (source.next() != INTEGER) {
        source.SetError(INTEGER_E);
        return 0;
    }
    word32 sLen = GetLength(source);
    if (sLen != 20) {
        while (sLen > 20 && source.remaining() > 0) {
            source.next();          // zero's at front, eat
            --sLen;
        }
        if (sLen < 20) { // add zero's to front so 20 bytes
            word32 tmpLen = sLen;
            while (tmpLen < 20) {
                decoded[rLen] = 0;
            decoded++;
                tmpLen++;
        }
        }
    }
    memcpy(decoded + rLen, source.get_buffer() + source.get_index(), sLen);
    source.advance(sLen);

    return 40;
}


/*
// Get Cert in PEM format from BEGIN to END
int GetCert(Source& source)
{
    char header[] = "-----BEGIN CERTIFICATE-----";
    char footer[] = "-----END CERTIFICATE-----";

    char* begin = strstr((char*)source.get_buffer(), header);
    char* end   = strstr((char*)source.get_buffer(), footer);

    if (!begin || !end || begin >= end) return -1;

    end += strlen(footer); 
    if (*end == '\r') end++;

    Source tmp((byte*)begin, end - begin + 1);
    source.Swap(tmp);

    return 0;
}



// Decode a BER encoded PKCS12 structure
void PKCS12_Decoder::Decode()
{
    ReadHeader();
    if (source_.GetError().What()) return;

    // Get AuthSafe

    GetSequence();
    
        // get object id
    byte obj_id = source_.next();
    if (obj_id != OBJECT_IDENTIFIER) {
        source_.SetError(OBJECT_ID_E);
        return;
    }

    word32 length = GetLength(source_);

    word32 algo_sum = 0;
    while (length--)
        algo_sum += source_.next();

    
       



    // Get MacData optional
    // mac     digestInfo  like certdecoder::getdigest?
    // macsalt octet string
    // iter    integer
    
}


void PKCS12_Decoder::ReadHeader()
{
    // Gets Version
    GetSequence();
    GetVersion();
}


// Get Cert in PEM format from pkcs12 file
int GetPKCS_Cert(const char* password, Source& source)
{
    PKCS12_Decoder pkcs12(source);
    pkcs12.Decode();

    return 0;
}
*/



} // namespace
