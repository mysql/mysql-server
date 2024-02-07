/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include "util/require.h"

#include "kernel/signaldata/FsOpenReq.hpp"
#include "my_getopt.h"
#include "portlib/ndb_file.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_opts.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_file.h"
#include "util/ndbxfrm_iterator.h"

using byte = unsigned char;

static int g_compress = 0;

static ndb_password_state opt_decrypt_password_state("decrypt", nullptr);
static ndb_password_option opt_decrypt_password(opt_decrypt_password_state);
static ndb_password_from_stdin_option opt_decrypt_password_from_stdin(
    opt_decrypt_password_state);

static ndb_password_state opt_encrypt_password_state("encrypt", nullptr);
static ndb_password_option opt_encrypt_password(opt_encrypt_password_state);
static ndb_password_from_stdin_option opt_encrypt_password_from_stdin(
    opt_encrypt_password_state);

static ndb_key_state opt_decrypt_key_state("decrypt", nullptr);
static ndb_key_option opt_decrypt_key(opt_decrypt_key_state);
static ndb_key_from_stdin_option opt_decrypt_key_from_stdin(
    opt_decrypt_key_state);

static ndb_key_state opt_encrypt_key_state("encrypt", nullptr);
static ndb_key_option opt_encrypt_key(opt_encrypt_key_state);
static ndb_key_from_stdin_option opt_encrypt_key_from_stdin(
    opt_encrypt_key_state);

static int g_info = 0;
static int g_detailed_info = 0;
static int g_encrypt_block_size = 0;
static int g_encrypt_cipher = ndb_ndbxfrm1::cipher_cbc;
static int g_encrypt_kdf_iter_count =
    -1;  // ndb_openssl_evp::DEFAULT_KDF_ITER_COUNT;
static int g_file_block_size = 512;
#if defined(TODO_READ_REVERSE)
static int g_read_reverse = 0;
#endif

