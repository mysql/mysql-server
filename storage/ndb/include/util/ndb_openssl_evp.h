/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UTIL_NDB_OPENSSL_EVP
#define UTIL_NDB_OPENSSL_EVP

#include <assert.h>  // assert()
#include <stdlib.h>  // abort()
#include <string.h>

#include <new>

#include "openssl/evp.h"
#include "openssl/rand.h"

#include "ndb_global.h"
#include "util/ndbxfrm_iterator.h"

class ndb_openssl_evp {
 public:
  using byte = unsigned char;
  using input_iterator = ndbxfrm_input_iterator;
  using output_iterator = ndbxfrm_output_iterator;
  using input_reverse_iterator = ndbxfrm_input_reverse_iterator;
  using output_reverse_iterator = ndbxfrm_output_reverse_iterator;

  class operation;
  class key256_iv256_set;

  static constexpr int DEFAULT_KDF_ITER_COUNT = 100000;
  static constexpr size_t MEMORY_NEED = 32768;
  static constexpr size_t MEMORY_ALIGN = alignof(size_t);
  static constexpr size_t BLOCK_LEN = 16;
  static constexpr size_t KEY_LEN = 32;
  static constexpr size_t IV_LEN = 32;
  /*
   * The data unit size is the amount of data that is encrypted with the same
   * key and IV.
   *
   * Note that for XTS what we store and call key_iv pair is the two keys, but
   * when encrypting a data chunk one also use a 16 bit sector number as IV
   * calculated from the data position.
   *
   * The typical data unit size in Ndb is 32768 bytes since that is the typical
   * page size.
   *
   * The choice of UINT32_MAX as max data unit size serves two purposes:
   *
   *   - it works also for 32-bit platforms.
   *   - the data size per key_iv pair fits in a 64-bit signed int also for XTS
   *     (needs 48 bits). Which simplifies safe calculations in
   *     get_needed_key_iv_pair_count().
   */
  static constexpr size_t MAX_DATA_UNIT_SIZE = UINT32_MAX;
  /*
   * MAX_KEY_IV_COUNT is 511 to keep ndb_openssl_evp::key256_iv256_set within
   * 32KiB.
   */
  static constexpr size_t MAX_KEY_IV_COUNT = 511;
  static constexpr size_t SALT_LEN = 32;
  static constexpr size_t CBC_KEY_LEN = 32;
  static constexpr size_t CBC_IV_LEN = 16;
  static constexpr size_t CBC_BLOCK_LEN = 16;
  static constexpr size_t XTS_KEYS_LEN = 64;
  static constexpr size_t XTS_SEQNUM_LEN = 16;
  static constexpr size_t XTS_BLOCK_LEN = 1;
  static constexpr size_t AESKW_EXTRA = 8;
  static_assert(KEY_LEN + IV_LEN == XTS_KEYS_LEN, "xts uses double key length");
  static_assert(KEY_LEN == CBC_KEY_LEN);
  static_assert(CBC_IV_LEN <= IV_LEN);

  static int library_init();
  static int library_end();

  ndb_openssl_evp();
  ~ndb_openssl_evp();
  /*
   * For XTS the data unit is the smallest data block that can be decrypted and
   * defines the block size that could be randomly accessed.
   *
   * CBC-mode do not support random access, indicated by
   * random_access_block_size (and m_data_unit_size) being zero.
   */
  size_t get_random_access_block_size() const { return m_data_unit_size; }
  int reset();

  int set_memory(void *mem, size_t size);  // sets m_key_iv_set

  int set_aes_256_cbc(bool padding, size_t data_unit_size);
  int set_aes_256_xts(bool padding, size_t data_unit_size);

  // Set CBC or XTS mode first, prior call
  size_t get_needed_key_iv_pair_count(ndb_off_t estimated_data_size) const;
  static constexpr size_t get_pbkdf2_max_key_iv_pair_count(
      size_t keying_material_buffer_size);
  static constexpr size_t get_aeskw_max_key_iv_pair_count(
      size_t keying_material_buffer_size);

