/***********************************************************************

Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <mysql/components/my_service.h>
#include "univ.i"

namespace innobase {
namespace encryption {

bool init_keyring_services(SERVICE_TYPE(registry) * reg_srv);

void deinit_keyring_services(SERVICE_TYPE(registry) * reg_srv);
}  // namespace encryption
}  // namespace innobase

// Forward declaration.
class IORequest;
struct Encryption_key;

// Forward declaration.
struct Encryption_metadata;

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

  /** Encryption progress type. */
  enum class Progress {
    /* Space encryption in progress */
    ENCRYPTION,
    /* Space decryption in progress */
    DECRYPTION,
    /* Nothing in progress */
    NONE
  };

  /** Encryption operation resume point after server restart. */
  enum class Resume_point {
    /* Resume from the beginning. */
    INIT,
    /* Resume processing. */
    PROCESS,
    /* Operation has ended. */
    END,
    /* All done. */
    DONE
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

  /** Encryption key length */
  static constexpr size_t KEY_LEN = 32;

  /** Default master key for bootstrap */
  static constexpr char DEFAULT_MASTER_KEY[] = "DefaultMasterKey";

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
  static constexpr uint32_t DEFAULT_MASTER_KEY_ID = 0;

  /** (De)Encryption Operation information size */
  static constexpr size_t OPERATION_INFO_SIZE = 1;

  /** Encryption Progress information size */
  static constexpr size_t PROGRESS_INFO_SIZE = sizeof(uint);

  /** Flag bit to indicate if Encryption/Decryption is in progress */
  static constexpr size_t ENCRYPT_IN_PROGRESS = 1 << 0;

  /** Decryption in progress. */
  static constexpr size_t DECRYPT_IN_PROGRESS = 1 << 1;

  /** Tablespaces whose key needs to be reencrypted */
  static std::vector<space_id_t> s_tablespaces_to_reencrypt;

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
  Encryption(const Encryption &other) noexcept = default;

  Encryption &operator=(const Encryption &) = default;

  /** Check if page is encrypted page or not
  @param[in]  page  page which need to check
  @return true if it is an encrypted page */
  [[nodiscard]] static bool is_encrypted_page(const byte *page) noexcept;

  /** Check if a log block is encrypted or not
  @param[in]  block block which need to check
  @return true if it is an encrypted block */
  [[nodiscard]] static bool is_encrypted_log(const byte *block) noexcept;

  /** Check the encryption option and set it
  @param[in]      option      encryption option
  @param[in,out]  type        The encryption type
  @return DB_SUCCESS or DB_UNSUPPORTED */
  [[nodiscard]] dberr_t set_algorithm(const char *option,
                                      Encryption *type) noexcept;

  /** Validate the algorithm string.
  @param[in]  option  Encryption option
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t validate(const char *option) noexcept;

  /** Convert to a "string".
  @param[in]  type  The encryption type
  @return the string representation */
  [[nodiscard]] static const char *to_string(Type type) noexcept;

  /** Check if the string is "empty" or "none".
  @param[in]  algorithm  Encryption algorithm to check
  @return true if no algorithm requested */
  [[nodiscard]] static bool is_none(const char *algorithm) noexcept;

  /** Generate random encryption value for key and iv.
  @param[in,out]  value Encryption value */
  static void random_value(byte *value) noexcept;

  /** Copy the given encryption metadata to the given Encryption_metadata
  object, if both key != nullptr and iv != nullptr. Generate randomly the
  new metadata, if both key == nullptr and iv == nullptr, and store it to
  the given Encryption_metadata object. Cannot be called with key, iv such
  that: (key == nullptr) != (iv == nullptr).
  @param[in]  type      encryption algorithm type to store
  @param[in]  key       encryption key to copy or nullptr to generate
  @param[in]  iv        encryption iv to copy or nullptr to generate
  @param[out] metadata  filled Encryption_metadata object */
  static void set_or_generate(Type type, byte *key, byte *iv,
                              Encryption_metadata &metadata);

  /** Create new master key for key rotation.
  @param[in,out]  master_key  master key */
  static void create_master_key(byte **master_key) noexcept;

  /** Get master key by key id.
  @param[in]      master_key_id master key id
  @param[in]      srv_uuid      uuid of server instance
  @param[in,out]  master_key    master key */
  static void get_master_key(uint32_t master_key_id, char *srv_uuid,
                             byte **master_key) noexcept;

  /** Get current master key and key id.
  @param[in,out]  master_key_id master key id
  @param[in,out]  master_key    master key */
  static void get_master_key(uint32_t *master_key_id,
                             byte **master_key) noexcept;

  /** Fill the encryption information.
  @param[in]      encryption_metadata  encryption metadata (key,iv)
  @param[in]      encrypt_key          encrypt with master key
  @param[out]     encrypt_info         encryption information
  @return true if success. */
  static bool fill_encryption_info(
      const Encryption_metadata &encryption_metadata, bool encrypt_key,
      byte *encrypt_info) noexcept;

  /** Get master key from encryption information
  @param[in]      encrypt_info  encryption information
  @param[in]      version       version of encryption information
  @param[in,out]  m_key_id      master key id
  @param[in,out]  srv_uuid      server uuid
  @param[in,out]  master_key    master key
  @return position after master key id or uuid, or the old position
  if can't get the master key. */
  static const byte *get_master_key_from_info(const byte *encrypt_info,
                                              Version version,
                                              uint32_t *m_key_id,
                                              char *srv_uuid,
                                              byte **master_key) noexcept;

  /** Checks if encryption info bytes represent data encrypted by the given
  version of the encryption mechanism.
  @param[in]  encryption_info       encryption info bytes
  @param[in]  version_magic_bytes   magic bytes which represent version
                                    of the encryption mechanism, for example:
                                    Encryption::KEY_MAGIC_V3
  @return result of the check */
  static bool is_encrypted_with_version(
      const byte *encryption_info, const char *version_magic_bytes) noexcept;

  /** Checks if encryption info bytes represent data encrypted by version V3
  of the encryption mechanism.
  @param[in]  encryption_info       encryption info bytes
  @return result of the check */
  static bool is_encrypted_with_v3(const byte *encryption_info) noexcept;

  /** Checks if encryption info bytes represent data encrypted by any of known
  versions of the encryption mechanism. Note, that if the encryption_info is
  read from file created by a newer MySQL version, it could be considered to be
  unknown for this MySQL version, and this function would return false.
  @param[in]  encryption_info       encryption info bytes
  @return result of the check */
  static bool is_encrypted(const byte *encryption_info) noexcept;

  /** Decoding the encryption info from the given array of bytes,
  which are assumed not to be related to any particular tablespace.
  @param[out]     encryption_metadata  decoded encryption metadata
  @param[in]      encryption_info      encryption info to decode
  @param[in]      decrypt_key          decrypt key using master key
  @return true if success */
  static bool decode_encryption_info(Encryption_metadata &encryption_metadata,
                                     const byte *encryption_info,
                                     bool decrypt_key) noexcept;

  /** Decoding the encryption info from the given array of bytes,
  which are assumed to be related to a given tablespace (unless
  space_id == dict_sys_t::s_invalid_space_id). The given tablespace
  is noted down in s_tablespaces_to_reencrypt if the encryption info
  became successfully decrypted using the master key and the space_id
  is not dict_sys_t::s_invalid_space_id. For such tablespaces the
  encryption info is later re-encrypted using the rotated master key
  in innobase_dict_recover().
  @param[in]      space_id        Tablespace id
  @param[in,out]  e_key           key, iv
  @param[in]      encryption_info encryption info to decode
  @param[in]      decrypt_key     decrypt key using master key
  @return true if success */
  static bool decode_encryption_info(space_id_t space_id, Encryption_key &e_key,
                                     const byte *encryption_info,
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
  [[nodiscard]] byte *encrypt(const IORequest &type, byte *src, ulint src_len,
                              byte *dst, ulint *dst_len) noexcept;

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
  @return DB_SUCCESS or error code */
  dberr_t decrypt_log(const IORequest &type, byte *src, ulint src_len,
                      byte *dst) noexcept;

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
  [[nodiscard]] dberr_t decrypt(const IORequest &type, byte *src, ulint src_len,
                                byte *dst, ulint dst_len) noexcept;

  /** Check if keyring plugin loaded. */
  static bool check_keyring() noexcept;

  /** Get encryption type
  @return encryption type **/
  Type get_type() const;

  /** Check if the encryption algorithm is NONE.
  @return true if no algorithm is set, false otherwise. */
  [[nodiscard]] bool is_none() const noexcept { return m_type == NONE; }

  /** Set encryption type
  @param[in]  type  encryption type **/
  void set_type(Type type);

  /** Set encryption key
  @param[in]  key  encryption key **/
  void set_key(const byte *key);

  /** Get key length
  @return  key length **/
  ulint get_key_length() const;

  /** Set key length
  @param[in]  klen  key length **/
  void set_key_length(ulint klen);

  /** Set initial vector
  @param[in]  iv  initial_vector **/
  void set_initial_vector(const byte *iv);

  /** Get master key id
  @return master key id **/
  static uint32_t get_master_key_id();

 private:
  /** Encrypt the page data contents. Page type can't be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE.
  @param[in]  src       page data which need to encrypt
  @param[in]  src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return true if operation successful, false otherwise. */
  [[nodiscard]] bool encrypt_low(byte *src, ulint src_len, byte *dst,
                                 ulint *dst_len) noexcept;

  /** Encrypt type */
  Type m_type;

  /** Encrypt key */
  const byte *m_key;

  /** Encrypt key length*/
  ulint m_klen;

  /** Encrypt initial vector */
  const byte *m_iv;

  /** Current master key id */
  static uint32_t s_master_key_id;

  /** Current uuid of server instance */
  static char s_uuid[SERVER_UUID_LEN + 1];
};

/** Encryption metadata. */
struct Encryption_metadata {
  /** Encrypt type */
  Encryption::Type m_type{Encryption::NONE};

  /** Encrypt key */
  byte m_key[Encryption::KEY_LEN];

  /** Encrypt key length */
  size_t m_key_len{0};

  /** Encrypt initial vector */
  byte m_iv[Encryption::KEY_LEN];

  bool can_encrypt() const { return m_type != Encryption::NONE; }
};

struct Encryption_key {
  /** Encrypt key */
  byte *m_key;

  /** Encrypt initial vector */
  byte *m_iv;

  /** Master key id */
  uint32_t m_master_key_id{Encryption::DEFAULT_MASTER_KEY_ID};
};
#endif /* os0enc_h */
