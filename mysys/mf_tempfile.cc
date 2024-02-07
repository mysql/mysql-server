/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/mf_tempfile.cc
*/

#include "my_config.h"

#include <errno.h>
#ifdef HAVE_O_TMPFILE
#include <fcntl.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysys/mysys_priv.h"
#include "mysys_err.h"
#include "nulls.h"

#ifdef WIN32
#include <fcntl.h>  // O_EXCL
#include <inttypes.h>
#include <rpc.h>  // UUID API
#include <sstream>
#include <string>
/* UuidCreateSequential needs Rpcrt4 library */
#pragma comment(lib, "Rpcrt4")

static void encode_crockford_base32(uint64_t val_to_encode, char *to,
                                    size_t encode_len) {
  assert(encode_len < 14);
  // Using Douglas Crockford's base 32 encoding to reduce confusion
  // between similar letters/numbers and avoid accidental obscenities.
  // See https://www.crockford.com/base32.html
  constexpr char crockford_base32[] = "0123456789abcdefghjkmnpqrstvwxyz";
  // Encode just the least significant encode_len * 5 bits of val_to_encode,
  // starting from the most significant 5 bits
  const auto BITS_PER_CHAR = 5;
  assert(encode_len * BITS_PER_CHAR < 64);
  for (size_t i = encode_len; i--;) {
    to[i] = crockford_base32[(val_to_encode >> BITS_PER_CHAR * i) % 32];
  }
}

/*
get_encoded_uuid_excluding_mac returns true on success, false on failure.
On success, the to buffer contains 16 chars of an encoded UUID and a null
terminator, so the to_len must be at least 17.
*/
static bool get_encoded_uuid_excluding_mac(char *to, size_t to_len) {
  assert(to_len >= 17);
  if (to_len < 17) {
    return false;
  }
  UUID uuid;
  const auto uuid_created = UuidCreateSequential(&uuid);
  if (!((uuid_created == RPC_S_OK) ||
        (uuid_created == RPC_S_UUID_LOCAL_ONLY))) {
    return false;
  }
  /*
    The uuid returned by UuidCreateSequential is a version 1 UUID, as described
    in rfc4122. The rfc4122 structure maps to the UUID structure thus:
      unsigned long  Data1 is time-low
      unsigned short Data2 is time-mid
      unsigned short Data3 is time-high-and-version
      unsigned char  Data4[0] is clock-seq-and-reserved
      unsigned char  Data4[1] is clock-seq-low
      unsigned char  Data4[2..7] is node (MAC address)

      As the MAC address is (usually!) unchanging for a machine, it doesn't
      help to include it in a "random" temporary filename, so it is not
      included in the encoded uuid. Including a machine's MAC address in a
      temporary filename would also leak information unnecessarily.
  */
  encode_crockford_base32(
      uuid.Data1 + (static_cast<uint64_t>(uuid.Data4[0]) << 32), to, 8);
  encode_crockford_base32(uuid.Data2 +
                              (static_cast<uint64_t>(uuid.Data3) << 16) +
                              (static_cast<uint64_t>(uuid.Data4[1]) << 32),
                          to + 8, 8);
  to[16] = '\0';
  return true;
}

/*
  @brief
  Create a temporary file with unique name in a given directory

  @details
  create_temp_file_uuid
    to             pointer to buffer where temporary filename will be stored
    dir            directory where to create the file
    prefix         prefix the filename with up to a maximum of three chars
                   from this value.

  @return
    1 on success, 0 on failure.

  @note
    This function uses UuidCreateSequential to create a UUID value that is
    unique on this machine. The UUID will include a MAC address obtained from
    a network adapter where available. The inclusion of a (non-varying) MAC
    address in a temporary filename would be neither useful nor secure, so it is
    omitted from the base 32 encoding of the UUID value obtained from
    UuidCreateSequential.
    The use of UuidCreateSequential rather than UuidCreate (which does not
    include a MAC address in the generated UUID) is a compromise between
    uniqueness guarantees and performance.

    The use of only up to three characters of the prefix string on Windows is
    to retain some backward compatibility with earlier implementations
    which used the Windows function GetTempFilename.
*/

static int create_temp_file_uuid(char *to, const std::string &dir,
                                 const std::string &prefix) {
  // Verify that the dir path is not too long...
  const auto reduced_prefix =
      prefix.substr(0, std::min(prefix.length(), (size_t)3));

  const std::string dir_with_separator =
      is_directory_separator(dir.back()) ? dir : dir + FN_LIBCHAR;

  if (dir_with_separator.length() > (MAX_PATH - MY_MAX_TEMP_FILENAME_LEN)) {
    my_osmaperr(CO_E_PATHTOOLONG);
    set_my_errno(errno);
    return 0;
  }
  char encoded_uuid[17];
  if (!get_encoded_uuid_excluding_mac(encoded_uuid, sizeof(encoded_uuid))) {
    return 0;
  }
  // Finally, assemble the filename...
  std::ostringstream full_path_builder;
  full_path_builder << dir_with_separator << reduced_prefix << encoded_uuid;
  strcpy(to, full_path_builder.str().c_str());

  HANDLE hfile = CreateFile(to, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hfile == INVALID_HANDLE_VALUE) {
    my_osmaperr(GetLastError());
    set_my_errno(errno);
    return 0;
  }
  if (!CloseHandle(hfile)) {
    my_osmaperr(GetLastError());
    set_my_errno(errno);
    return 0;
  }
  return 1;
}
#endif  // WIN32
/*
  @brief
  Create a temporary file with unique name in a given directory

  @details
  create_temp_file
    to             pointer to buffer where temporary filename will be stored
    dir            directory where to create the file
    prefix         prefix the filename with this (only up to the first three
                   chars will be used on Windows).
    mode           Flags to use for my_create/my_open
    MyFlags        Magic flags

  @return
    File descriptor of opened file if success
    -1 and sets errno if fails.

  @note
    The behaviour of this function differs a lot between
    implementation, it's main use is to generate a file with
    a name that does not already exist.

    The implementation using mkstemp should be considered the
    reference implementation when adding a new or modifying an
    existing one

*/

