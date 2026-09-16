/*****************************************************************************

Copyright (c) 2013, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/fsp0file.h
 Tablespace data file implementation.

 Created 2013-7-26 by Kevin Lewis
 *******************************************************/

#ifndef fsp0file_h
#define fsp0file_h

#include <vector>
#include "fil0fil.h" /* SPACE_UNKNOWN */
#include "ha_prototypes.h"
#include "mem0mem.h"
#include "os0file.h"

#ifdef UNIV_HOTBACKUP
#include "fil0fil.h"
#include "fsp0types.h"

/** MEB routine to get the master key. MEB will extract
the key from the keyring encrypted file stored in backup.
@param[in]      key_id          the id of the master key
@param[in]      key_type        master key type
@param[out]     key             the master key being returned
@param[out]     key_length      the length of the returned key
@retval 0 if the key is being returned, 1 otherwise. */
extern int meb_key_fetch(const char *key_id, char **key_type,
                         const char *user_id, void **key, size_t *key_length);
#endif /* UNIV_HOTBACKUP */

/** Types of nodes specifed in innodb_data_file_path */
enum class node_device_type_t {

  /** Not a raw partition, a regular file */
  REGULAR_FILE,

  /** A 'newraw' partition, only to be initialized */
  NEW_RAW,

  /** An initialized raw partition */
  OLD_RAW
};

/** Data file control information. */
class Datafile {
  friend class SysTablespace;

 public:
  Datafile() { m_handle.m_file = OS_FILE_CLOSED; }

  Datafile(const Datafile &file) = delete;

  ~Datafile() { shutdown(); }

  Datafile &operator=(const Datafile &file) = delete;

  /** Release the resources. */
  void shutdown();

  /** Open a data file in read-only mode to check if it exists
  so that it can be validated.
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t open_read_only();

  /** Close a data file.
  @return DB_SUCCESS or error code */
  dberr_t close();

  /** Extracts basic space fields from the first page header.
  @param[in]    page            The content of the first page of the tablespace.
  */
  void extract_fields_from_first_page(const byte *page);

  /** Returns if the Datafile is created in raw partition
  @return true if partition  used is raw , false otherwise */
  bool is_raw_type() {
    return (m_type == node_device_type_t::NEW_RAW ||
            m_type == node_device_type_t::OLD_RAW);
  }

  /** Make a full filepath from a directory path and a filename.
  Prepend the dirpath to filename using the extension given.
  If dirpath is nullptr, prepend the default datadir to filepath.
  Store the result in m_filepath.
  @param[in]    dirpath         directory path
  @param[in]    filename        filename or filepath
  @param[in]    ext             filename extension */
  void make_filepath(const char *dirpath, const char *filename,
                     ib_file_suffix ext);

  /** Set the filepath by duplicating the filepath sent in */
  void set_filepath(const char *filepath);

  /** Validates the first page of tablespace supplied and checks that it
  conforms with the expected space ID and flags. The datafile will not be
  attempted to be opened.
  @param[in]    page            The content of the first page of the tablespace.
  @param[in]    space_id        The expected tablespace ID.
  @param[in]    flags           The expected tablespace flags.
  @param[in]    filepath        The file path to the first node in the
                                tablespace, used for error message printing.
  @param[in]    for_import      if it is for importing
  @retval DB_SUCCESS if tablespace is valid, DB_ERROR if not.
  m_is_valid is also set true on success, else false. */
  [[nodiscard]] dberr_t validate_to_dd(const byte *page, space_id_t space_id,
                                       uint32_t flags,
                                       const std::string &filepath,
                                       bool for_import);

  /** Validates this datafile for the purpose of recovery. The file must
  exist and be already opened. The datafile is always closed when returning from
  this method.
  @param[in]    space_id                    Expected space ID
  @param[in]    expected_physical_page_size Physical page size of the
                                            tablespace.
  @retval DB_WRONG_FILE_NAME tablespace in file header doesn't match
          expected value
  @retval DB_SUCCESS on if the datafile is valid
  @retval DB_CORRUPTION if the datafile is not readable
  @retval DB_INVALID_ENCRYPTION_META if the encryption meta data
          is not readable
  @retval DB_TABLESPACE_EXISTS if there is a duplicate space_id
  m_is_valid is also set true on success, else false. */
  [[nodiscard]] dberr_t validate_for_recovery(
      space_id_t space_id, uint32_t expected_physical_page_size);

  void reset_first_page_fields_cache() {
    m_first_page_fields_cache.m_is_valid = false;
  }

  /** Get Datafile::m_filepath.
  @return m_filepath */
  const char *filepath() const { return (m_filepath); }

  /** Get Datafile::m_handle.
  @return m_handle */
  pfs_os_file_t handle() const {
    ut_ad(is_open());
    return (m_handle);
  }

  /** Returns cached server version read from the first page during validation.
   */
  uint32_t get_cached_server_version() const {
    ut_a(m_first_page_fields_cache.m_is_valid);
    return m_first_page_fields_cache.m_server_version;
  }

