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

#ifndef RPL_LOG_ENCRYPTION_INCLUDED
#define RPL_LOG_ENCRYPTION_INCLUDED

#include <openssl/evp.h>
#include <sql/basic_istream.h>
#include <sql/basic_ostream.h>
#include <sql/sql_class.h>
#include <map>
#include <string>

/**
  @file rpl_log_encryption.h

  @brief This file includes the major components for encrypting/decrypting
         binary log files.

  * Replication logs

    Here, replication logs includes both the binary and relay log files.

  * File Level Encryption

    - All standard binary log file data (including BINLOG_MAGIC) in replication
      logs are encrypted.

    - A replication log file is either encrypted or not (standard binary log
      file). It is not possible that part of a log file is encrypted and part
      of it is non-encrypted.

    - There is an encryption header in the begin of each encrypted replication
      log file.

      <pre>
            +--------------------+
            |  Encryption Header |
            +--------------------+
            |  Encrypted Data    |
            +--------------------+
      </pre>

      The encrypted replication file header includes necessary information to
      decrypt the encrypted data of the file (the standard binary log file
      data). For detail, check Rpl_encryption_header class.

  * Two Tier Keys

    Replication logs are encrypted with two tier keys. A 'File Password' for
    encrypting the standard binary log file data and a 'Replication Encryption
    Key' for encrypting the 'File Password'.

    - File password

      Each replication log file has a password. A file key used to encrypt the
      file is generated from the file password. The encrypted 'File Password'
      is stored into encryption header of the file. For details, check
      Rpl_encryption_header class.

    - Replication encryption key

      A replication encryption key is used to encrypt/decrypt the file
      password stored in an encrypted replication file header. It is generated
      by keyring and stored in/retrieved from keyring.
*/
typedef std::basic_string<unsigned char> Key_string;

#ifdef MYSQL_SERVER

/**
  The Rpl_encryption class is the container for the replication log encryption
  feature generic and server instance functions.
*/
class Rpl_encryption {
 public:
  enum class Keyring_status {
    SUCCESS = 0,
    KEYRING_ERROR = 1,
    KEY_NOT_FOUND = 2,
    UNEXPECTED_KEY_SIZE = 3,
    UNEXPECTED_KEY_TYPE = 4
  };

  /**
    Get the key with given key ID. The key to be returned will be retrieved
    from the keyring or from a cached copy in memory.

    @param[in] key_id ID of the key to be returned.
    @param[in] key_type Expected type of the key to be returned.

    @return A pair containing the status of the operation (Keyring_status) and
            a Key_string. Errors shall be checked by consulting the status.
  */
  static std::pair<Keyring_status, Key_string> get_key(
      const std::string &key_id, const std::string &key_type);

  /**
    Get the key with given key ID. The key to be returned will be retrieved
    from the keyring or from a cached copy in memory.

    @param[in] key_id ID of the key to be returned.
    @param[in] key_type Expected type of the key to be returned.
    @param[in] key_size Expected size of the key to be returned.

    @return A pair containing the status of the operation (Keyring_status) and
            a Key_string. Errors shall be checked by consulting the status.
  */
  static std::pair<Keyring_status, Key_string> get_key(
      const std::string &key_id, const std::string &key_type, size_t key_size);

 private:
  /**
    Fetch a key from keyring. When error happens, it either reports an error to
    user or write an error to log accordingly.

    @param[in] key_id ID of the key to be returned.
    @param[in] key_type Expected type of the key to be returned.

    @return A tuple containing the status of the operation (Keyring_status), a
            pointer to the fetched key (nullptr if the key was not fetched) and
            the returned key size. Errors shall be checked by consulting the
            status.
  */
  static std::tuple<Keyring_status, void *, size_t> fetch_key_from_keyring(
      const std::string &key_id, const std::string &key_type);
};

#endif  // MYSQL_SERVER

class Rpl_cipher {
 public:
  virtual ~Rpl_cipher(){};

