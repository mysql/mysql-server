/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_FILESYSTEM_INCLUDED
#define MYSQL_HARNESS_FILESYSTEM_INCLUDED

#include "harness_export.h"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#endif

#include "mysql/harness/access_rights.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

/**
 * @defgroup Filesystem Platform-independent file system operations
 *
 * This module contain platform-independent file system operations.
 */

/**
 * Class representing a path in a file system.
 *
 * @ingroup Filesystem
 *
 * Paths are used to access files in the file system and can be either
 * relative or absolute. Absolute paths have a slash (`/`) first in
 * the path, otherwise, the path is relative.
 */
class HARNESS_EXPORT Path {
  friend std::ostream &operator<<(std::ostream &out, const Path &path) {
    out << path.path_;
    return out;
  }

 public:
  /**
   * Enum used to identify file types.
   */

  enum class FileType {
    /** An error occurred when trying to get file type, but it is *not*
     * that the file was not found. */
    STATUS_ERROR,

    /** Empty path was given */
    EMPTY_PATH,

    /** The file was not found. */
    FILE_NOT_FOUND,

    /** The file is a regular file. */
    REGULAR_FILE,

    /** The file is a directory. */
    DIRECTORY_FILE,

    /** The file is a symbolic link. */
    SYMLINK_FILE,

    /** The file is a block device */
    BLOCK_FILE,

    /** The file is a character device */
    CHARACTER_FILE,

    /** The file is a FIFO */
    FIFO_FILE,

    /** The file is a UNIX socket */
    SOCKET_FILE,

    /** The type of the file is unknown, either because it was not
     * fetched yet or because stat(2) reported something else than the
     * above. */
    TYPE_UNKNOWN,
  };

  friend HARNESS_EXPORT std::ostream &operator<<(std::ostream &out,
                                                 FileType type);

  Path() noexcept;

  /**
   * Construct a path
   *
   * @param path Non-empty string denoting the path.
   * @throws std::invalid_argument
   */
  Path(std::string path);

  Path(std::string_view path) : Path(std::string(path)) {}
  Path(const char *path) : Path(std::string(path)) {}

  /**
   * Create a path from directory, basename, and extension.
   */
  static Path make_path(const Path &directory, const std::string &basename,
                        const std::string &extension);

  bool operator==(const Path &rhs) const;
  bool operator!=(const Path &rhs) const { return !(*this == rhs); }

  /**
   * Path ordering operator.
   *
   * This is mainly used for ordered containers. The paths are ordered
   * lexicographically.
   */
  bool operator<(const Path &rhs) const;

  /**
   * Get the file type.
   *
   * The file type is normally cached so if the file type under a path
   * changes it is necessary to force a refresh.
   *
   * @param refresh Set to `true` if the file type should be
   * refreshed, default to `false`.
   *
   * @return The type of the file.
   */
  FileType type(bool refresh = false) const;

  /**
   * Check if the file is a directory.
   */
  bool is_directory() const;

  /**
   * Check if the file is a regular file.
   */
  bool is_regular() const;

  /**
   * Check if the path is absolute or not
   *
   * The path is considered absolute if it starts with one of:
   *   Unix:    '/'
   *   Windows: '/' or '\' or '.:' (where . is any character)
   * else:
   *   it's considered relative (empty path is also relative in such respect)
   */
  bool is_absolute() const;

  /**
   * Check if path exists
   */
  bool exists() const;

  /*
   * @brief Checks if path exists and can be opened for reading.
   *
   * @return true if path exists and can be opened for reading,
   *         false otherwise.
   */
  bool is_readable() const;

  /**
   * Get the directory name of the path.
   *
   * This will strip the last component of a path, assuming that the
   * what remains is a directory name. If the path is a relative path
   * that do not contain any directory separators, a dot will be
   * returned (denoting the current directory).
   *
   * @note No checking of the components are done, this is just simple
   * path manipulation.
   *
   * @return A new path object representing the directory portion of
   * the path.
   */
  Path dirname() const;

  /**
   * Get the basename of the path.
   *
   * Return the basename of the path: the path without the directory
   * portion.
   *
   * @note No checking of the components are done, this is just simple
   * path manipulation.
   *
   * @return A new path object representing the basename of the path.
   * the path.
   */
  Path basename() const;

  /**
   * Append a path component to the current path.
   *
   * This function will append a path component to the path using the
   * appropriate directory separator.
   *
   * @param other Path component to append to the path.
   */
  void append(const Path &other);

  /**
   * Join two path components to form a new path.
   *
   * This function will join the two path components using a
   * directory separator.
   *
   * @note This will return a new `Path` object. If you want to modify
   * the existing path object, you should use `append` instead.
   *
   * @param other Path component to be appended to the path
   */
  Path join(const Path &other) const;

  /**
   * Returns the canonical form of the path, resolving relative paths.
   */
  Path real_path() const;

