/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "util/ndbxfrm_padding.h"
#include "ndb_types.h"
#include "util/ndbxfrm_iterator.h"
#include "util/require.h"

void ndb_pkcs7_padding::set_padding(ndb_off_t data_size) {
  require(padding == 0);
  require(remaining == 0);
  remaining = padding = cipher_block_size - data_size % cipher_block_size;
}

bool ndb_pkcs7_padding::check_and_clear_padding(ndb_off_t data_size) {
  bool ok = true;
  if (remaining != 0) {
    remaining = 0;
    ok = false;
  } else if (padding != cipher_block_size - data_size % cipher_block_size) {
    ok = false;
  }
  padding = 0;
  return ok;
}

int ndb_pkcs7_padding::pad(ndbxfrm_buffer *buf) {
  auto out = buf->get_output_iterator();
  int ret = pad(&out);
  buf->update_write(out);
  return ret;
}

int ndb_pkcs7_padding::unpad(ndbxfrm_buffer *buf) {
  auto in = buf->get_input_iterator();
  int ret = unpad(&in);
  buf->update_read(in);
  return ret;
}

int ndb_pkcs7_padding::unpad_reverse(ndbxfrm_buffer *buf) {
  auto in = buf->get_input_reverse_iterator();
  int ret = unpad_reverse(&in);
  buf->update_reverse_read(in);
  return ret;
}

int ndb_pkcs7_padding::pad(ndbxfrm_output_iterator *out) {
  require(padding > 0);
  while (!out->empty() && remaining > 0) {
    *out->begin() = padding;
    out->advance(1);
    remaining--;
  }
  if (remaining == 0) {
    padding = 0;
    return 0;
  }
  return ndbxfrm_progress::have_more_output;
}

int ndb_pkcs7_padding::unpad(ndbxfrm_input_iterator *in) {
  if (padding == 0) {
    if (in->empty()) return ndbxfrm_progress::need_more_input;
    in->reduce(1);
    padding = *in->cend();
    if (padding == 0 || padding > cipher_block_size) return -1;
    require(padding > 0);
    require(padding <= cipher_block_size);
    remaining = padding;
    remaining--;
  }
  require(padding > 0);
  while (!in->empty() && remaining > 0) {
    in->reduce(1);
    if (*in->cend() != padding) return -1;
    remaining--;
  }
  if (remaining == 0) {
    return 0;
  }
  return ndbxfrm_progress::need_more_input;
}

int ndb_pkcs7_padding::unpad_reverse(ndbxfrm_input_reverse_iterator *in) {
  if (padding == 0) {
    if (in->empty()) return ndbxfrm_progress::need_more_input;
    in->advance(1);
    padding = *in->cbegin();
    require(padding > 0);
    require(padding <= cipher_block_size);
    remaining = padding;
    remaining--;
  }
  require(padding > 0);
  while (!in->empty() && remaining > 0) {
    in->advance(1);
    if (*in->cbegin() != padding) return -1;
    remaining--;
  }
  if (remaining == 0) {
    return 0;
  }
  return ndbxfrm_progress::need_more_input;
}

#ifdef TEST_NDBXFRM_PADDING

#include <algorithm>
#include "unittest/mytap/tap.h"

#define OK(cond) ok((cond), #cond)

int main() {
  plan(16);

  uint8_t buffer[100];
  for (unsigned i = 0; i < sizeof(buffer); i++) buffer[i] = '@' + i % 32;

  uint8_t original_data[std::size(buffer)];
  std::copy(buffer, std::end(buffer), original_data);

  ndb_pkcs7_padding padder;

  constexpr size_t data_size = 3;
  uint8_t pad_byte = ndb_pkcs7_padding::cipher_block_size - data_size;
  size_t pad_size;
  uint8_t *data_end = buffer + data_size;
  uint8_t *padded_end = buffer + data_size + pad_byte;

  ndbxfrm_output_iterator out = {data_end, std::end(buffer), false};
  padder.set_padding(data_size);
  OK(padder.pad(&out) == 0);
  OK(out.begin() == padded_end);
  pad_size = std::count(buffer + data_size, out.begin(), pad_byte);
  ok(pad_byte == pad_size, "pad: got pad size %zu expected %u", pad_size,
     pad_byte);
  bool data_intact =
      std::equal(original_data, original_data + data_size, buffer, data_end);
  ok(data_intact, "pad: got data '%.*s' expected '%.*s'", (int)data_size,
     original_data, (int)data_size, buffer);

  ndbxfrm_input_iterator in = ndbxfrm_input_iterator{buffer, padded_end, false};
  OK(padder.unpad(&in) == 0);
  ok(in.size() == data_size, "unpad: got unpadded input size %zu expected %zu",
     in.size(), data_size);
  pad_size = padded_end - in.cend();
  ok(pad_byte == pad_size, "unpad: got pad size %zu expected %u", pad_size,
     pad_byte);
  OK(padder.check_and_clear_padding(data_size));

  in = ndbxfrm_input_iterator{buffer, padded_end - 1, false};
  OK(padder.unpad(&in) == -1);
  OK(!padder.check_and_clear_padding(data_size));

  ndbxfrm_input_reverse_iterator rin = {padded_end, buffer, false};
  OK(padder.unpad_reverse(&rin) == 0);
  ok(rin.size() == data_size,
     "unpad_reverse: got unpadded input size %zu expected %zu", rin.size(),
     data_size);
  pad_size = padded_end - rin.cbegin();
  ok(pad_byte == pad_size, "unpad_reverse: got pad size %zu expected %u",
     pad_size, pad_byte);
  OK(padder.check_and_clear_padding(data_size));

  rin = {padded_end - 1, buffer, false};
  OK(padder.unpad_reverse(&rin) == -1);
  OK(!padder.check_and_clear_padding(data_size));

  return exit_status();
}

#endif