// clang-format off
static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,

  // Specific options
  { "compress", 'c', "Compress file",
    &g_compress, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "decrypt-key", NDB_OPT_NOSHORT, "Decryption key",
    nullptr, nullptr, 0, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_decrypt_key},
  { "decrypt-key-from-stdin", NDB_OPT_NOSHORT, "Decryption key",
    &opt_decrypt_key_from_stdin.opt_value, nullptr, 0, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, &opt_decrypt_key_from_stdin},
  { "decrypt-password", NDB_OPT_NOSHORT, "Decryption password",
    nullptr, nullptr, 0, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_decrypt_password},
  { "decrypt-password-from-stdin", NDB_OPT_NOSHORT, "Decryption password",
    &opt_decrypt_password_from_stdin.opt_value, nullptr, 0, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, &opt_decrypt_password_from_stdin},
  { "encrypt-block-size", NO_ARG,
    "Size of input data chunks that are encrypted as an unit. Used with XTS, "
    "zero for CBC mode.",
    &g_encrypt_block_size, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, INT_MAX, nullptr, 0, nullptr },
  { "encrypt-cipher", NO_ARG, "Encrypt cipher: CBC(1), XTS(2).",
    &g_encrypt_cipher, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    ndb_ndbxfrm1::cipher_cbc, 0, INT_MAX, nullptr, 0, nullptr },
  { "encrypt-kdf-iter-count", 'k', "Iteration count to used in key definition",
    &g_encrypt_kdf_iter_count, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    ndb_openssl_evp::DEFAULT_KDF_ITER_COUNT, 0, INT_MAX, nullptr, 0, nullptr },
  { "encrypt-key", NDB_OPT_NOSHORT, "Encryption key",
    nullptr, nullptr, nullptr, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_encrypt_key},
  { "encrypt-key-from-stdin", NDB_OPT_NOSHORT, "Encryption key",
    &opt_encrypt_key_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_encrypt_key_from_stdin},
  { "encrypt-password", NDB_OPT_NOSHORT, "Encryption password",
    nullptr, nullptr, nullptr, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_encrypt_password},
  { "encrypt-password-from-stdin", NDB_OPT_NOSHORT, "Encryption password",
    &opt_encrypt_password_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_encrypt_password_from_stdin},
  { "file-block-size", NO_ARG, "File block size.",
    &g_file_block_size, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    512, 0, INT_MAX, nullptr, 0, nullptr },
  { "info", 'i', "Print info about file",
    &g_info, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
    { "detailed-info", NDB_OPT_NOSHORT, "Print info about file including file header and trailer",
    &g_detailed_info, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
#if defined(TODO_READ_REVERSE)
  { "read-reverse", 'R', "Read file in reverse",
    &g_read_reverse, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
#endif
  NdbStdOpt::end_of_options
};
// clang-format on

static const char *load_defaults_groups[] = {"ndbxfrm", nullptr};

static int dump_info(const char name[], bool print_header_and_trailer);
static int copy_file(const char src[], const char dst[]);

int main(int argc, char *argv[]) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  if (opts.handle_options()) return 2;

#if defined(TODO_READ_REVERSE)
  bool do_write = (g_compress || (opt_encrypt_password != nullptr));

  if (g_read_reverse && do_write) {
    fprintf(stderr,
            "Error: Writing with encryption (--encrypt-password) or "
            "compression (--compress) not allowed when reading in reverse "
            "(--read-reverse).\n");
    return 1;
  }

  if (g_info || g_detailed_info) && (do_write || g_read_reverse))
  {
      fprintf(stderr,
              "Error: Writing (--encrypt-password, --compress) or reading "
              "(--read-reverse) file not allowed in connection with --info.\n");
      return 1;
    }
#endif

  if (ndb_option::post_process_options()) {
    BaseString err_msg = opt_decrypt_key_state.get_error_message();
    if (!err_msg.empty()) {
      fprintf(stderr, "Error: %s\n", err_msg.c_str());
    }
    err_msg = opt_decrypt_password_state.get_error_message();
    if (!err_msg.empty()) {
      fprintf(stderr, "Error: %s\n", err_msg.c_str());
    }
    err_msg = opt_encrypt_key_state.get_error_message();
    if (!err_msg.empty()) {
      fprintf(stderr, "Error: %s\n", err_msg.c_str());
    }
    err_msg = opt_encrypt_password_state.get_error_message();
    if (!err_msg.empty()) {
      fprintf(stderr, "Error: %s\n", err_msg.c_str());
    }
    return 2;
  }
  if (opt_decrypt_key_state.get_key() != nullptr &&
      opt_decrypt_password_state.get_password() != nullptr) {
    fprintf(stderr, "Error: Both decrypt key and decrypt password is set.\n");
    return 2;
  }
  if (opt_encrypt_key_state.get_key() != nullptr &&
      opt_encrypt_password_state.get_password() != nullptr) {
    fprintf(stderr, "Error: Both encrypt key and encrypt password is set.\n");
    return 2;
  }
  if ((opt_decrypt_key_state.get_key() != nullptr ||
       opt_encrypt_key_state.get_key() != nullptr) &&
      !ndb_openssl_evp::is_aeskw256_supported()) {
    fprintf(stderr,
            "Error: decrypt and encrypt key options requires OpenSSL 1.0.2 "
            "or newer.\n");
    return 2;
  }

  if (g_detailed_info) {
    for (int argi = 0; argi < argc; argi++) {
      dump_info(argv[argi], true);
    }
    return 0;
  }

  if (g_info) {
    for (int argi = 0; argi < argc; argi++) {
      dump_info(argv[argi], false);
    }
    return 0;
  }

  if (g_file_block_size < 0) {
    fprintf(stderr, "Error: file_block_size %d can not be negative.\n",
            g_file_block_size);
    return 1;
  }

  if (argc != 2) {
    fprintf(stderr, "Error: Need one source file and one destination file.\n");
    return 1;
  }

  ndb_openssl_evp::library_init();

  int rc = copy_file(argv[0], argv[1]);

  ndb_openssl_evp::library_end();

  return rc;
}

int dump_info(const char name[], bool print_header_and_trailer) {
  ndb_file file;
  ndbxfrm_file xfrm;
  int r;

  r = file.open(name, FsOpenReq::OM_READONLY);
  if (r == -1) {
    fprintf(stderr, "Error: Could not open file '%s' for read.\n", name);
    return 1;
  }
  ndb_ndbxfrm1::header header;
  ndb_ndbxfrm1::trailer trailer;
  r = xfrm.read_header_and_trailer(file, header, trailer);
  if (r == -1) {
    fprintf(stderr, "Error: Could not read file '%s'.\n", name);
    return 1;
  }
  require(r == 0);

  Uint32 cipher = 0;
  header.get_encryption_cipher(&cipher);
  bool is_compressed = (header.get_compression_method() != 0);
  bool is_encrypted = (cipher != 0);
  printf("File=%s, compression=%s, encryption=%s\n", name,
         is_compressed ? "yes" : "no", is_encrypted ? "yes" : "no");

  if (print_header_and_trailer) {
    header.printf(stdout);
    trailer.printf(stdout);
  }

  file.close();
  return 0;
}