  /**
    Open the cipher with given password.

    @param[in] password The password which is used to initialize the cypher.
    @param[in] header_size The encrypted stream offset.

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
  */
  virtual int get_header_size() = 0;
};

/**
  @class Rpl_encryption_header

  It implements the feature to serialize and deserialize a replication log file
  encryption header.

  The new encrypted binary log file format is composed of two parts:

  <pre>
      +---------------------+
      |  Encryption Header  |
      +---------------------+
      |   Encrypted Data    |
      +---------------------+
  </pre>

  The encryption header exists only in the begin of encrypted replication log
  files.

  <pre>
    +------------------------+----------------------------------------------+
    | MAGIC HEADER (4 bytes) | Replication logs encryption version (1 byte) |
    +------------------------+----------------------------------------------+
    |                Version specific encryption header data                |
    +-----------------------------------------------------------------------+
                             Encryption Header Format
  </pre>

  <table>
  <caption>Encryption Header Format</caption>
  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>Magic Header</td>
    <td>4 Bytes</td>
    <td>
      The content is always 0xFD62696E. It is similar to Binlog Magic Header.
      Binlog magic header is: 0xFE62696e.
    </td>
  <tr>
    <td>Replication logs encryption version</td>
    <td>1 Byte</td>
    <td>
      The replication logs encryption version defines how the header shall be
      deserialized and how the Encrypted Data shall be decrypted.
    </td>
  </tr>
  <tr>
    <td>Version specific encryption data header</td>
    <td>Depends on the version field</td>
    <td>
      Data required to fetch a replication key from keyring and deserialize
      the Encrypted Data.
    </td>
  </tr>
  </table>
*/
class Rpl_encryption_header {
 public:
  static const int ENCRYPTION_MAGIC_SIZE = 4;
  static const char *ENCRYPTION_MAGIC;

  virtual ~Rpl_encryption_header();

  static std::unique_ptr<Rpl_encryption_header> get_header(
      Basic_istream *istream);

  /**
    Deserialize encryption header from a stream.

    @param[in] istream The input stream for deserializing the encryption
                       header.

    @retval false Success.
    @retval true Error.
  */
  virtual bool deserialize(Basic_istream *istream) = 0;
  virtual char get_version() const = 0;
  virtual const std::string &get_key_id() const = 0;
  virtual const Key_string &get_encrypted_password() const = 0;
  virtual const Key_string &get_iv() const = 0;
  virtual int get_header_size() = 0;
  virtual Key_string decrypt_file_password() = 0;
  virtual std::unique_ptr<Rpl_cipher> get_encryptor() = 0;
  virtual std::unique_ptr<Rpl_cipher> get_decryptor() = 0;

 protected:
  static const int VERSION_OFFSET = ENCRYPTION_MAGIC_SIZE;
  static const int VERSION_SIZE = 1;
  static const int OPTIONAL_FIELD_OFFSET = VERSION_OFFSET + VERSION_SIZE;
};