  /** Returns cached space version read from the first page during validation.
   */
  uint32_t get_cached_space_version() const {
    ut_a(m_first_page_fields_cache.m_is_valid);
    return m_first_page_fields_cache.m_space_version;
  }

  /** Returns cached space ID read from the first page during validation. */
  space_id_t get_cached_space_id() const {
    ut_a(m_first_page_fields_cache.m_is_valid);
    return m_first_page_fields_cache.m_space_id;
  }

  /** Returns cached space FSP flags read from the first page during validation.
   */
  uint32_t get_cached_space_flags() const {
    ut_a(m_first_page_fields_cache.m_is_valid);
    return m_first_page_fields_cache.m_space_flags;
  }

  /** Returns cached space flush LSN read from the first page during validation.
   */
  uint32_t get_cached_space_flush_lsn() const {
    ut_a(m_first_page_fields_cache.m_is_valid);
    return m_first_page_fields_cache.m_flush_lsn;
  }

  /** Get the physical page size used by the tablespace. */
  size_t physical_page_size() const {
    return page_size_t{get_cached_space_flags()}.physical();
  }

  /**
  @return true if m_handle is open, false if not */
  bool is_open() const { return (m_handle.m_file != OS_FILE_CLOSED); }

  /** Get Datafile::m_is_valid.
  @return m_is_valid */
  bool is_valid() const { return (m_is_valid); }

  /** @return file size in number of pages */
  page_no_t size() const { return (m_size); }

 private:
  /**  Checks the consistency of the first page supplied when the tablespace is
  opened, and verifies the space is not already added to the `fil` mappings with
  a different datafile path. m_is_valid is set true on success, else false. The
  datafile will not be attempted to be opened.
  @param[in]    page            The content of the first page of the tablespace.
  @param[in]    space_id        Expected space ID
  @param[in]    filepath        The file path to the first node in the
                                tablespace, used for error message printing.
  @param[in]    for_import      if it is for importing
  @retval DB_WRONG_FILE_NAME tablespace in file header doesn't match
          expected value
  @retval DB_SUCCESS on if the datafile is valid
  @retval DB_CORRUPTION if the datafile is not readable
  @retval DB_INVALID_ENCRYPTION_META if the encryption meta data
          is not readable
  @retval DB_TABLESPACE_EXISTS if there is a duplicate space_id */
  [[nodiscard]] dberr_t validate_first_page(const byte *page,
                                            space_id_t space_id,
                                            const std::string &filepath,
                                            bool for_import);

  /** Free the filepath buffer. */
  void free_filepath();

  /** Set the filename pointer to the start of the file name
  in the filepath. */
  void set_filename() {
    if (m_filepath == nullptr) {
      return;
    }

    char *last_slash = strrchr(m_filepath, OS_PATH_SEPARATOR);

    m_filename = last_slash ? last_slash + 1 : m_filepath;
  }

  /** Reads the first page of the datafile. The datafile must be already opened.
  @param[in]    space_id           Expected space ID
  @param[in]    physical_page_size Physical page size of the tablespace.
  @return DB_SUCCESS or DB_IO_ERROR if page cannot be read */
  [[nodiscard]] ut::Expected<ut::unique_ptr_aligned<byte[]>> read_first_page(
      space_id_t space_id, uint32_t physical_page_size);

 private:
  /** Points into m_filepath to the file name with extension */
  char *m_filename{};

  /** Open file handle */
  pfs_os_file_t m_handle;

  /** size in pages */
  page_no_t m_size{};

  /** The type of the data file */
  node_device_type_t m_type{node_device_type_t::REGULAR_FILE};

  /** Cache of fields extracted from the first page. */
  struct first_page_fields_cache_t {
    /** Tablespace ID. Contained in the datafile header.
    If this is a system tablespace, FSP_SPACE_ID is only valid
    in the first datafile. */
    space_id_t m_space_id{SPACE_UNKNOWN};

    /** Server version */
    uint32_t m_server_version{};

    /** Space version */
    uint32_t m_space_version{};

    /** Tablespace flags. Contained in the datafile header.
    If this is a system tablespace, FSP_SPACE_FLAGS are only valid
    in the first datafile. */
    uint32_t m_space_flags{};

    /** Flush LSN stored in the tablespace header. */
    lsn_t m_flush_lsn{};

    /** Specifies if the cache values have already values assigned. */
    bool m_is_valid{};
  };

  /** Cache of fields extracted from the first page. */
  first_page_fields_cache_t m_first_page_fields_cache;

  /** true if file already existed on startup */
  bool m_exists{};

  /* true if the tablespace is valid */
  bool m_is_valid{};

 protected:
  /** Physical file path with base name and extension */
  char *m_filepath{};

 public:
  /** Encryption key read from first page */
  byte *m_encryption_key{};

  /** Encryption iv read from first page */
  byte *m_encryption_iv{};

  /** Encryption operation in progress */
  Encryption::Progress m_encryption_op_in_progress{Encryption::Progress::NONE};

  /** Master key id read from first page */
  uint32_t m_encryption_master_key_id{};
};
#endif /* fsp0file_h */