  int generate_salt256(byte salt[SALT_LEN]);
  int derive_and_add_key_iv_pair(const byte pwd[], size_t pwd_len,
                                 size_t iter_count, const byte salt[SALT_LEN]);
  int add_key_iv_pairs(const byte key_iv_pairs[], size_t pair_count,
                       size_t pair_size);
  int remove_all_key_iv_pairs();
  static int generate_key(byte key[], size_t key_len);
  static int wrap_keys_aeskw256(byte *wrapped, size_t *wrapped_size,
                                const byte *keys, size_t key_size,
                                const byte *wrapping_key,
                                size_t wrapping_key_size);
  static int unwrap_keys_aeskw256(byte *keys, size_t *key_size,
                                  const byte *wrapped, size_t wrapped_size,
                                  const byte *wrapping_key,
                                  size_t wrapping_key_size);

  static bool is_aeskw256_supported();

 private:
  const EVP_CIPHER *m_evp_cipher;
  bool m_padding;  // used by cbc, should be false for xts
  bool m_has_key_iv;
  bool m_mix_key_iv_pair;
  size_t m_data_unit_size;  // used by xts, typically 512B, should be 0 for cbc.
  byte m_key_iv[KEY_LEN + IV_LEN];
  key256_iv256_set *m_key_iv_set;  // if nullptr use m_key/m_iv
};

class ndb_openssl_evp::key256_iv256_set {
 public:
  key256_iv256_set();
  ~key256_iv256_set();
  int clear();
  int get_next_key_iv_slot(byte **key_iv);
  int commit_next_key_iv_slot();
  int get_key_iv_pair(size_t index, const byte **key, const byte **iv) const;
  int get_key_iv_mixed_pair(size_t index, const byte **key,
                            const byte **iv) const;

 private:
  size_t m_key_iv_count;
  /*
   * key256_iv256_set object should fit in a 32KiB page in memory.
   * Note both key and IV are 256 bits, although CBC will only use the first
   * 128 bits of IV. And XTS will use key as key1 and IV as key2.
   */
  struct {
    byte m_key_iv[KEY_LEN + IV_LEN];
  } m_key_iv[MAX_KEY_IV_COUNT];
};
static_assert(sizeof(ndb_openssl_evp::key256_iv256_set) <= 32768);
static_assert(alignof(ndb_openssl_evp::key256_iv256_set) ==
              ndb_openssl_evp::MEMORY_ALIGN);

class ndb_openssl_evp::operation {
 public:
  operation(const ndb_openssl_evp *context);
  operation();
  ~operation();
  void reset();
  int set_context(const ndb_openssl_evp *context);

  int setup_key_iv(ndb_off_t input_position, const byte **key, const byte **iv,
                   byte xts_seq_num[16]);
  int setup_encrypt_key_iv(ndb_off_t input_position);
  int setup_decrypt_key_iv(ndb_off_t input_position, const byte *iv_ = nullptr);

  int encrypt_init(ndb_off_t output_position, ndb_off_t input_position);
  int encrypt(output_iterator *out, input_iterator *in);
  int encrypt_end();

  int decrypt_init(ndb_off_t output_position, ndb_off_t input_position);
  int decrypt_init_reverse(ndb_off_t output_position, ndb_off_t input_position);
  int decrypt(output_iterator *out, input_iterator *in);
  int decrypt_reverse(output_reverse_iterator *out, input_reverse_iterator *in);
  int decrypt_end();

  ndb_off_t get_input_position() const { return m_input_position; }
  ndb_off_t get_output_position() const { return m_output_position; }

 private:
  enum operation_mode { NO_OP, ENCRYPT, DECRYPT };
  operation_mode m_op_mode;
  bool m_reverse;
  bool m_at_padding_end;

  ndb_off_t m_input_position;
  ndb_off_t m_output_position;
  const ndb_openssl_evp *m_context;
  EVP_CIPHER_CTX *m_evp_context;
  byte m_key_iv[KEY_LEN + IV_LEN];
};

constexpr size_t ndb_openssl_evp::get_pbkdf2_max_key_iv_pair_count(
    size_t keying_material_buffer_size) {
  return std::min<size_t>(MAX_KEY_IV_COUNT,
                          keying_material_buffer_size / SALT_LEN);
}

constexpr size_t ndb_openssl_evp::get_aeskw_max_key_iv_pair_count(
    size_t keying_material_buffer_size) {
  if (keying_material_buffer_size < 8) return 0;
  return std::min<size_t>(
      MAX_KEY_IV_COUNT, (keying_material_buffer_size - 8) / (KEY_LEN + IV_LEN));
}

#endif
