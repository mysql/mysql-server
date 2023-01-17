/*
   Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
*/

#include "config.h" // WORDS_BIGENDIAN
#include "util/require.h"
#include <stdio.h>
#include <string.h>

#include "kernel/signaldata/FsOpenReq.hpp"
#include "my_getopt.h"
#include "portlib/ndb_file.h"
#include "util/ndb_opts.h"
#include "util/ndbxfrm_iterator.h"
#include "util/ndbxfrm_file.h"
#include "util/ndb_ndbxfrm1.h" // ndb_ndbxfrm1::toggle_endian
#include "util/ndb_openssl_evp.h"

using byte = unsigned char;

static ndb_password_state opt_filesystem_password_state("filesystem", nullptr);
static ndb_password_option opt_filesystem_password(opt_filesystem_password_state);
static ndb_password_from_stdin_option opt_filesystem_password_from_stdin(
        opt_filesystem_password_state);

static constexpr Uint32 max_buffer_size = 512;

int read_secrets_file(const char filename[]);

// clang-format off
static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,

  // Specific options
  { "filesystem-password", NDB_OPT_NOSHORT, "Filesystem password",
    nullptr, nullptr, 0, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_filesystem_password},
  {"filesystem-password-from-stdin", NDB_OPT_NOSHORT, "Filesystem password",
   &opt_filesystem_password_from_stdin.opt_value, nullptr, 0, GET_BOOL, NO_ARG,
   0, 0, 0, nullptr, 0, &opt_filesystem_password_from_stdin},
  NdbStdOpt::end_of_options
};
// clang-format on

static const char* load_defaults_groups[] = { "ndb_secretsfile_reader", nullptr };

int main(int argc, char* argv[])
{
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  if (opts.handle_options())
    return 2;

  if (ndb_option::post_process_options())
  {
    BaseString err_msg = opt_filesystem_password_state.get_error_message();
    if (!err_msg.empty())
    {
      fprintf(stderr, "Error: %s\n", err_msg.c_str());
    }
    return 2;
  }

  if (argc != 1)
  {
    fprintf(stderr, "Error: Need a secrets file as argument.");
    return 1;
  }

  ndb_openssl_evp::library_init();
  int rc = read_secrets_file(argv[0]);
  ndb_openssl_evp::library_end();
  return rc;
}

int read_secrets_file(const char filename[])
{
  ndb_file src_file;
  int r;

  r = src_file.open(filename, FsOpenReq::OM_READONLY);
  if (r == -1)
  {
    fprintf(stderr,
            "Error: Could not open secrets file '%s' for read.\n",
            filename);
    perror(filename);
    return 1;
  }

  ndbxfrm_file x_file;
  const byte* pwd = reinterpret_cast<const byte*>
  (opt_filesystem_password_state.get_password());
  const size_t pwd_len = opt_filesystem_password_state.get_password_length();
  r = x_file.open(src_file, pwd, pwd_len);
  if (r < 0)
  {
    fprintf(stderr, "Error: Failed to read secrets file.\n");
    src_file.close();
    return 1;
  }
  if(!x_file.is_encrypted())
  {
    fprintf(stdout, "Warning: Trying to read unencrypted file. "
                    "Secretsfile should be encrypted.\n");
  }

  byte buffer[max_buffer_size];
  ndbxfrm_output_iterator it = { buffer, buffer  + max_buffer_size,false };
  r = x_file.read_forward(&it);
  if (r < 0)
  {
    fprintf(stderr, "Error: Failed to read secrets file.\n");
    x_file.close(true);
    src_file.close();
    return 1;
  }

  Uint32 bytes_read = it.begin() - buffer;
  Uint32 bytes_available = bytes_read;

  if(bytes_available < 8)
  {
    fprintf(stderr, "Error: Failed to read secrets file, "
                    "invalid MAGIC\n");
    x_file.close(true);
    src_file.close();
    return 1;
  }
  bytes_available -= 8;
  if (memcmp(buffer, "NDBSCRT1", 8) != 0)
  {
    fprintf(stderr, "Error: Failed to read secrets file using the "
                         "provided filesystem password (wrong password?)\n");
    x_file.close(true);
    src_file.close();
    return 1;
  }

  r = x_file.close(false);
  if(r!=0)
  {
       fprintf(stderr, "Error: Invalid secretsfile, "
                       "checksum validation failed (wrong password?)\n");
       src_file.close();
       return 1;
  }

  if(bytes_available < 4)
  {
    fprintf(stderr, "Error: Failed to read secrets file, "
                    "unable to read KEY length\n");
    x_file.close(true);
    src_file.close();
    return 1;
  }
  bytes_available -= 4;

  Uint32 key_len;
  memcpy(&key_len, &buffer[8], 4);
#ifdef WORDS_BIGENDIAN
  // key length is always stored in little endian
  ndb_ndbxfrm1::toggle_endian32(&key_len);
#endif
  if(bytes_available < key_len)
  {
    fprintf(stderr, "Error: Failed to read secrets file, "
                    "unable to read KEY\n");
    x_file.close(true);
    src_file.close();
    return 1;
  }
  for (Uint32 i = 12; i < key_len+12; i++)
  {
    printf("%02x", buffer[i]);
  }
  printf("\n");

  src_file.close();
  return 0;
}
