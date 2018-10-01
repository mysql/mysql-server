/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef RPL_CIPHER_INCLUDED
#define RPL_CIPHER_INCLUDED

#include <openssl/evp.h>
#include <string>

/**
  @file rpl_cipher.h

  @brief This file includes core components for encrypting/decrypting
         binary log files.
*/

typedef std::basic_string<unsigned char> Key_string;

/**
  @class Rpl_cipher

  This abstract class represents the interface of a replication logs encryption
  cipher that can be used to encrypt/decrypt a given stream content in both
  sequential and random way.

  - Sequential means encrypting/decrypting a stream from the begin to end
    in order. For sequential encrypting/decrypting, you just need to call
    it like:

      open();
      encrypt();
      ...
      encrypt(); // call it again and again
      ...
      close();

  - Random means encrypting/decrypting a stream data without order. For
    example:

    - It first encrypts the data of a stream at the offset from 100 to 200.

    - And then encrypts the data of the stream at the offset from 0 to 99.

    For random encrypting/decrypting, you need to call set_stream_offset()
    before calling encrypt(). Example:

      open();

      set_stream_offset(100);
      encrypt(...);
      ...
      set_stream_offset(0);
      encrypt(...)

      close();
*/
class Rpl_cipher {
 public:
  virtual ~Rpl_cipher(){};

  /**
    Open the cipher with given password.

    @param[in] password The password which is used to initialize the cipher.
    @param[in] header_size The encrypted stream offset wrt the down stream.

    @retval false Success.
    @retval true Error.
  */
  virtual bool open(const Key_string &password, int header_size) = 0;

  /** Close the cipher. */
  virtual void close() = 0;

  /**
    Encrypt data.

    @param[in] dest The buffer for storing encrypted data. It should be
                    at least 'length' bytes.
    @param[in] src The data which will be encrypted.
    @param[in] length Length of the data.

    @retval false Success.
    @retval true Error.
  */
  virtual bool encrypt(unsigned char *dest, const unsigned char *src,
                       int length) = 0;

  /**
    Decrypt data.

    @param[in] dest The buffer for storing decrypted data. It should be
                    at least 'length' bytes.
    @param[in] src The data which will be decrypted.
    @param[in] length Length of the data.

    @retval false Success.
    @retval true Error.
  */
  virtual bool decrypt(unsigned char *dest, const unsigned char *src,
                       int length) = 0;

  /**
    Support encrypting/decrypting data at random position of a stream.

    @param[in] offset The stream offset of the data which will be encrypted/
                      decrypted in next encrypt()/decrypt() call.

    @retval false Success.
    @retval true Error.
  */
  virtual bool set_stream_offset(uint64_t offset) = 0;

  /**
    Returns the size of the header of the stream being encrypted/decrypted.

    @return the size of the header of the stream being encrypted/decrypted.
  */
  int get_header_size();

 protected:
  int m_header_size = 0;
};

enum Cipher_type { ENCRYPT, DECRYPT };

/**
  @class Aes_ctr_cipher

  The class implements AES-CTR encryption/decryption. It supports to
  encrypt/decrypt a stream in both sequential and random way.
*/
template <Cipher_type TYPE>
class Aes_ctr_cipher : public Rpl_cipher {
 public:
  static const int PASSWORD_LENGTH = 32;
  static const int AES_BLOCK_SIZE = 16;
  static const int FILE_KEY_LENGTH = 32;

  virtual ~Aes_ctr_cipher();

  bool open(const Key_string &password, int header_size) override;
  void close() override;
  bool encrypt(unsigned char *dest, const unsigned char *src,
               int length) override;
  bool decrypt(unsigned char *dest, const unsigned char *src,
               int length) override;
  bool set_stream_offset(uint64_t offset) override;

 private:
  /* Cipher context */
  EVP_CIPHER_CTX *m_ctx = nullptr;
  /* The file key to encrypt/decrypt data. */
  unsigned char m_file_key[FILE_KEY_LENGTH];
  /* The initialization vector (IV) used to encrypt/decrypt data. */
  unsigned char m_iv[AES_BLOCK_SIZE];

  /**
    Initialize OpenSSL cipher related context and IV.

    @param[in] offset The stream offset to compute the AES-CTR counter which
                      will be set into IV.

    @retval false Success.
    @retval true Error.
  */
  bool init_cipher(uint64_t offset);

  /** Destroy OpenSSL cipher related context. */
  void deinit_cipher();
};

typedef class Aes_ctr_cipher<Cipher_type::ENCRYPT> Aes_ctr_encryptor;
typedef class Aes_ctr_cipher<Cipher_type::DECRYPT> Aes_ctr_decryptor;
#endif  // RPL_CIPHER_INCLUDED
