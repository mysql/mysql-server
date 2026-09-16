/* Copyright (c) 2012, 2026, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <gtest/gtest.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_file.h"

#if defined(_WIN32)
namespace {

// Replace this cast with FileDispositionInfoEx when the minimum target reaches
// Windows 10 version 1607 / Windows Server 2016 (NTDDI_WIN10_RS1).
constexpr auto kFileDispositionInfoEx =
    static_cast<FILE_INFO_BY_HANDLE_CLASS>(21);

/** Owns two CRT streams that share deletion of the same temporary file. */
class FreopenTestFile {
 public:
  bool create() {
    if (GetTempFileNameA(DATA_DIR, "mfr", 0, m_path) == 0) return false;

    for (FILE *&stream : m_streams) {
      HANDLE handle =
          CreateFileA(m_path, GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle == INVALID_HANDLE_VALUE) return false;

      const int fd =
          _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDWR);
      if (fd == -1) {
        CloseHandle(handle);
        return false;
      }

      stream = _fdopen(fd, "r+");
      if (stream == nullptr) {
        _close(fd);
        return false;
      }
    }
    return true;
  }

  ~FreopenTestFile() {
    for (FILE *stream : m_streams) {
      if (stream != nullptr) fclose(stream);
    }
    if (m_path[0] != '\0') DeleteFileA(m_path);
  }

  const char *path() const { return m_path; }
  FILE *stream(size_t index) const { return m_streams[index]; }

  bool mark_legacy_delete_pending() const {
    HANDLE handle = CreateFileA(
        m_path, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    FILE_DISPOSITION_INFO info{TRUE};
    const bool marked = SetFileInformationByHandle(handle, FileDispositionInfo,
                                                   &info, sizeof(info)) != 0;
    CloseHandle(handle);
    return marked;
  }

  bool mark_posix_delete_pending() const {
    HANDLE handle = CreateFileA(
        m_path, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    FILE_DISPOSITION_INFO_EX info{FILE_DISPOSITION_FLAG_DELETE |
                                  FILE_DISPOSITION_FLAG_POSIX_SEMANTICS};
    const bool marked =
        SetFileInformationByHandle(handle, kFileDispositionInfoEx, &info,
                                   sizeof(info)) != 0;
    CloseHandle(handle);
    return marked;
  }

  static bool is_stream_valid(FILE *stream) { return os_handle(stream) != -1; }

  /** Return true if stream refers to a disk file rather than NUL. */
  static bool is_stream_backed_by_disk_file(FILE *stream) {
    const intptr_t handle = os_handle(stream);
    return handle != -1 &&
           GetFileType(reinterpret_cast<HANDLE>(handle)) == FILE_TYPE_DISK;
  }

 private:
  static intptr_t os_handle(FILE *stream) {
    const int fd = _fileno(stream);
    return fd < 0 ? -1 : _get_osfhandle(fd);
  }

  char m_path[MAX_PATH]{};
  FILE *m_streams[2]{};
};

TEST(FileUtilsTest, FreopenAfterLegacyDelete) {
  FreopenTestFile file;
  ASSERT_TRUE(file.create()) << GetLastError();
  ASSERT_TRUE(file.mark_legacy_delete_pending()) << GetLastError();

  // With legacy deletion, the name remains unavailable until both original
  // handles are released. The failed reopen attempts must detach the handles
  // without invalidating their CRT descriptors.
  EXPECT_EQ(nullptr, my_freopen(file.path(), "a", file.stream(0)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(0)));
  EXPECT_FALSE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(0)));
  EXPECT_EQ(nullptr, my_freopen(file.path(), "a", file.stream(1)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(1)));
  EXPECT_FALSE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(1)));

  // Once both old handles are detached, deletion completes and the retry can
  // create and reopen the file for both streams.
  EXPECT_EQ(file.stream(0), my_freopen(file.path(), "a", file.stream(0)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(0)));
  EXPECT_TRUE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(0)));
  EXPECT_EQ(file.stream(1), my_freopen(file.path(), "a", file.stream(1)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(1)));
  EXPECT_TRUE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(1)));
}

TEST(FileUtilsTest, FreopenAfterPosixDelete) {
  FreopenTestFile file;
  ASSERT_TRUE(file.create()) << GetLastError();
  ASSERT_TRUE(file.mark_posix_delete_pending()) << GetLastError();

  // POSIX deletion removes the name while the original handles remain open.
  // The first reopen creates a new file and the second opens that new file.
  EXPECT_EQ(file.stream(0), my_freopen(file.path(), "a", file.stream(0)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(0)));
  EXPECT_TRUE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(0)));
  EXPECT_EQ(file.stream(1), my_freopen(file.path(), "a", file.stream(1)));
  EXPECT_TRUE(FreopenTestFile::is_stream_valid(file.stream(1)));
  EXPECT_TRUE(FreopenTestFile::is_stream_backed_by_disk_file(file.stream(1)));
}

}  // namespace
#endif

#if !defined(_WIN32)
TEST(FileUtilsTest, TellPipe) {
  int pipefd[2];
  EXPECT_EQ(0, pipe(pipefd));
  my_off_t const pos = mysql_file_tell(pipefd[1], MYF(0));
  EXPECT_EQ(MY_FILEPOS_ERROR, pos);
  EXPECT_EQ(ESPIPE, my_errno());
  EXPECT_EQ(0, close(pipefd[0]));
  EXPECT_EQ(0, close(pipefd[1]));
}
#endif
