/***********************************************************************

Copyright (c) 2019, Oracle and/or its affiliates. All Rights Reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/** @file include/os0enc.h
 Page encryption infrastructure. */

#ifndef os0enc_h
#define os0enc_h

#include "univ.i"

// Forward declaration.
class IORequest;

/** Encryption algorithm. */
class Encryption {
 public:
  /** Algorithm types supported */
  enum Type {

    /** No encryption */
    NONE = 0,

    /** Use AES */
    AES = 1,
  };

  /** Encryption information format version */
  enum Version {

    /** Version in 5.7.11 */
    VERSION_1 = 0,

    /** Version in > 5.7.11 */
    VERSION_2 = 1,

    /** Version in > 8.0.4 */
    VERSION_3 = 2,
  };

  /** Encryption magic bytes for 5.7.11, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V1[] = "lCA";

  /** Encryption magic bytes for 5.7.12+, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V2[] = "lCB";

  /** Encryption magic bytes for 8.0.5+, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V3[] = "lCC";

  /** Encryption master key prifix */
  static constexpr char MASTER_KEY_PREFIX[] = "INNODBKey";

  /** Default master key for bootstrap */
  static constexpr char DEFAULT_MASTER_KEY[] = "DefaultMasterKey";

  /** Encryption key length */
  static constexpr size_t KEY_LEN = 32;

  /** Encryption magic bytes size */
  static constexpr size_t MAGIC_SIZE = 3;

  /** Encryption master key prifix size */
  static constexpr size_t MASTER_KEY_PRIFIX_LEN = 9;

  /** Encryption master key prifix size */
  static constexpr size_t MASTER_KEY_NAME_MAX_LEN = 100;

  /** UUID of server instance, it's needed for composing master key name */
  static constexpr size_t SERVER_UUID_LEN = 36;

  /** Encryption information total size: magic number + master_key_id +
  key + iv + server_uuid + checksum */
  static constexpr size_t INFO_SIZE =
      (MAGIC_SIZE + sizeof(uint32) + (KEY_LEN * 2) + SERVER_UUID_LEN +
       sizeof(uint32));

  /** Maximum size of Encryption information considering all
  formats v1, v2 & v3. */
  static constexpr size_t INFO_MAX_SIZE = INFO_SIZE + sizeof(uint32);

  /** Default master key id for bootstrap */
  static constexpr size_t DEFAULT_MASTER_KEY_ID = 0;

  /** (De)Encryption Operation information size */
  static constexpr size_t OPERATION_INFO_SIZE = 1;

  /** Encryption Progress information size */
  static constexpr size_t PROGRESS_INFO_SIZE = sizeof(uint);

  /** Flag bit to indicate if Encryption/Decryption is in progress */
  static constexpr size_t ENCRYPT_IN_PROGRESS = 1 << 0;

  /** Decryption in progress. */
  static constexpr size_t DECRYPT_IN_PROGRESS = 1 << 1;

  /** Default constructor */
  Encryption() noexcept : m_type(NONE) {}

  /** Specific constructor
  @param[in]  type    Algorithm type */
  explicit Encryption(Type type) noexcept : m_type(type) {
#ifdef UNIV_DEBUG
    switch (m_type) {
      case NONE:
      case AES:

      default:
        ut_error;
    }
#endif /* UNIV_DEBUG */
  }

  /** Copy constructor */
  Encryption(const Encryption &other) noexcept
      : m_type(other.m_type),
        m_key(other.m_key),
        m_klen(other.m_klen),
        m_iv(other.m_iv) {}

  Encryption &operator=(const Encryption &) = default;

  /** Check if page is encrypted page or not
  @param[in]  page  page which need to check
  @return true if it is an encrypted page */
  static bool is_encrypted_page(const byte *page) noexcept MY_ATTRIBUTE(
      (warn_unused_result));

  /** Check if a log block is encrypted or not
  @param[in]  block block which need to check
  @return true if it is an encrypted block */
  static bool is_encrypted_log(const byte *block) noexcept MY_ATTRIBUTE(
      (warn_unused_result));

  /** Check the encryption option and set it
  @param[in]      option      encryption option
  @param[in,out]  encryption  The encryption type
  @return DB_SUCCESS or DB_UNSUPPORTED */
  dberr_t set_algorithm(
      const char *option,
      Encryption *type) noexcept MY_ATTRIBUTE((warn_unused_result));

  /** Validate the algorithm string.
  @param[in]  option  Encryption option
  @return DB_SUCCESS or error code */
  static dberr_t validate(const char *option) noexcept MY_ATTRIBUTE(
      (warn_unused_result));

  /** Convert to a "string".
  @param[in]  type  The encryption type
  @return the string representation */
  static const char *to_string(Type type) noexcept MY_ATTRIBUTE(
      (warn_unused_result));

  /** Check if the string is "empty" or "none".
  @param[in]  algorithm  Encryption algorithm to check
  @return true if no algorithm requested */
  static bool is_none(const char *algorithm) noexcept MY_ATTRIBUTE(
      (warn_unused_result));

  /** Generate random encryption value for key and iv.
  @param[in,out]  value Encryption value */
  static void random_value(byte *value) noexcept;

  /** Create new master key for key rotation.
  @param[in,out]  master_key  master key */
  static void create_master_key(byte **master_key) noexcept;

  /** Get master key by key id.
  @param[in]      master_key_id master key id
  @param[in]      srv_uuid      uuid of server instance
  @param[in,out]  master_key    master key */
  static void get_master_key(ulint master_key_id, char *srv_uuid,
                             byte **master_key) noexcept;