  /**
   * Get a C-string representation to the path.
   *
   * @note This will return a pointer to the internal representation
   * of the path and hence will become a dangling pointer when the
   * `Path` object is destroyed.
   *
   * @return Pointer to a null-terminated C-string.
   */
  const char *c_str() const { return path_.c_str(); }

  /**
   * Get a string representation of the path.
   *
   * @return Instance of std::string containing the path.
   */
  const std::string &str() const noexcept { return path_; }

  /**
   * Test if path is set
   *
   * @return Test result
   */
  bool is_set() const noexcept { return (type_ != FileType::EMPTY_PATH); }

  /**
   * Directory separator string.
   *
   * @note This is platform-dependent and defined in the appropriate
   * source file.
   */
  static const char *const directory_separator;

  /**
   * Root directory string.
   *
   * @note This is platform-dependent and defined in the appropriate
   * source file.
   */
  static const char *const root_directory;

  operator bool() const noexcept { return is_set(); }

 private:
  void validate_non_empty_path() const;  // throws std::invalid_argument

  std::string path_;
  mutable FileType type_;
};

/**
 * Class representing a directory in a file system.
 *
 * @ingroup Filesystem
 *
 * In addition to being a refinement of `Path`, it also have functions
 * that make it act like a container of paths and support iterating
 * over the entries in a directory.
 *
 * An example of how it could be used is:
 * @code
 * for (auto&& entry: Directory(path))
 *   std::cout << entry << std::endl;
 * @endcode
 */
class HARNESS_EXPORT Directory : public Path {
 public:
  /**
   * Directory iterator for iterating over directory entries.
   *
   * A directory iterator is an input iterator.
   */
  class HARNESS_EXPORT DirectoryIterator {
    friend class Directory;

   public:
    using value_type = Path;
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

    DirectoryIterator(const Path &path,
                      const std::string &pattern = std::string());

    // Create an end iterator
    DirectoryIterator();

    /**
     * Destructor.
     *
     * @note We need this *declared* because the default constructor
     * try to generate a default constructor for shared_ptr on State
     * below, which does not work since it is not visible. The
     * destructor need to be *defined* in the corresponding .cc file
     * since the State type is visible there (but you can use a
     * default definition).
     */
    ~DirectoryIterator();

    // We need these since the default move/copy constructor is
    // deleted when you define a destructor.
#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
    DirectoryIterator(DirectoryIterator &&);
    DirectoryIterator(const DirectoryIterator &);
#endif

    /** Standard iterator operators */
    /** @{ */
    Path operator*() const;
    DirectoryIterator &operator++();
    Path operator->() { return this->operator*(); }
    bool operator!=(const DirectoryIterator &other) const;

    // This avoids C2678 (no binary operator found) in MSVC,
    // MSVC's std::copy implementation (used by TestFilesystem) uses operator==
    // (while GCC's implementation uses operator!=).
    bool operator==(const DirectoryIterator &other) const {
      return !(this->operator!=(other));
    }
    /** @} */

   private:
    /**
     * Path to the root of the directory
     */
    const Path path_;

    /**
     * Pattern that matches entries iterated over.
     */
    std::string pattern_;

    /*
     * Platform-dependent container for iterator state.
     *
     * The definition of this class is different for different
     * platforms, meaning that it is not defined here at all but
     * rather in the corresponding `filesystem-<platform>.cc` file.
     *
     * The directory iterator is the most critical piece since it holds
     * an iteration state for the platform: something that requires
     * different types on the platforms.
     */
    class State;
    std::shared_ptr<State> state_;
  };

  /**
   * Construct a directory instance.
   *
   * Construct a directory instance in different ways depending on the
   * version of the constructor used.
   */
  Directory(const std::string &path)  // NOLINT(runtime/explicit)
      : Path(path) {}                 // throws std::invalid_argument

  /** @overload */              // throws std::invalid_argument
  Directory(const Path &path);  // NOLINT(runtime/explicit)

  Directory(const Directory &) = default;
  Directory &operator=(const Directory &) = default;
  ~Directory();

  /**
   * Iterator to first entry.
   *
   * @return Returns an iterator pointing to the first entry.
   */
  DirectoryIterator begin();

  DirectoryIterator begin() const { return cbegin(); }

  /**
   * Constant iterator to first entry.
   *
   * @return Returns a constant iterator pointing to the first entry.
   */
  DirectoryIterator cbegin() const;

  /**
   * Iterator past-the-end of entries.
   *
   * @return Returns an iterator pointing *past-the-end* of the entries.
   */
  DirectoryIterator end();

  DirectoryIterator end() const { return cend(); }

  /**
   * Constant iterator past-the-end of entries.
   *
   * @return Returns a constant iterator pointing *past-the-end* of the entries.
   */
  DirectoryIterator cend() const;

  /**
   * Check if the directory is empty.
   *
   * @retval true Directory is empty.
   * @retval false Directory is no empty.
   */
  bool is_empty() const;

  /**
   * Recursively list all paths in a directory.
   *
   * Recursively create a list of relative paths from a directory. Path will
   * be relative to the given directory. Empty directories are also listed.
   *
   * @return Recursive list of paths from a directory.
   */
  std::vector<Path> list_recursive() const;