File create_temp_file(char *to, const char *dir, const char *prefix,
                      int mode [[maybe_unused]],
                      UnlinkOrKeepFile unlink_or_keep,
                      myf MyFlags [[maybe_unused]]) {
  File file = -1;
#ifdef _WIN32
  TCHAR path_buf[MAX_PATH + 1];
#endif

  DBUG_TRACE;
  DBUG_PRINT("enter", ("dir: %s, prefix: %s", dir, prefix));
#if defined(_WIN32)

  if (!dir) {
    auto res = GetTempPath(sizeof(path_buf), path_buf);
    if (res == 0) {
      // There's been a problem with GetTempPath
      my_osmaperr(GetLastError());
      set_my_errno(errno);
      return -1;
    }
    if (res > sizeof(path_buf)) {
      my_osmaperr(CO_E_PATHTOOLONG);
      set_my_errno(errno);
      return -1;
    }
    dir = path_buf;
  }
  // Treat null prefix as an empty string
  if (!prefix) {
    prefix = "";
  }
  /*
    Use create_temp_file_uuid to generate a unique filename, create
    the file and release its handle
     - uses up to the first three letters from prefix
  */
  if (create_temp_file_uuid(to, dir, prefix) == 0) return -1;

  DBUG_PRINT("info", ("name: %s", to));

  /*
    Open the file without the "open only if file doesn't already exist"
    since the file has already been created by create_temp_file_uuid
  */
  if ((file = my_open(to, (mode & ~O_EXCL), MyFlags)) < 0) {
    /* Open failed, remove the file created by GetTempFileName */
    const int tmp = my_errno();
    (void)my_delete(to, MYF(0));
    set_my_errno(tmp);
    return file;
  }
  if (unlink_or_keep == UNLINK_FILE) {
    my_delete(to, MYF(0));
  }

#else /* mkstemp() is available on all non-Windows supported platforms. */
#ifdef HAVE_O_TMPFILE
  if (unlink_or_keep == UNLINK_FILE) {
    if (!dir && !(dir = getenv("TMPDIR"))) dir = DEFAULT_TMPDIR;

    char dirname_buf[FN_REFLEN];
    convert_dirname(dirname_buf, dir, nullptr);

    // Verify that the generated filename will fit in a FN_REFLEN size buffer.
    int max_filename_len = snprintf(to, FN_REFLEN, "%s%.20sfd=%d", dirname_buf,
                                    prefix ? prefix : "tmp.", 4 * 1024 * 1024);
    if (max_filename_len >= FN_REFLEN) {
      errno = ENAMETOOLONG;
      set_my_errno(ENAMETOOLONG);
      return file;
    }

    /* Explicitly don't use O_EXCL here as it has a different
       meaning with O_TMPFILE.
    */
    file = mysys_priv::RetryOnEintr(
        [&]() {
          return open(dirname_buf, O_RDWR | O_TMPFILE | O_CLOEXEC,
                      S_IRUSR | S_IWUSR);
        },
        -1);

    if (file >= 0) {
      sprintf(to, "%s%.20sfd=%d", dirname_buf, prefix ? prefix : "tmp.", file);
      file_info::RegisterFilename(file, to,
                                  file_info::OpenType::FILE_BY_O_TMPFILE);
    }
  }
  // Fall through, in case open() failed above (or we have KEEP_FILE).
#endif /* HAVE_O_TMPFILE */
  if (file == -1) {
    char prefix_buff[30];
    uint pfx_len;

    pfx_len = (uint)(my_stpcpy(my_stpnmov(prefix_buff, prefix ? prefix : "tmp.",
                                          sizeof(prefix_buff) - 7),
                               "XXXXXX") -
                     prefix_buff);
    if (!dir && !(dir = getenv("TMPDIR"))) dir = DEFAULT_TMPDIR;
    if (strlen(dir) + pfx_len > FN_REFLEN - 2) {
      errno = ENAMETOOLONG;
      set_my_errno(ENAMETOOLONG);
      return file;
    }
    my_stpcpy(convert_dirname(to, dir, NullS), prefix_buff);
    file = mkstemp(to);
    if (file < 0) {
      set_my_errno(errno);
      if (MyFlags & (MY_FAE | MY_WME)) {
        MyOsError(my_errno(), EE_CANTCREATEFILE, MYF(0), to);
      }
      return file;
    }
    file_info::RegisterFilename(file, to, file_info::OpenType::FILE_BY_MKSTEMP);
    if (unlink_or_keep == UNLINK_FILE) {
      unlink(to);
    }
  }
#endif /* _WIN32 */
  if (file >= 0) {
    mysql_mutex_lock(&THR_LOCK_open);
    my_tmp_file_created++;
    mysql_mutex_unlock(&THR_LOCK_open);
  }
  return file;
}
