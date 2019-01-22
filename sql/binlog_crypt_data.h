/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef BINLOG_CRYPT_DATA_H
#define BINLOG_CRYPT_DATA_H

#include "control_events.h"
#include "my_crypt.h"

class Binlog_crypt_data final {
 public:
  Binlog_crypt_data() noexcept;
  ~Binlog_crypt_data();
  Binlog_crypt_data(const Binlog_crypt_data &b);
  Binlog_crypt_data &operator=(Binlog_crypt_data b) noexcept;

  bool is_enabled() const noexcept { return enabled; }
  void disable() noexcept { enabled = false; }
  uchar *get_key() const noexcept { return key; }
  size_t get_keys_length() const noexcept { return key_length; }
  uint get_key_version() const noexcept { return key_version; }
  uint32_t get_offs() const noexcept { return offs; }

  void free_key(uchar *&key, size_t &key_length) noexcept;
  bool init(uint sch, uint kv, const uchar *nonce);
  bool init_with_loaded_key(uint sch, const uchar *nonce) noexcept;
  bool load_latest_binlog_key();
  void set_iv(uchar *iv, uint32 offs) const;

 private:
  uint key_version;
  size_t key_length;
  uchar *key;
  uchar nonce[binary_log::Start_encryption_event::NONCE_LENGTH];
  uint dst_len;
  uchar iv[binary_log::Start_encryption_event::IV_LENGTH];
  bool enabled;
  uint scheme;
  uint32_t offs;
};

#endif  // BINLOG_CRYPT_DATA_H