  /**
   * Iterate over entries matching a glob.
   */
  DirectoryIterator glob(const std::string &glob);
};

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

/** @brief Removes a directory.
 *
 * @ingroup Filesystem
 *
 * @param dir path of the directory to be removed; this directory must be empty
 *
 * @return void on success, error_code on failure
 */
HARNESS_EXPORT
stdx::expected<void, std::error_code> delete_dir(
    const std::string &dir) noexcept;

/** @brief Removes a file.
 *
 * @ingroup Filesystem
 *
 * @param path of the file to be removed
 *
 * @return void on success, error_code on failure
 */
HARNESS_EXPORT
stdx::expected<void, std::error_code> delete_file(
    const std::string &path) noexcept;

/** @brief Removes directory and all its contents.
 *
 * @ingroup Filesystem
 *
 * @param dir path of the directory to be removed
 *
 * @return void on success, error_code on failure
 */
HARNESS_EXPORT
stdx::expected<void, std::error_code> delete_dir_recursive(
    const std::string &dir) noexcept;

/** @brief Creates a temporary directory with partially-random name and returns
 * its path.
 *
 * Creates a directory with a name of form {prefix}-{6 random alphanumerals}.
 * For example, a possible directory name created by a call to
 * get_tmp_dir("foo") might be: foo-3f9x0z
 *
 * Such directory is usually meant to be used as a temporary directory (thus the
 * "_tmp_" in the name of this function).
 *
 * @ingroup Filesystem
 *
 * @param name name to be used as a directory name prefix
 *
 * @return path to the created directory
 *
 * @throws std::runtime_error if operation failed
 */
HARNESS_EXPORT
std::string get_tmp_dir(const std::string &name = "router");

// TODO: description
// TODO: move to some other place?
HARNESS_EXPORT
std::string get_plugin_dir(const std::string &runtime_dir);

HARNESS_EXPORT
std::string get_tests_data_dir(const std::string &runtime_dir);

#ifndef _WIN32
using perm_mode = mode_t;
HARNESS_EXPORT
extern const perm_mode kStrictDirectoryPerm;
#else
using perm_mode = int;
HARNESS_EXPORT
extern const perm_mode kStrictDirectoryPerm;
#endif

/** @brief Creates a directory
 * *
 * @param dir       name (or path) of the directory to create
 * @param mode      permission mode for the created directory
 * @param recursive if true then imitate unix `mkdir -p` recursively
 *                  creating parent directories if needed
 * @retval 0 operation succeeded
 * @retval -1 operation failed because of wrong parameters
 * @retval > 0 errno for failure to mkdir() system call
 */
HARNESS_EXPORT
int mkdir(const std::string &dir, perm_mode mode, bool recursive = false);

/**
 * Changes file access permissions to be fully accessible by all users.
 *
 * On Unix, the function sets file permission mask to 777.
 * On Windows, Everyone group is granted full access to the file.
 *
 * @param[in] file_name File name.
 *
 * @throw std::exception Failed to change file permissions.
 */
void HARNESS_EXPORT make_file_public(const std::string &file_name);

#ifdef _WIN32
/**
 * Changes file access permissions to be readable by all users.
 *
 * On Windows, Everyone group is granted read access to the file.
 *
 * @param[in] file_name File name.
 *
 * @throw std::exception Failed to change file permissions.
 */
void make_file_readable_for_everyone(const std::string &file_name);
#endif

/**
 * Changes file access permissions to be accessible only by a limited set of
 * users.
 *
 * On Unix, the function sets file permission mask to 600.
 * On Windows, all permissions to this file are removed for Everyone group,
 * LocalService account gets read (and optionally write) access.
 *
 * @param[in] file_name File name.
 * @param[in] read_only_for_local_service Weather the LocalService user on
 * Windows should get only the read access (if false will grant write access
 * too). Not used on non-Windows.
 *
 * @throw std::exception Failed to change file permissions.
 */
void HARNESS_EXPORT
make_file_private(const std::string &file_name,
                  const bool read_only_for_local_service = true);

/**
 * Changes file access permissions to be read only.
 *
 * On Unix, the function sets file permission mask to 555.
 * On Windows, all permissions to this file are read access only for Everyone
 * group, LocalService account gets read access.
 *
 * @param[in] file_name File name.
 *
 * @throw std::exception Failed to change file permissions.
 */
void HARNESS_EXPORT make_file_readonly(const std::string &file_name);

/**
 * Verifies access permissions of a file.
 *
 * On Unix systems it throws if file's permissions differ from 600.
 * On Windows it throws if file can be accessed by Everyone group.
 *
 * @param[in] file_name File to be verified.
 *
 * @throw std::exception File access rights are too permissive or
 *                        an error occurred.
 * @throw std::system_error OS and/or filesystem doesn't support file
 *                           permissions.
 */

void HARNESS_EXPORT check_file_access_rights(const std::string &file_name);

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_FILESYSTEM_INCLUDED */