/**
  @class Rpl_encryption_header_v1

  <pre>
    +------------------------+----------------------------------------------+
    | MAGIC HEADER (4 bytes) | Replication logs encryption version (1 byte) |
    +------------------------+----------------------------------------------+
    |             Replication Encryption Key ID (60 to 69 bytes)            |
    +-----------------------------------------------------------------------+
    |                   Encrypted File Password (33 bytes)                  |
    +-----------------------------------------------------------------------+
    |               IV For Encrypting File Password (17 bytes)              |
    +-----------------------------------------------------------------------+
    |                       Padding (388 to 397 bytes)                      |
    +-----------------------------------------------------------------------+
                Encrypted binary log file header format version 1
  </pre>

  <table>
  <caption>Encrypted binary log file header format version 1</caption>
  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>Replication Encryption Key ID</td>
    <td>
     Variable length field that uses Type, Length, Value (TLV) format. Type
     takes 1 byte. Length takes 1 byte. Values takes Length bytes.
    </td>
    <td>
      ID of the key that shall be retrieved from keyring to be used to decrypt
      the file password field.
    </td>
  </tr>
  <tr>
    <td>Encrypted File Password</td>
    <td>
      Fixed length field that uses Type, Value format. Type takes 1 byte.
      Value takes 32 bytes.</td>
    <td>It is the encrypted file password.</td>
  </tr>
  <tr>
    <td>IV for Encrypting File Password</td>
    <td>
      Fixed length field that uses Type, Value format. Type takes 1 byte.
      Value takes 16 bytes.</td>
    <td>
      The iv, together with the key, is used to encrypt/decrypt the
      file password.
    </td>
  </tr>
  <tr>
    <td>Padding</td>
    <td>Variable length, all bytes are 0.</td>
    <td>
      Encryption header has 512 bytes. Above fields don't take all bytes. All
      unused bytes are filled with 0 as padding.
    </td>
  </tr>
  </table>
*/
class Rpl_encryption_header_v1 : public Rpl_encryption_header {
 public:
  static const char *KEY_TYPE;
  static const int KEY_LENGTH = 32;
  static const int HEADER_SIZE = 512;
  static const int IV_FIELD_SIZE = 16;
  static const int PASSWORD_FIELD_SIZE = 32;

  Rpl_encryption_header_v1() = default;

  virtual ~Rpl_encryption_header_v1();
  /**
    Initialize the class with given encryption metadata. It is called for
    deserialization.

    @param[in] key_id ID of the master key used to encrypt the file password.
    @param[in] encrypted_password The encrypted file password.
    @param[in] iv The initialization vector (IV) used to encrypt file password.
  */
  Rpl_encryption_header_v1(const std::string &key_id,
                           const Key_string &encrypted_password,
                           const Key_string &iv);

  bool deserialize(Basic_istream *istream);
  char get_version() const;
  const std::string &get_key_id() const;
  const Key_string &get_encrypted_password() const;
  const Key_string &get_iv() const;
  int get_header_size();
  Key_string decrypt_file_password();
  std::unique_ptr<Rpl_cipher> get_encryptor();
  std::unique_ptr<Rpl_cipher> get_decryptor();

 private:
  enum Field_type {
    KEY_ID = 1,
    ENCRYPTED_FILE_PASSWORD = 2,
    IV_FOR_FILE_PASSWORD = 3
  };

  char m_version = 1;
  std::string m_key_id;
  Key_string m_encrypted_password;
  Key_string m_iv;
};

enum Cipher_type { ENCRYPT, DECRYPT };

/**
  @class Aes_ctr_cipher

  The class implements AES-CTR encryption/decryption. It supports to
  encrypt/decrypt a stream in both sequential and random way.

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
template <Cipher_type TYPE>
class Aes_ctr_cipher : public Rpl_cipher {
 public:
  static const int PASSWORD_LENGTH = 32;
  static const int AES_BLOCK_SIZE = 16;
  static const int FILE_KEY_LENGTH = 32;

  virtual ~Aes_ctr_cipher();

  virtual bool open(const Key_string &password, int header_size);
  virtual void close();
  virtual bool encrypt(unsigned char *dest, const unsigned char *src,
                       int length);
  virtual bool decrypt(unsigned char *dest, const unsigned char *src,
                       int length);
  virtual bool set_stream_offset(uint64_t offset);
  virtual int get_header_size();

 private:
  EVP_CIPHER_CTX *m_ctx = nullptr;
  /* The file key to encrypt/decrypt data. */
  unsigned char m_file_key[FILE_KEY_LENGTH];
  /* The initialization vector (IV) used to encrypt/decrypt data. */
  unsigned char m_iv[AES_BLOCK_SIZE];

  int m_header_size = 0;

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
#endif  // RPL_LOG_ENCRYPTION_INCLUDED
