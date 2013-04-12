/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* asn.hpp provides ASN1 BER, PublicKey, and x509v3 decoding 
*/


#ifndef TAO_CRYPT_ASN_HPP
#define TAO_CRYPT_ASN_HPP


#include <time.h>
#include "misc.hpp"
#include "block.hpp"
#include "error.hpp"
#ifdef USE_SYS_STL
    #include <list>
#else
    #include "list.hpp"
#endif


namespace STL = STL_NAMESPACE;


namespace TaoCrypt {

// these tags and flags are not complete
enum ASNTag
{
    BOOLEAN             = 0x01,
    INTEGER             = 0x02,
    BIT_STRING          = 0x03,
    OCTET_STRING        = 0x04,
    TAG_NULL            = 0x05,
    OBJECT_IDENTIFIER   = 0x06,
    OBJECT_DESCRIPTOR   = 0x07,
    EXTERNAL            = 0x08,
    REAL                = 0x09,
    ENUMERATED          = 0x0a,
    UTF8_STRING         = 0x0c,
    SEQUENCE            = 0x10,
    SET                 = 0x11,
    NUMERIC_STRING      = 0x12,
    PRINTABLE_STRING    = 0x13,
    T61_STRING          = 0x14,
    VIDEOTEXT_STRING    = 0x15,
    IA5_STRING          = 0x16,
    UTC_TIME            = 0x17,
    GENERALIZED_TIME    = 0x18,
    GRAPHIC_STRING      = 0x19,
    VISIBLE_STRING      = 0x1a,
    GENERAL_STRING      = 0x1b,
    LONG_LENGTH         = 0x80
};

enum ASNIdFlag
{
    UNIVERSAL           = 0x00,
    DATA                = 0x01,
    HEADER              = 0x02,
    CONSTRUCTED         = 0x20,
    APPLICATION         = 0x40,
    CONTEXT_SPECIFIC    = 0x80,
    PRIVATE             = 0xc0
};


enum DNTags
{
    COMMON_NAME         = 0x03,  // CN
    SUR_NAME            = 0x04,  // SN
    COUNTRY_NAME        = 0x06,  // C
    LOCALITY_NAME       = 0x07,  // L
    STATE_NAME          = 0x08,  // ST
    ORG_NAME            = 0x0a,  // O
    ORGUNIT_NAME        = 0x0b   // OU
};


enum PCKS12_Tags
{
    /* DATA                      = 1, */ // from ASN1
    SIGNED_DATA               = 2,
    ENVELOPED_DATA            = 3,
    SIGNED_AND_ENVELOPED_DATA = 4,
    DIGESTED_DATA             = 5,
    ENCRYPTED_DATA            = 6
};


enum Constants
{
    MIN_DATE_SZ   = 13,
    MAX_DATE_SZ   = 16,
    MAX_ALGO_SZ   = 16,
    MAX_LENGTH_SZ =  5,    
    MAX_SEQ_SZ    =  5,    // enum(seq|con) + length(4)
    MAX_ALGO_SIZE =  9,
    MAX_DIGEST_SZ = 69,    // SHA512 + enum(Bit or Octet) + length(4)
    DSA_SIG_SZ    = 40,
    ASN_NAME_MAX  = 512    // max total of all included names
};


class Source;
class RSA_PublicKey;
class RSA_PrivateKey;
class DSA_PublicKey;
class DSA_PrivateKey;
class Integer;
class DH;


// General BER decoding
class BER_Decoder : public virtual_base {
protected:
    Source& source_;
public:
    explicit BER_Decoder(Source& s) : source_(s) {}
    virtual ~BER_Decoder() {}

    Integer& GetInteger(Integer&);
    word32   GetSequence();
    word32   GetSet();
    word32   GetVersion();
    word32   GetExplicitVersion();

    Error GetError();
private:
    virtual void ReadHeader() = 0;

    BER_Decoder(const BER_Decoder&);            // hide copy
    BER_Decoder& operator=(const BER_Decoder&); // and assign
};


// RSA Private Key BER Decoder
class RSA_Private_Decoder : public BER_Decoder {
public:
    explicit RSA_Private_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode(RSA_PrivateKey&);
private:
    void ReadHeader();
};


// RSA Public Key BER Decoder
class RSA_Public_Decoder : public BER_Decoder {
public:
    explicit RSA_Public_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode(RSA_PublicKey&);
private:
    void ReadHeader();
    void ReadHeaderOpenSSL();
};


// DSA Private Key BER Decoder
class DSA_Private_Decoder : public BER_Decoder {
public:
    explicit DSA_Private_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode(DSA_PrivateKey&);
private:
    void ReadHeader();
};


// DSA Public Key BER Decoder
class DSA_Public_Decoder : public BER_Decoder {
public:
    explicit DSA_Public_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode(DSA_PublicKey&);
private:
    void ReadHeader();
};


// DH Key BER Decoder
class DH_Decoder : public BER_Decoder {
public:
    explicit DH_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode(DH&);
private:
    void ReadHeader();
};


// PKCS12 BER Decoder
class PKCS12_Decoder : public BER_Decoder {
public:
    explicit PKCS12_Decoder(Source& s) : BER_Decoder(s) {}
    void Decode();
private:
    void ReadHeader();
};


// General PublicKey
class PublicKey {
    byte*  key_;
    word32 sz_;
public:
    explicit PublicKey(const byte* k = 0, word32 s = 0);
    ~PublicKey() { tcArrayDelete(key_); }

    const byte* GetKey() const { return key_; }
    word32      size()   const { return sz_; }