int copy_file(const char src[], const char dst[]) {
  ndb_file src_file;
  ndb_file dst_file;

  int r;

  if (dst_file.create(dst) != 0) {
    fprintf(stderr, "Error: Could not create file '%s'.\n", dst);
    perror(dst);
    return 1;  // File exists?
  }

  r = src_file.open(src, FsOpenReq::OM_READONLY);
  if (r == -1) {
    fprintf(stderr, "Error: Could not open file '%s' for read.\n", src);
    perror(src);
    dst_file.remove(dst);
    return 1;
  }

  r = dst_file.open(dst, FsOpenReq::OM_WRITEONLY);
  if (r == -1) {
    fprintf(stderr, "Error: Could not open file '%s' for write.\n", dst);
    perror(dst);
    src_file.close();
    dst_file.remove(dst);
    return 1;
  }

  ndbxfrm_file src_xfrm;
  ndbxfrm_file dst_xfrm;

  const byte *src_pwd_key =
      opt_decrypt_key_state.get_key() != nullptr
          ? opt_decrypt_key_state.get_key()
          : reinterpret_cast<const byte *>(
                opt_decrypt_password_state.get_password());
  const size_t src_pwd_key_len =
      opt_decrypt_key_state.get_key() != nullptr
          ? opt_decrypt_key_state.get_key_length()
          : opt_decrypt_password_state.get_password_length();
  r = src_xfrm.open(src_file, src_pwd_key, src_pwd_key_len);
  if (r == -1) {
    fprintf(stderr, "Error: Can not read file %s, bad password or key?\n", src);
    src_file.close();
    dst_file.close();
    dst_file.remove(dst);
    return 1;
  }

  require(g_file_block_size >= 0);
  size_t file_block_size = g_file_block_size;

  const byte *dst_pwd_key =
      opt_encrypt_key_state.get_key() != nullptr
          ? opt_encrypt_key_state.get_key()
          : reinterpret_cast<const byte *>(
                opt_encrypt_password_state.get_password());
  const size_t dst_pwd_key_len =
      opt_encrypt_key_state.get_key() != nullptr
          ? opt_encrypt_key_state.get_key_length()
          : opt_encrypt_password_state.get_password_length();
  const Uint64 file_size = src_file.get_size();
  r = dst_xfrm.create(dst_file, g_compress, dst_pwd_key, dst_pwd_key_len,
                      g_encrypt_kdf_iter_count, g_encrypt_cipher,
                      -1 /* key count */, g_encrypt_block_size, file_block_size,
                      file_size, true);
  if (r != 0) {
    fprintf(stderr, "Error: Can not initialize file %s.\n", dst);
    src_xfrm.close(true);
    src_file.close();
    dst_file.close();
    dst_file.remove(dst);
    return 1;
  }

  // Copy data

  ndbxfrm_buffer buffer;
  buffer.init();
  for (;;) {
    ndbxfrm_input_iterator wr_it = buffer.get_input_iterator();
    if (dst_xfrm.write_forward(&wr_it) == -1) {
      fprintf(stderr, "Error: Can not write file %s.\n", dst);
      r = 2;  // write failure
      break;
    }
    buffer.update_read(wr_it);
    buffer.rebase(0);

    if (buffer.last() && buffer.read_size() == 0) {
      break;  // All read and written
    }

    ndbxfrm_output_iterator rd_it = buffer.get_output_iterator();
    if (src_xfrm.read_forward(&rd_it) == -1) {
      if (src_xfrm.is_encrypted()) {
        fprintf(stderr, "Error: Can not read file %s, bad password or key?\n",
                src);
      } else {
        fprintf(stderr, "Error: Can not read file %s.\n", src);
      }
      r = 2;  // read failure
      break;
    }
    buffer.update_write(rd_it);
  }

  if (r != 0) {
    src_xfrm.close(true);
    src_file.close();
  } else {
    r = src_xfrm.close(false);
    if (r != 0) {
      if (src_xfrm.is_encrypted()) {
        fprintf(stderr, "Error: Can not read file %s, bad password or key?\n",
                src);
      } else {
        fprintf(stderr, "Error: Can not read file %s.\n", src);
      }
      r = 2;  // read failure
    }
    if (src_file.close() != 0 && r == 0) {
      fprintf(stderr, "Error: Can not read file %s.\n", src);
      r = 2;  // read failure
    }
  }
  dst_xfrm.close((r != 0));

  dst_file.sync();
  dst_file.close();

  if (r != 0) {
    dst_file.remove(dst);
  }

  return r;
}
