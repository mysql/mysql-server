/*****************************************************************************

Copyright (c) 2013, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

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

/** Types of raw partitions in innodb_data_file_path */
enum device_t {

  /** Not a raw partition */
  SRV_NOT_RAW = 0,

  /** A 'newraw' partition, only to be initialized */
  SRV_NEW_RAW,

  /** An initialized raw partition */
  SRV_OLD_RAW
};

/** Data file control information. */
class Datafile {
  friend class Tablespace;
  friend class SysTablespace;

 public:
  Datafile()
      : m_name(),
        m_filename(),
        m_open_flags(OS_FILE_OPEN),
        m_size(),
        m_order(),
        m_type(SRV_NOT_RAW),
        m_space_id(SPACE_UNKNOWN),
        m_flags(),
        m_exists(),
        m_is_valid(),
        m_first_page(),
        m_atomic_write(),
        m_filepath(),
        m_last_os_error(),
        m_file_info(),
        m_encryption_key(),
        m_encryption_iv(),
        m_encryption_op_in_progress(Encryption::Progress::NONE),
        m_encryption_master_key_id(0) {
    m_handle.m_file = OS_FILE_CLOSED;
  }

  Datafile(const char *name, uint32_t flags, page_no_t size, ulint order)
      : m_name(mem_strdup(name)),
        m_filename(),
        m_open_flags(OS_FILE_OPEN),
        m_size(size),
        m_order(order),
        m_type(SRV_NOT_RAW),
        m_space_id(SPACE_UNKNOWN),
        m_flags(flags),
        m_exists(),
        m_is_valid(),
        m_first_page(),
        m_atomic_write(),
        m_filepath(),
        m_last_os_error(),
        m_file_info(),
        m_encryption_key(),
        m_encryption_iv(),
        m_encryption_op_in_progress(Encryption::Progress::NONE),
        m_encryption_master_key_id(0) {
    ut_ad(m_name != nullptr);
    m_handle.m_file = OS_FILE_CLOSED;
    /* No op */
  }

  Datafile(const Datafile &file)
      : m_handle(file.m_handle),
        m_open_flags(file.m_open_flags),
        m_size(file.m_size),
        m_order(file.m_order),
        m_type(file.m_type),
        m_space_id(file.m_space_id),
        m_flags(file.m_flags),
        m_exists(file.m_exists),
        m_is_valid(file.m_is_valid),
        m_first_page(),
        m_atomic_write(file.m_atomic_write),
        m_last_os_error(),
        m_file_info(),
        m_encryption_key(),
        m_encryption_iv(),
        m_encryption_op_in_progress(Encryption::Progress::NONE),
        m_encryption_master_key_id(0) {
    m_name = mem_strdup(file.m_name);
    ut_ad(m_name != nullptr);

    if (file.m_filepath != nullptr) {
      m_filepath = mem_strdup(file.m_filepath);
      ut_a(m_filepath != nullptr);
      set_filename();
    } else {
      m_filepath = nullptr;
      m_filename = nullptr;
    }
  }

  ~Datafile() { shutdown(); }

  Datafile &operator=(const Datafile &file) {
    ut_a(this != &file);

    ut_ad(m_name == nullptr);
    m_name = mem_strdup(file.m_name);
    ut_a(m_name != nullptr);

    m_size = file.m_size;
    m_order = file.m_order;
    m_type = file.m_type;

    ut_a(m_handle.m_file == OS_FILE_CLOSED);
    m_handle = file.m_handle;

    m_exists = file.m_exists;
    m_is_valid = file.m_is_valid;
    m_open_flags = file.m_open_flags;
    m_space_id = file.m_space_id;
    m_flags = file.m_flags;
    m_last_os_error = 0;

    if (m_filepath != nullptr) {
      ut::free(m_filepath);
      m_filepath = nullptr;
      m_filename = nullptr;
    }

    if (file.m_filepath != nullptr) {
      m_filepath = mem_strdup(file.m_filepath);
      ut_a(m_filepath != nullptr);
      set_filename();
    }

    /* Do not make a copy of the first page,
    it should be reread if needed */
    m_first_page = nullptr;
    m_encryption_key = nullptr;
    m_encryption_iv = nullptr;
    m_encryption_op_in_progress = Encryption::Progress::NONE;
    m_encryption_master_key_id = 0;

    m_atomic_write = file.m_atomic_write;

    return (*this);
  }

  /** Initialize the name and flags of this datafile.
  @param[in]    name    tablespace name, will be copied
  @param[in]    flags   tablespace flags */
  void init(const char *name, uint32_t flags);

  /** Release the resources. */
  void shutdown();