    void SetKey(const byte*);
    void SetSize(word32 s);

    void AddToEnd(const byte*, word32);
private:
    PublicKey(const PublicKey&);            // hide copy
    PublicKey& operator=(const PublicKey&); // and assign
};


enum { SHA_SIZE = 20 };


// A Signing Authority
class Signer {
    PublicKey key_;
    char      name_[ASN_NAME_MAX];
    byte      hash_[SHA_SIZE];
public:
    Signer(const byte* k, word32 kSz, const char* n, const byte* h);
    ~Signer();

    const PublicKey& GetPublicKey()  const { return key_; }
    const char*      GetName()       const { return name_; }
    const byte*      GetHash()       const { return hash_; }

private:
    Signer(const Signer&);              // hide copy
    Signer& operator=(const Signer&);   // and assign
};


typedef STL::list<Signer*> SignerList;


enum ContentType { HUH = 651 };
enum SigType  { SHAwDSA = 517, MD2wRSA = 646, MD5wRSA = 648, SHAwRSA = 649,
                SHA256wRSA = 655, SHA384wRSA = 656, SHA512wRSA = 657,
                SHA256wDSA = 416 };
enum HashType { MD2h = 646, MD5h = 649, SHAh = 88, SHA256h = 414,
                SHA384h = 415, SHA512h = 416 };
enum KeyType  { DSAk = 515, RSAk = 645 };     // sums of algo OID


// an x509v Certificate BER Decoder
class CertDecoder : public BER_Decoder {
public:
    enum DateType { BEFORE, AFTER };   
    enum NameType { ISSUER, SUBJECT };
    enum CertType { CA, USER };

    explicit CertDecoder(Source&, bool decode = true, SignerList* sl = 0,
                         bool noVerify = false, CertType ct = USER);
    ~CertDecoder();

    const PublicKey& GetPublicKey()  const { return key_; }
    KeyType          GetKeyType()    const { return KeyType(keyOID_); }
    const char*      GetIssuer()     const { return issuer_; }
    const char*      GetCommonName() const { return subject_; }
    const byte*      GetHash()       const { return subjectHash_; }
    const char*      GetBeforeDate() const { return beforeDate_; }
    byte             GetBeforeDateType() const { return beforeDateType_; }
    const char*      GetAfterDate()  const { return afterDate_; }
    byte             GetAfterDateType() const { return afterDateType_; }

    void DecodeToKey();
private:
    PublicKey key_;
    word32    certBegin_;               // offset to start of cert
    word32    sigIndex_;                // offset to start of signature
    word32    sigLength_;               // length of signature
    word32    signatureOID_;            // sum of algorithm object id
    word32    keyOID_;                  // sum of key algo  object id
    byte      subjectHash_[SHA_SIZE];   // hash of all Names
    byte      issuerHash_[SHA_SIZE];    // hash of all Names
    byte*     signature_;
    char      issuer_[ASN_NAME_MAX];    // Names
    char      subject_[ASN_NAME_MAX];   // Names
    char      beforeDate_[MAX_DATE_SZ]; // valid before date
    byte      beforeDateType_;          // beforeDate time type
    char      afterDate_[MAX_DATE_SZ];  // valid after date
    byte      afterDateType_;           // afterDate time type
    bool      verify_;                  // Default to yes, but could be off

    void   ReadHeader();
    void   Decode(SignerList*, CertType);
    void   StoreKey();
    void   AddDSA();
    bool   ValidateSelfSignature();
    bool   ValidateSignature(SignerList*);
    bool   ConfirmSignature(Source&);
    void   GetKey();
    char*  AddTag(char*, const char*, const char*, word32, word32);
    void   GetName(NameType);
    void   GetValidity();
    void   GetDate(DateType);
    void   GetCompareHash(const byte*, word32, byte*, word32);
    word32 GetAlgoId();
    word32 GetSignature();
    word32 GetDigest();
};


word32 GetLength(Source&);

word32 SetLength(word32, byte*);
word32 SetSequence(word32, byte*);

word32 EncodeDSA_Signature(const byte* signature, byte* output);
word32 EncodeDSA_Signature(const Integer& r, const Integer& s, byte* output);
word32 DecodeDSA_Signature(byte* decoded, const byte* encoded, word32 sz);


// General DER encoding
class DER_Encoder : public virtual_base {
public:
    DER_Encoder() {}
    virtual ~DER_Encoder() {}

    word32 SetAlgoID(HashType, byte*);

    Error  GetError() const { return error_; }
private:
    //virtual void WriteHeader() = 0;
    Error error_;

    DER_Encoder(const DER_Encoder&);            // hide copy
    DER_Encoder& operator=(const DER_Encoder&); // and assign
};



class Signature_Encoder : public DER_Encoder {
    const byte* digest_;
    word32      digestSz_;
    SigType     digestOID_;
public:
    explicit Signature_Encoder(const byte*, word32, HashType, Source&);

private:
    void   WriteHeader();
    word32 SetDigest(const byte*, word32, byte*);

    Signature_Encoder(const Signature_Encoder&);            // hide copy
    Signature_Encoder& operator=(const Signature_Encoder&); // and assign
};


// Get Cert in PEM format from BEGIN to END
int GetCert(Source&);

// Get Cert in PEM format from pkcs12 file
int GetPKCS_Cert(const char* password, Source&);

bool ASN1_TIME_extract(const unsigned char* date, unsigned char format,
                       tm *parsed_time);

} // namespace


#endif // TAO_CRYPT_ASN_HPP
