/* Copyright (c) 2020, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UTIL_NDB_OPENSSL_EVP
#define UTIL_NDB_OPENSSL_EVP

#include <assert.h> // assert()
#include <stdlib.h> // abort()
#include <string.h>

#include <new>

#include "openssl/evp.h"
#include "openssl/rand.h"

#include "util/ndbxfrm_iterator.h"
#include "ndb_global.h"

class ndb_openssl_evp
{
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
  static constexpr size_t BLOCK_LEN = 16;
  static constexpr size_t KEY_LEN = 32;
  static constexpr size_t IV_LEN = 32;
  static constexpr size_t SALT_LEN = 32;
  static constexpr size_t CBC_KEY_LEN = 32;
  static constexpr size_t CBC_IV_LEN = 16;
  static constexpr size_t CBC_BLOCK_LEN = 16;
  static constexpr size_t XTS_KEY_LEN = 64;
  static constexpr size_t XTS_IV_LEN = 16;
  static constexpr size_t XTS_BLOCK_LEN = 1;
  static_assert(KEY_LEN + IV_LEN == XTS_KEY_LEN, "xts uses double key length");
  static_assert(KEY_LEN == CBC_KEY_LEN, "");
  static_assert(CBC_IV_LEN <= IV_LEN, "");

  static int library_init();
  static int library_end();

  ndb_openssl_evp();
  ~ndb_openssl_evp();
  int reset();

  int set_memory(void* mem, size_t size); // sets m_key_iv_set

  int set_aes_256_cbc(bool padding, size_t data_unit_size);
  int set_aes_256_xts(size_t data_unit_size);
  int generate_salt256(byte salt[SALT_LEN]);
  int derive_and_add_key_iv_pair(const byte pwd[],
                                 size_t pwd_len,
                                 size_t iter_count,
                                 const byte salt[SALT_LEN]);
  int remove_all_key_iv_pairs();
private:

  const EVP_CIPHER *m_evp_cipher;
  bool m_padding; // used by cbc, should be false for xts
  bool m_has_key_iv;
  bool m_mix_key_iv_pair;
  size_t m_data_unit_size; // used by xts, typically 512B, should be 0 for cbc.
  byte m_key_iv[KEY_LEN + IV_LEN];
  key256_iv256_set* m_key_iv_set; // if nullptr use m_key/m_iv
};

class ndb_openssl_evp::key256_iv256_set
{
public:
  key256_iv256_set();
  ~key256_iv256_set();
  int clear();
  int get_next_key_iv_slot(byte **key_iv);
  int commit_next_key_iv_slot();
  int get_key_iv_pair(size_t index, const byte **key, const byte **iv) const;
  int get_key_iv_mixed_pair(size_t index, const byte **key, const byte **iv) const;
private:
  size_t m_key_iv_count;
  /*
   * Keep key-iv pair usage less than 32768-512 bytes to let file header have
   * 512 bytes non-key-iv data and still have keys within first 32768 bytes.
   * 500 pairs with 32MiB data units give "unique" keys for
   * 500x500x32MiB = 8MMiB = ~8TiB
   * For XTS m_key_iv is used as double length key.
   * For CBC key is at beginning and iv at end.
   */
  struct { byte m_key_iv[KEY_LEN + IV_LEN]; } m_key_iv[500];
};
static_assert(sizeof(ndb_openssl_evp::key256_iv256_set) <= 32768 - 512, "");

class ndb_openssl_evp::operation
{
public:
  operation(ndb_openssl_evp* context);
  ~operation();

  int setup_key_iv(off_t input_position, const byte **key, const byte **iv, byte xts_seq_num[16]);
  int setup_encrypt_key_iv(off_t input_position);
  int setup_decrypt_key_iv(off_t input_position, const byte* iv_=nullptr);

  int encrypt_init(off_t output_position, off_t input_position);
  int encrypt(output_iterator* out, input_iterator* in);
  int encrypt_end();

  int decrypt_init(off_t output_position, off_t input_position);
  int decrypt_init_reverse(off_t output_position, off_t input_position);
  int decrypt(output_iterator* out, input_iterator* in);
  int decrypt_reverse(output_reverse_iterator* out, input_reverse_iterator* in);
  int decrypt_end();

  off_t get_input_position() const { return m_input_position; }
  off_t get_output_position() const { return m_output_position; }
private:
  enum operation_mode { NO_OP, ENCRYPT, DECRYPT };
  operation_mode m_op_mode;
  bool m_reverse;
  bool m_at_padding_end;

  off_t m_input_position;
  off_t m_output_position;
  ndb_openssl_evp* m_context;
  EVP_CIPHER_CTX *m_evp_context;
};

#endif