  /** Get current master key and key id.
  @param[in,out]  master_key_id master key id
  @param[in,out]  master_key    master key */
  static void get_master_key(ulint *master_key_id, byte **master_key) noexcept;

  /** Fill the encryption information.
  @param[in]      key           encryption key
  @param[in]      iv            encryption iv
  @param[in,out]  encrypt_info  encryption information
  @param[in]      is_boot       if it's for bootstrap
  @param[in]      encrypt_key   encrypt with master key
  @return true if success. */
  static bool fill_encryption_info(byte *key, byte *iv, byte *encrypt_info,
                                   bool is_boot, bool encrypt_key) noexcept;

  /** Get master key from encryption information
  @param[in]      encrypt_info  encryption information
  @param[in]      version       version of encryption information
  @param[in,out]  m_key_id      master key id
  @param[in,out]  srv_uuid      server uuid
  @param[in,out]  master_key    master key
  @return position after master key id or uuid, or the old position
  if can't get the master key. */
  static byte *get_master_key_from_info(byte *encrypt_info, Version version,
                                        uint32_t *m_key_id, char *srv_uuid,
                                        byte **master_key) noexcept;

  /** Decoding the encryption info from the first page of a tablespace.
  @param[in,out]  key             key
  @param[in,out]  iv              iv
  @param[in]      encryption_info encryption info
  @param[in]      decrypt_key     decrypt key using master key
  @return true if success */
  static bool decode_encryption_info(byte *key, byte *iv, byte *encryption_info,
                                     bool decrypt_key) noexcept;

  /** Encrypt the redo log block.
  @param[in]      type      IORequest
  @param[in,out]  src_ptr   log block which need to encrypt
  @param[in,out]  dst_ptr   destination area
  @return true if success. */
  bool encrypt_log_block(const IORequest &type, byte *src_ptr,
                         byte *dst_ptr) noexcept;

  /** Encrypt the redo log data contents.
  @param[in]      type      IORequest
  @param[in,out]  src       page data which need to encrypt
  @param[in]      src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return buffer data, dst_len will have the length of the data */
  byte *encrypt_log(const IORequest &type, byte *src, ulint src_len, byte *dst,
                    ulint *dst_len) noexcept;

  /** Encrypt the page data contents. Page type can't be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE.
  @param[in]      type      IORequest
  @param[in,out]  src       page data which need to encrypt
  @param[in]      src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return buffer data, dst_len will have the length of the data */
  byte *encrypt(const IORequest &type, byte *src, ulint src_len, byte *dst,
                ulint *dst_len) noexcept MY_ATTRIBUTE((warn_unused_result));

  /** Decrypt the log block.
  @param[in]      type  IORequest
  @param[in,out]  src   data read from disk, decrypted data
                        will be copied to this page
  @param[in,out]  dst   scratch area to use for decryption
  @return DB_SUCCESS or error code */
  dberr_t decrypt_log_block(const IORequest &type, byte *src,
                            byte *dst) noexcept;

  /** Decrypt the log data contents.
  @param[in]      type      IORequest
  @param[in,out]  src       data read from disk, decrypted data
                            will be copied to this page
  @param[in]      src_len   source data length
  @param[in,out]  dst       scratch area to use for decryption
  @param[in]      dst_len   size of the scratch area in bytes
  @return DB_SUCCESS or error code */
  dberr_t decrypt_log(const IORequest &type, byte *src, ulint src_len,
                      byte *dst, ulint dst_len) noexcept;

  /** Decrypt the page data contents. Page type must be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE, if not then the source contents are
  left unchanged and DB_SUCCESS is returned.
  @param[in]      type    IORequest
  @param[in,out]  src     data read from disk, decrypt
                          data will be copied to this page
  @param[in]      src_len source data length
  @param[in,out]  dst     scratch area to use for decrypt
  @param[in]  dst_len     size of the scratch area in bytes
  @return DB_SUCCESS or error code */
  dberr_t decrypt(const IORequest &type, byte *src, ulint src_len, byte *dst,
                  ulint dst_len) noexcept MY_ATTRIBUTE((warn_unused_result));

  /** Check if keyring plugin loaded. */
  static bool check_keyring() noexcept;

  /** Get encryption type
  @return encryption type **/
  Type get_type() const;

  /** Set encryption type
  @param[in] encryption type **/
  void set_type(Type type);

  /** Get encryption key
  @return encryption key **/
  byte *get_key() const;

  /** Set encryption key
  @param[in] encryption key **/
  void set_key(byte *key);

  /** Get key length
  @return key length **/
  ulint get_key_length() const;

  /** Set key length
  @param[in] key length **/
  void set_key_length(ulint klen);

  /** Get initial vector
  @return initial vector **/
  byte *get_initial_vector() const;

  /** Set initial vector
  @param[in] initial_vector **/
  void set_initial_vector(byte *iv);

  /** Get master key id
  @return master key id **/
  static ulint get_master_key_id();

 private:
  /** Encrypt type */
  Type m_type;

  /** Encrypt key */
  byte *m_key;

  /** Encrypt key length*/
  ulint m_klen;

  /** Encrypt initial vector */
  byte *m_iv;

  /** Current master key id */
  static ulint s_master_key_id;

  /** Current uuid of server instance */
  static char s_uuid[SERVER_UUID_LEN + 1];
};

#endif /* os0enc_h */