  /** Open a data file in read-only mode to check if it exists
  so that it can be validated.
  @param[in]    strict  whether to issue error messages
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t open_read_only(bool strict);

  /** Open a data file in read-write mode during start-up so that
  doublewrite pages can be restored and then it can be validated.
  @param[in]    read_only_mode  if true, then readonly mode checks
                                  are enforced.
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t open_read_write(bool read_only_mode);

  /** Initialize OS specific file info. */
  void init_file_info();

  /** Close a data file.
  @return DB_SUCCESS or error code */
  dberr_t close();

  /** Returns if the Datafile is created in raw partition
  @return true if partition  used is raw , false otherwise */
  bool is_raw_type() {
    return (m_type == SRV_NEW_RAW || m_type == SRV_OLD_RAW);
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

  /** Allocate and set the datafile or tablespace name in m_name.
  If a name is provided, use it; else if the datafile is file-per-table,
  extract a file-per-table tablespace name from m_filepath; else it is a
  general tablespace, so just call it that for now. The value of m_name
  will be freed in the destructor.
  @param[in]    name    Tablespace Name if known, nullptr if not */
  void set_name(const char *name);

  /** Validates the datafile and checks that it conforms with the expected
  space ID and flags.  The file should exist and be successfully opened
  in order for this function to validate it.
  @param[in]    space_id        The expected tablespace ID.
  @param[in]    flags           The expected tablespace flags.
  @param[in]    for_import      if it is for importing
  @retval DB_SUCCESS if tablespace is valid, DB_ERROR if not.
  m_is_valid is also set true on success, else false. */
  [[nodiscard]] dberr_t validate_to_dd(space_id_t space_id, uint32_t flags,
                                       bool for_import);

  /** Validates this datafile for the purpose of recovery.  The file should
  exist and be successfully opened. We initially open it in read-only mode
  because we just want to read the SpaceID.  However, if the first page is
  corrupt and needs to be restored from the doublewrite buffer, we will reopen
  it in write mode and try to restore that page. The file will be closed when
  returning from this method.
  @param[in]    space_id        Expected space ID
  @retval DB_SUCCESS on success
  m_is_valid is also set true on success, else false. */
  [[nodiscard]] dberr_t validate_for_recovery(space_id_t space_id);

  /**  Checks the consistency of the first page of a datafile when the
  tablespace is opened. This occurs before the fil_space_t is created so the
  Space ID found here must not already be open. m_is_valid is set true on
  success, else false. The datafile is always closed when returning from this
  method.
  @param[in]    space_id        Expected space ID
  @param[out]   flush_lsn       contents of FIL_PAGE_FILE_FLUSH_LSN
  @param[in]    for_import      if it is for importing
  (only valid for the first file of the system tablespace)
  @retval DB_WRONG_FILE_NAME tablespace in file header doesn't match
          expected value
  @retval DB_SUCCESS on if the datafile is valid
  @retval DB_CORRUPTION if the datafile is not readable
  @retval DB_INVALID_ENCRYPTION_META if the encryption meta data
          is not readable
  @retval DB_TABLESPACE_EXISTS if there is a duplicate space_id */
  [[nodiscard]] dberr_t validate_first_page(space_id_t space_id,
                                            lsn_t *flush_lsn, bool for_import);

  /** Get LSN of first page */
  lsn_t get_flush_lsn() {
    ut_ad(m_first_page != nullptr);
    return mach_read_from_8(m_first_page + FIL_PAGE_LSN);
  }

  /** Get Datafile::m_name.
  @return m_name */
  const char *name() const { return (m_name); }

  /** Get Datafile::m_filepath.
  @return m_filepath */
  const char *filepath() const { return (m_filepath); }

  /** Get Datafile::m_handle.
  @return m_handle */
  pfs_os_file_t handle() const {
    ut_ad(is_open());
    return (m_handle);
  }

  /** Get Datafile::m_order.
  @return m_order */
  ulint order() const { return (m_order); }

  /** Get Datafile::m_server_version.
  @return m_server_version */
  ulint server_version() const { return (m_server_version); }

  /** Get Datafile::m_space_version.
  @return m_space_version */
  ulint space_version() const { return (m_space_version); }

  /** Get Datafile::m_space_id.
  @return m_space_id */
  space_id_t space_id() const { return (m_space_id); }

  /** Get Datafile::m_flags.
  @return m_flags */
  uint32_t flags() const { return (m_flags); }

  /**
  @return true if m_handle is open, false if not */
  bool is_open() const { return (m_handle.m_file != OS_FILE_CLOSED); }

  /** Get Datafile::m_is_valid.
  @return m_is_valid */
  bool is_valid() const { return (m_is_valid); }

  /** Get the last OS error reported
  @return m_last_os_error */
  ulint last_os_error() const { return (m_last_os_error); }

  /** Do a quick test if the filepath provided looks the same as this filepath
  byte by byte. If they are two different looking paths to the same file,
  same_as() will be used to show that after the files are opened.
  @param[in]    other   filepath to compare with
  @retval true if it is the same filename by byte comparison
  @retval false if it looks different */
  bool same_filepath_as(const char *other) const;

  /** Test if another opened datafile is the same file as this object.
  @param[in]    other   Datafile to compare with
  @return true if it is the same file, else false */
  bool same_as(const Datafile &other) const;

  /** Determine the space id of the given file descriptor by reading
  a few pages from the beginning of the .ibd file.
  @return DB_SUCCESS if space id was successfully identified,
  else DB_ERROR. */
  dberr_t find_space_id();

  /** @return file size in number of pages */
  page_no_t size() const { return (m_size); }

#ifdef UNIV_HOTBACKUP
  /** Set the tablespace ID.
  @param[in]    space_id        Tablespace ID to set */
  void set_space_id(space_id_t space_id) {
    ut_ad(space_id <= 0xFFFFFFFFU);
    m_space_id = space_id;
  }

  /** Set th tablespace flags
  @param[in]    flags   Tablespace flags */
  void set_flags(uint32_t flags) { m_flags = flags; }
#endif /* UNIV_HOTBACKUP */

 private:
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

  /** Create/open a data file.
  @param[in]    read_only_mode  if true, then readonly mode checks
                                  are enforced.
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t open_or_create(bool read_only_mode);

  /** Reads a few significant fields from the first page of the
  datafile, which must already be open.
  @param[in]    read_only_mode  If true, then readonly mode checks
                                  are enforced.
  @return DB_SUCCESS or DB_IO_ERROR if page cannot be read */
  [[nodiscard]] dberr_t read_first_page(bool read_only_mode);

  /** Free the first page from memory when it is no longer needed. */
  void free_first_page();

  /** Set the Datafile::m_open_flags.
  @param open_flags     The Open flags to set. */
  void set_open_flags(os_file_create_t open_flags) {
    m_open_flags = open_flags;
  }

  /** Finds a given page of the given space id from the double write buffer
  and copies it to the corresponding .ibd file.
  @param[in]    restore_page_no         Page number to restore
  @return DB_SUCCESS if page was restored from doublewrite, else DB_ERROR */
  dberr_t restore_from_doublewrite(page_no_t restore_page_no);

 private:
  /** Datafile name at the tablespace location.
  This is either the basename of the file if an absolute path
  was entered, or it is the relative path to the datadir or
  Tablespace::m_path. */
  char *m_name;

  /** Points into m_filepath to the file name with extension */
  char *m_filename;

  /** Open file handle */
  pfs_os_file_t m_handle;

  /** Flags to use for opening the data file */
  os_file_create_t m_open_flags;

  /** size in pages */
  page_no_t m_size;

  /** ordinal position of this datafile in the tablespace */
  ulint m_order;

  /** The type of the data file */
  device_t m_type;

  /** Tablespace ID. Contained in the datafile header.
  If this is a system tablespace, FSP_SPACE_ID is only valid
  in the first datafile. */
  space_id_t m_space_id;

  /** Server version */
  uint32_t m_server_version;

  /** Space version */
  uint32_t m_space_version;

  /** Tablespace flags. Contained in the datafile header.
  If this is a system tablespace, FSP_SPACE_FLAGS are only valid
  in the first datafile. */
  uint32_t m_flags;

  /** true if file already existed on startup */
  bool m_exists;

  /* true if the tablespace is valid */
  bool m_is_valid;

  /** Buffer to hold first page */
  byte *m_first_page;

  /** true if atomic writes enabled for this file */
  bool m_atomic_write;

 protected:
  /** Physical file path with base name and extension */
  char *m_filepath;

  /** Last OS error received so it can be reported if needed. */
  ulint m_last_os_error;

 public:
  /** Use the following to determine the uniqueness of this datafile. */
#ifdef _WIN32
  using WIN32_FILE_INFO = BY_HANDLE_FILE_INFORMATION;

  /** Use fields dwVolumeSerialNumber, nFileIndexLow, nFileIndexHigh. */
  WIN32_FILE_INFO m_file_info;
#else
  /** Use field st_ino. */
  struct stat m_file_info;
#endif /* WIN32 */

  /** Encryption key read from first page */
  byte *m_encryption_key;

  /** Encryption iv read from first page */
  byte *m_encryption_iv;

  /** Encryption operation in progress */
  Encryption::Progress m_encryption_op_in_progress;

  /** Master key id read from first page */
  uint32_t m_encryption_master_key_id;
};
#endif /* fsp0file_h */
