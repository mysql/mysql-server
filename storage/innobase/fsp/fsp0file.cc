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

/** @file fsp/fsp0file.cc
 Tablespace data file implementation

 Created 2013-7-26 by Kevin Lewis
 *******************************************************/

#include "ha_prototypes.h"

#include "dict0dd.h"
#include "fil0fil.h"
#include "fsp0file.h"
#include "fsp0sysspace.h"
#include "fsp0types.h"
#include "os0file.h"
#include "page0page.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "ut0new.h"

#include <scope_guard.h>

#ifdef UNIV_HOTBACKUP
#include "my_sys.h"
#endif /* UNIV_HOTBACKUP */

/** Initialize the name and flags of this datafile.
@param[in]      name    tablespace name, will be copied
@param[in]      flags   tablespace flags */
void Datafile::init(const char *name, uint32_t flags) {
  ut_ad(m_name == nullptr);
  ut_ad(name != nullptr);

  m_name = mem_strdup(name);
  m_flags = flags;
  m_encryption_key = nullptr;
  m_encryption_iv = nullptr;
}

/** Release the resources. */
void Datafile::shutdown() {
  close();

  ut::free(m_name);
  m_name = nullptr;

  free_filepath();

  free_first_page();

  if (m_encryption_key != nullptr) {
    ut::free(m_encryption_key);
    m_encryption_key = nullptr;
  }

  if (m_encryption_iv != nullptr) {
    ut::free(m_encryption_iv);
    m_encryption_iv = nullptr;
  }
}

/** Create/open a data file.
@param[in]      read_only_mode  if true, then readonly mode checks are enforced.
@return DB_SUCCESS or error code */
dberr_t Datafile::open_or_create(bool read_only_mode) {
  bool success;
  ut_a(m_filepath != nullptr);
  ut_ad(m_handle.m_file == OS_FILE_CLOSED);

  m_handle =
      os_file_create(innodb_data_file_key, m_filepath, m_open_flags,
                     OS_FILE_NORMAL, OS_DATA_FILE, read_only_mode, &success);

  if (!success) {
    m_last_os_error = os_file_get_last_error(true);
    ib::error(ER_IB_MSG_390) << "Cannot open datafile '" << m_filepath << "'";
    return (DB_CANNOT_OPEN_FILE);
  }

  return (DB_SUCCESS);
}

/** Open a data file in read-only mode to check if it exists so that it
can be validated.
@param[in]      strict  whether to issue error messages
@return DB_SUCCESS or error code */
dberr_t Datafile::open_read_only(bool strict) {
  bool success = false;
  ut_ad(m_handle.m_file == OS_FILE_CLOSED);

  /* This function can be called for file objects that do not need
  to be opened, which is the case when the m_filepath is NULL */
  if (m_filepath == nullptr) {
    return (DB_ERROR);
  }

  set_open_flags(OS_FILE_OPEN);
  m_handle = os_file_create_simple_no_error_handling(
      innodb_data_file_key, m_filepath, m_open_flags, OS_FILE_READ_ONLY, true,
      &success);

  if (success) {
    m_exists = true;
    init_file_info();

    return (DB_SUCCESS);
  }

  if (strict) {
    m_last_os_error = os_file_get_last_error(true);
    ib::error(ER_IB_MSG_391) << "Cannot open datafile for read-only: '"
                             << m_filepath << "' OS error: " << m_last_os_error;
  }

  return (DB_CANNOT_OPEN_FILE);
}

/** Open a data file in read-write mode during start-up so that
doublewrite pages can be restored and then it can be validated.
@param[in]      read_only_mode  if true, then readonly mode checks are enforced.
@return DB_SUCCESS or error code */
dberr_t Datafile::open_read_write(bool read_only_mode) {
  bool success = false;
  ut_ad(m_handle.m_file == OS_FILE_CLOSED);

  /* This function can be called for file objects that do not need
  to be opened, which is the case when the m_filepath is NULL */
  if (m_filepath == nullptr) {
    return (DB_ERROR);
  }

  set_open_flags(OS_FILE_OPEN);
  m_handle = os_file_create_simple_no_error_handling(
      innodb_data_file_key, m_filepath, m_open_flags, OS_FILE_READ_WRITE,
      read_only_mode, &success);

  if (!success) {
    m_last_os_error = os_file_get_last_error(true);
    ib::error(ER_IB_MSG_392)
        << "Cannot open datafile for read-write: '" << m_filepath << "'";
    return (DB_CANNOT_OPEN_FILE);
  }

  m_exists = true;

  init_file_info();

  return (DB_SUCCESS);
}

/** Initialize OS specific file info. */
void Datafile::init_file_info() {
#ifdef _WIN32
  GetFileInformationByHandle(m_handle.m_file, &m_file_info);
#else
  fstat(m_handle.m_file, &m_file_info);
#endif /* WIN32 */
}

/** Close a data file.
@return DB_SUCCESS or error code */
dberr_t Datafile::close() {
  if (m_handle.m_file != OS_FILE_CLOSED) {
    auto success = os_file_close(m_handle);
    ut_a(success);

    m_handle.m_file = OS_FILE_CLOSED;
  }

  return DB_SUCCESS;
}

/** Make a full filepath from a directory path and a filename.
Prepend the dirpath to filename using the extension given.
If dirpath is nullptr, prepend the default datadir to filepath.
Store the result in m_filepath.
@param[in]      dirpath         directory path
@param[in]      filename        filename or filepath
@param[in]      ext             filename extension */
void Datafile::make_filepath(const char *dirpath, const char *filename,
                             ib_file_suffix ext) {
  free_filepath();

  std::string path;
  std::string name;

  if (dirpath != nullptr) {
    path.assign(dirpath);
  }

  if (filename != nullptr) {
    name.assign(filename);
  }

  m_filepath = Fil_path::make(path, name, ext);

  ut_ad(m_filepath != nullptr);

  set_filename();
}

/** Set the filepath by duplicating the filepath sent in. This is the
name of the file with its extension and absolute or relative path.
@param[in]      filepath        filepath to set */
void Datafile::set_filepath(const char *filepath) {
  free_filepath();
  m_filepath = static_cast<char *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, strlen(filepath) + 1));
  ::strcpy(m_filepath, filepath);
  set_filename();
}

/** Free the filepath buffer. */
void Datafile::free_filepath() {
  if (m_filepath != nullptr) {
    ut::free(m_filepath);
    m_filepath = nullptr;
    m_filename = nullptr;
  }
}

/** Do a quick test if the filepath provided looks the same as this filepath
byte by byte. If they are two different looking paths to the same file,
same_as() will be used to show that after the files are opened.
@param[in]      other   filepath to compare with
@retval true if it is the same filename by byte comparison
@retval false if it looks different */
bool Datafile::same_filepath_as(const char *other) const {
  return (0 == strcmp(m_filepath, other));
}

/** Test if another opened datafile is the same file as this object.
@param[in]      other   Datafile to compare with
@return true if it is the same file, else false */
bool Datafile::same_as(const Datafile &other) const {
#ifdef _WIN32
  return (m_file_info.dwVolumeSerialNumber ==
              other.m_file_info.dwVolumeSerialNumber &&
          m_file_info.nFileIndexHigh == other.m_file_info.nFileIndexHigh &&
          m_file_info.nFileIndexLow == other.m_file_info.nFileIndexLow);
#else
  return (m_file_info.st_ino == other.m_file_info.st_ino &&
          m_file_info.st_dev == other.m_file_info.st_dev);
#endif /* WIN32 */
}

/** Allocate and set the datafile or tablespace name in m_name.
If a name is provided, use it; else if the datafile is file-per-table,
extract a file-per-table tablespace name from m_filepath; else it is a
general tablespace, so just call it that for now. The value of m_name
will be freed in the destructor.
@param[in]      name    Tablespace Name if known, nullptr if not */
void Datafile::set_name(const char *name) {
  ut::free(m_name);

  if (name != nullptr) {
    m_name = mem_strdup(name);
  } else if (fsp_is_file_per_table(m_space_id, m_flags)) {
    m_name = fil_path_to_space_name(m_filepath);
#ifndef UNIV_HOTBACKUP
  } else if (fsp_is_undo_tablespace(m_space_id)) {
    m_name = undo::make_space_name(m_space_id);
#endif /* !UNIV_HOTBACKUP */
  } else if (fsp_is_dd_tablespace(m_space_id)) {
    m_name = mem_strdup(dict_sys_t::s_dd_space_name);
  } else {
#ifndef UNIV_HOTBACKUP
    /* Give this general tablespace a temporary name. */
    m_name = static_cast<char *>(ut::malloc_withkey(
        UT_NEW_THIS_FILE_PSI_KEY, strlen(general_space_name) + 20));

    sprintf(m_name, "%s_" SPACE_ID_PF, general_space_name, m_space_id);
#else  /* !UNIV_HOTBACKUP */
    /* Use the absolute path of general tablespaces. Absolute path
    will help MEB to ignore the dirty records from the redo logs
    pertaining to the same tablespace but with older space_ids.
    It will also not cause name clashes with remote tablespaces or
    tables in schema directory. */
    size_t len = strlen(m_filepath);
    m_name = static_cast<char *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, len + 1));
    memcpy(m_name, m_filepath, len);
    m_name[len] = '\0';
#endif /* !UNIV_HOTBACKUP */
  }
}

/** Reads a few significant fields from the first page of the first
datafile, which must already be open.
@param[in]      read_only_mode  If true, then readonly mode checks are enforced.
@return DB_SUCCESS or DB_IO_ERROR if page cannot be read */
dberr_t Datafile::read_first_page(bool read_only_mode) {
  if (m_handle.m_file == OS_FILE_CLOSED) {
    dberr_t err = open_or_create(read_only_mode);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  /* Align the memory for a possible read from a raw device */
  m_first_page = static_cast<byte *>(
      ut::aligned_alloc(UNIV_PAGE_SIZE_MAX, UNIV_PAGE_SIZE));

  IORequest request;
  dberr_t err = DB_ERROR;
  size_t page_size = UNIV_PAGE_SIZE_MAX;

  /* Don't want unnecessary complaints about partial reads. */

  request.disable_partial_io_warnings();

  while (page_size >= UNIV_PAGE_SIZE_MIN) {
    ulint n_read = 0;

    err = os_file_read_no_error_handling(request, m_filename, m_handle,
                                         m_first_page, 0, page_size, &n_read);

    if (err == DB_IO_ERROR && n_read >= UNIV_PAGE_SIZE_MIN) {
      page_size >>= 1;

    } else if (err == DB_SUCCESS) {
      ut_a(n_read == page_size);

      break;

    } else {
      ib::error(ER_IB_MSG_393) << "Cannot read first page of '" << m_filepath
                               << "' " << ut_strerr(err);
      break;
    }
  }

  if (err == DB_SUCCESS && m_order == 0) {
    m_flags = fsp_header_get_flags(m_first_page);

    m_space_id = fsp_header_get_space_id(m_first_page);

    m_server_version = fsp_header_get_server_version(m_first_page);

    m_space_version = fsp_header_get_space_version(m_first_page);
  }

  return (err);
}

/** Free the first page from memory when it is no longer needed. */
void Datafile::free_first_page() {
  ut::aligned_free(m_first_page);
  m_first_page = nullptr;
}

/** Validates the datafile and checks that it conforms with the expected
space ID and flags.  The file should exist and be successfully opened
in order for this function to validate it.
@param[in]      space_id        The expected tablespace ID.
@param[in]      flags           The expected tablespace flags.
@param[in]      for_import      if it is for importing
@retval DB_SUCCESS if tablespace is valid, DB_ERROR if not.
m_is_valid is also set true on success, else false. */
dberr_t Datafile::validate_to_dd(space_id_t space_id, uint32_t flags,
                                 bool for_import) {
  dberr_t err;

  if (!is_open()) {
    return (DB_ERROR);
  }

  /* Validate this single-table-tablespace with the data dictionary,
  but do not compare the DATA_DIR flag, in case the tablespace was
  remotely located. */
  err = validate_first_page(space_id, nullptr, for_import);
  if (err != DB_SUCCESS) {
    return (err);
  }

  if (m_space_id == space_id && FSP_FLAGS_ARE_NOT_SET(flags) &&
      fsp_is_dd_tablespace(space_id)) {
    return (DB_SUCCESS);
  }

  /* Make sure the datafile we found matched the space ID.
  If the datafile is a file-per-table tablespace then also match
  the row format and zip page size. */

  /* We exclude SDI & DATA_DIR space flags because they are not stored
  in table flags in dictionary */

  if (m_space_id == space_id &&
      !((m_flags ^ flags) & ~(FSP_FLAGS_MASK_DATA_DIR | FSP_FLAGS_MASK_SHARED |
                              FSP_FLAGS_MASK_SDI))) {
    /* Datafile matches the tablespace expected. */
    return (DB_SUCCESS);
  }

  /* For a shared tablesapce, it is possible that encryption flag updated in
  the ibd file, but the server crashed before DD flags are updated. Exclude
  encryption flags for that scenario. */
  if ((FSP_FLAGS_GET_ENCRYPTION(flags) != FSP_FLAGS_GET_ENCRYPTION(m_flags)) &&
      fsp_is_shared_tablespace(flags)) {
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
    /* Note this tablespace id down and assert that it is in the list of
    tablespaces for which encryption is being resumed. */
    flag_mismatch_spaces.push_back(space_id);
#endif
#endif /* !UNIV_HOTBACKUP */

    if (!((m_flags ^ flags) &
          ~(FSP_FLAGS_MASK_ENCRYPTION | FSP_FLAGS_MASK_DATA_DIR |
            FSP_FLAGS_MASK_SHARED | FSP_FLAGS_MASK_SDI))) {
      return (DB_SUCCESS);
    }
  }

  /* else do not use this tablespace. */
  m_is_valid = false;

  ib::error(ER_IB_MSG_394)
      << "In file '" << m_filepath
      << "', tablespace id and"
         " flags are "
      << m_space_id << " and " << m_flags
      << ", but in"
         " the InnoDB data dictionary they are "
      << space_id << " and " << flags
      << ". Have you moved InnoDB .ibd files around without"
         " using the commands DISCARD TABLESPACE and IMPORT TABLESPACE?"
         " "
      << TROUBLESHOOT_DATADICT_MSG;

  return (DB_ERROR);
}

dberr_t Datafile::validate_for_recovery(space_id_t space_id) {
  dberr_t err;

  ut_ad(!srv_read_only_mode);
  ut_ad(is_open());

  err = validate_first_page(space_id, nullptr, false);

  switch (err) {
    case DB_SUCCESS:
    case DB_TABLESPACE_EXISTS:
    case DB_TABLESPACE_NOT_FOUND:
    case DB_INVALID_ENCRYPTION_META:
      break;

    default:
      /* For encryption tablespace, we skip the retry step,
      since it is only because the keyring is not ready. */
      if (FSP_FLAGS_GET_ENCRYPTION(m_flags) && (err != DB_CORRUPTION)) {
        return (err);
      }

      /* Re-open the file in read-write mode  Attempt to restore
      page 0 from doublewrite and read the space ID from a survey
      of the first few pages. */
      err = open_read_write(srv_read_only_mode);
      if (err != DB_SUCCESS) {
        ib::error(ER_IB_MSG_395) << "Datafile '" << m_filepath
                                 << "' could not"
                                    " be opened in read-write mode so that the"
                                    " doublewrite pages could be restored.";
        return (err);
      };

      err = find_space_id();
      if (err != DB_SUCCESS || m_space_id == 0) {
        ib::error(ER_IB_MSG_396)
            << "Datafile '" << m_filepath
            << "' is"
               " corrupted. Cannot determine the space ID from"
               " the first 64 pages.";
        return (err);
      }

      err = restore_from_doublewrite(0);

      if (err != DB_SUCCESS) {
        return (err);
      }

      /* Free the previously read first page and then re-validate. */
      free_first_page();

      err = validate_first_page(space_id, nullptr, false);
  }

  if (err == DB_SUCCESS || err == DB_INVALID_ENCRYPTION_META) {
    set_name(nullptr);
  }

  return (err);
}

dberr_t Datafile::validate_first_page(space_id_t space_id, lsn_t *flush_lsn,
                                      bool for_import) {
  char *prev_name;
  char *prev_filepath;
  const char *error_txt = nullptr;

  m_is_valid = true;

  /* fil_space_read_name_and_filepath will acquire the fil shard mutex. If there
  is any other thread that tries to open this file, it will have the fil
  mutex and will wait for this file to open. It will not succeed on Windows
  as we don't open the file for shared write. */
  auto guard = create_scope_guard([this]() { close(); });

  if (m_first_page == nullptr &&
      read_first_page(srv_read_only_mode) != DB_SUCCESS) {
    error_txt = "Cannot read first page";
  } else {
    ut_ad(m_first_page);

    if (flush_lsn != nullptr) {
      *flush_lsn = mach_read_from_8(m_first_page + FIL_PAGE_FILE_FLUSH_LSN);
    }
  }

  if (error_txt == nullptr && m_space_id == TRX_SYS_SPACE && !m_flags) {
    /* Check if the whole page is blank. */

    const byte *b = m_first_page;
    ulint nonzero_bytes = UNIV_PAGE_SIZE;

    while (*b == '\0' && --nonzero_bytes != 0) {
      b++;
    }

    if (nonzero_bytes == 0) {
      error_txt = "Header page consists of zero bytes";
    }
  }

  const page_size_t page_size(m_flags);

  if (error_txt != nullptr) {
    /* skip the next few tests */
  } else if (univ_page_size.logical() != page_size.logical()) {
    /* Page size must be univ_page_size. */

    ib::error(ER_IB_MSG_397) << "Data file '" << m_filepath
                             << "' uses page size " << page_size.logical()
                             << ", but the innodb_page_size"
                                " start-up parameter is "
                             << univ_page_size.logical();

    free_first_page();

    return (DB_ERROR);
  } else if (!fsp_flags_is_valid(m_flags) || FSP_FLAGS_GET_TEMPORARY(m_flags)) {
    /* Tablespace flags must be valid. */
    error_txt = "Tablespace flags are invalid";
  } else if (page_get_page_no(m_first_page) != 0) {
    /* First page must be number 0 */
    error_txt = "Header page contains inconsistent data";

  } else if (m_space_id == SPACE_UNKNOWN) {
    /* The space_id can be most anything, except -1. */
    error_txt = "A bad Space ID was found";

  } else if (m_space_id != 0 && space_id != m_space_id) {
    /* Tablespace ID mismatch. The file could be in use
    by another tablespace. */

#ifndef UNIV_HOTBACKUP
    ut_d(ib::info(ER_IB_MSG_398)
         << "Tablespace file '" << filepath() << "' ID mismatch"
         << ", expected " << space_id << " but found " << m_space_id);
#else  /* !UNIV_HOTBACKUP */
    ib::trace_2() << "Tablespace file '" << filepath() << "' ID mismatch"
                  << ", expected " << space_id << " but found " << m_space_id;
#endif /* !UNIV_HOTBACKUP */

    return (DB_WRONG_FILE_NAME);

  } else {
    BlockReporter reporter(false, m_first_page, page_size,
                           fsp_is_checksum_disabled(m_space_id));

    if (reporter.is_corrupted()) {
      /* Look for checksum and other corruptions. */
      error_txt = "Checksum mismatch";
    }

    /** TODO: Enable following after WL#11063: Update
    server version information in InnoDB tablespaces:

    else if (!for_import
               && (fsp_header_get_server_version(m_first_page)
                   != DD_SPACE_CURRENT_SRV_VERSION)) {
            error_txt = "Wrong server version";
    } else if (!for_import
               && (fsp_header_get_space_version(m_first_page)
                   != DD_SPACE_CURRENT_SPACE_VERSION)) {
            error_txt = "Wrong tablespace version";
    } */
  }

  if (error_txt != nullptr) {
    ib::error(ER_IB_MSG_399)
        << error_txt << " in datafile: " << m_filepath
        << ", Space ID:" << m_space_id << ", Flags: " << m_flags << ". "
        << TROUBLESHOOT_DATADICT_MSG;
    m_is_valid = false;

    free_first_page();

    return (DB_CORRUPTION);
  }

  /* For encrypted tablespace, check the encryption info in the
  first page can be decrypt by master key, otherwise, this table
  can't be open. And for importing, we skip checking it. */
  if (FSP_FLAGS_GET_ENCRYPTION(m_flags) && !for_import) {
    m_encryption_key = static_cast<byte *>(
        ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));
    m_encryption_iv = static_cast<byte *>(
        ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));
#ifdef UNIV_ENCRYPT_DEBUG
    fprintf(stderr, "Got from file %u:", m_space_id);
#endif

    Encryption_key e_key{m_encryption_key, m_encryption_iv};
    if (!fsp_header_get_encryption_key(m_flags, e_key, m_first_page)) {
      ib::error(ER_IB_MSG_401)
          << "Encryption information in datafile: " << m_filepath
          << " can't be decrypted, please confirm that"
             " keyring is loaded.";

      m_is_valid = false;
      free_first_page();
      ut::free(m_encryption_key);
      ut::free(m_encryption_iv);
      m_encryption_key = nullptr;
      m_encryption_iv = nullptr;
      return (DB_INVALID_ENCRYPTION_META);
    } else {
#ifdef UNIV_DEBUG
      ib::info(ER_IB_MSG_402) << "Read encryption metadata from " << m_filepath
                              << " successfully, encryption"
                              << " of this tablespace enabled.";
#endif
      m_encryption_master_key_id = e_key.m_master_key_id;
    }

    if (recv_recovery_is_on() &&
        memcmp(m_encryption_key, m_encryption_iv, Encryption::KEY_LEN) == 0) {
      ut::free(m_encryption_key);
      ut::free(m_encryption_iv);
      m_encryption_key = nullptr;
      m_encryption_iv = nullptr;
    }
  }
#ifndef UNIV_HOTBACKUP
  /* Set encryption operation in progress based on operation type
  at page 0. */
  m_encryption_op_in_progress =
      fsp_header_encryption_op_type_in_progress(m_first_page, page_size);
#endif /* UNIV_HOTBACKUP */

  if (fil_space_read_name_and_filepath(m_space_id, &prev_name,
                                       &prev_filepath)) {
    if (0 == strcmp(m_filepath, prev_filepath)) {
      ut::free(prev_name);
      ut::free(prev_filepath);
      return (DB_SUCCESS);
    }

    /* Make sure the space_id has not already been opened. */
    ib::error(ER_IB_MSG_403) << "Attempted to open a previously opened"
                                " tablespace. Previous tablespace "
                             << prev_name << " at filepath: " << prev_filepath
                             << " uses space ID: " << m_space_id
                             << ". Cannot open filepath: " << m_filepath
                             << " which uses the same space ID.";

    ut::free(prev_name);
    ut::free(prev_filepath);

    m_is_valid = false;

    free_first_page();

    return (DB_TABLESPACE_EXISTS);
  }

  return (DB_SUCCESS);
}

/** Determine the space id of the given file descriptor by reading a few
pages from the beginning of the .ibd file.
@return DB_SUCCESS if space id was successfully identified, else DB_ERROR. */
dberr_t Datafile::find_space_id() {
  os_offset_t file_size;

  ut_ad(m_handle.m_file != OS_FILE_CLOSED);

  file_size = os_file_get_size(m_handle);

  if (file_size == (os_offset_t)-1) {
    ib::error(ER_IB_MSG_404)
        << "Could not get file size of datafile '" << m_filepath << "'";
    return (DB_CORRUPTION);
  }

  /* Assuming a page size, read the space_id from each page and store it
  in a map.  Find out which space_id is agreed on by majority of the
  pages.  Choose that space_id. */
  for (uint32_t page_size = UNIV_ZIP_SIZE_MIN; page_size <= UNIV_PAGE_SIZE_MAX;
       page_size <<= 1) {
    /* map[space_id] = count of pages */
    typedef std::map<space_id_t, ulint, std::less<space_id_t>,
                     ut::allocator<std::pair<const space_id_t, ulint>>>
        Pages;

    Pages verify;
    ulint page_count = 64;
    ulint valid_pages = 0;

    /* Adjust the number of pages to analyze based on file size */
    while ((page_count * page_size) > file_size) {
      --page_count;
    }

    ib::info(ER_IB_MSG_405)
        << "Page size:" << page_size << ". Pages to analyze:" << page_count;

    byte *page = static_cast<byte *>(
        ut::aligned_alloc(UNIV_PAGE_SIZE_MAX, UNIV_SECTOR_SIZE));

    for (ulint j = 0; j < page_count; ++j) {
      dberr_t err;
      ulint n_bytes = j * page_size;
      IORequest request(IORequest::READ);
      bool encrypted = false;

      err =
          os_file_read(request, m_filename, m_handle, page, n_bytes, page_size);

      if (err == DB_IO_DECRYPT_FAIL) {
        /* At this stage, even if the page decryption failed, we don't have to
        report error now. Currently, only the space_id will be read from the
        page header.  Since page header is unencrypted, we will ignore the
        decryption error for now. */
        encrypted = true;

      } else if (err == DB_IO_DECOMPRESS_FAIL) {
        /* If the page was compressed on the fly then
        try and decompress the page */

        n_bytes = os_file_compressed_page_size(page);

        if (n_bytes != ULINT_UNDEFINED) {
          err = os_file_read(request, m_filename, m_handle, page, page_size,
                             UNIV_PAGE_SIZE_MAX);

          if (err != DB_SUCCESS) {
            ib::info(ER_IB_MSG_406) << "READ FAIL: "
                                    << "page_no:" << j;
            continue;
          }
        }

      } else if (err != DB_SUCCESS) {
        ib::info(ER_IB_MSG_407) << "READ FAIL: page_no:" << j;

        continue;
      }

      bool noncompressed_ok = false;

      /* For noncompressed pages, the page size must be
      equal to univ_page_size.physical(). */
      if (page_size == univ_page_size.physical()) {
        BlockReporter reporter(false, page, univ_page_size, false);

        noncompressed_ok = !reporter.is_corrupted();
      }

      bool compressed_ok = false;

      /* file-per-table tablespaces can be compressed with
      the same physical and logical page size.  General
      tablespaces must have different physical and logical
      page sizes in order to be compressed. For this check,
      assume the page is compressed if univ_page_size.
      logical() is equal to or less than 16k and the
      page_size we are checking is equal to or less than
      univ_page_size.logical(). */
      if (!encrypted && univ_page_size.logical() <= UNIV_PAGE_SIZE_DEF &&
          page_size <= univ_page_size.logical()) {
        const page_size_t compr_page_size(page_size, univ_page_size.logical(),
                                          true);

        BlockReporter reporter(false, page, compr_page_size, false);

        compressed_ok = !reporter.is_corrupted();
      }

      if (noncompressed_ok || compressed_ok || encrypted) {
        space_id_t space_id = mach_read_from_4(page + FIL_PAGE_SPACE_ID);

        if (space_id > 0) {
          ib::info(ER_IB_MSG_408)
              << "VALID: space:" << space_id << " page_no:" << j
              << " page_size:" << page_size;

          ++valid_pages;

          ++verify[space_id];
        }
      }
    }

    ut::aligned_free(page);

    ib::info(ER_IB_MSG_409) << "Page size: " << page_size
                            << ". Possible space_id count:" << verify.size();

    const ulint pages_corrupted = 3;

    for (ulint missed = 0; missed <= pages_corrupted; ++missed) {
      for (Pages::const_iterator it = verify.begin(); it != verify.end();
           ++it) {
        ib::info(ER_IB_MSG_410)
            << "space_id:" << it->first
            << ", Number of pages matched: " << it->second << "/" << valid_pages
            << " (" << page_size << ")";

        if (it->second == (valid_pages - missed)) {
          ib::info(ER_IB_MSG_411) << "Chosen space:" << it->first;

          m_space_id = it->first;
          return (DB_SUCCESS);
        }
      }
    }
  }

  return (DB_CORRUPTION);
}

/** Finds a given page of the given space id from the double write buffer
and copies it to the corresponding .ibd file.
@param[in]      restore_page_no         Page number to restore
@return DB_SUCCESS if page was restored from doublewrite, else DB_ERROR */
dberr_t Datafile::restore_from_doublewrite(page_no_t restore_page_no) {
  ut_a(is_open());
  auto page_id = page_id_t{m_space_id, restore_page_no};

  /* Find if double write buffer contains page_no of given space id. */
  const byte *page = recv_sys->dblwr->find(page_id);

  bool found = false;
  lsn_t reduced_lsn = LSN_MAX;
  std::tie(found, reduced_lsn) = recv_sys->dblwr->find_entry(page_id);

  if (page == nullptr) {
    /* If the first page of the given user tablespace is not there
    in the doublewrite buffer, then the recovery is going to fail
    now. Hence this is treated as an error. */

    if (found && reduced_lsn != LSN_MAX && reduced_lsn != 0) {
      ib::fatal(UT_LOCATION_HERE, ER_REDUCED_DBLWR_PAGE_FOUND, m_filepath,
                page_id.space(), page_id.page_no());
    } else {
      ib::error(ER_IB_MSG_412)
          << "Corrupted page " << page_id_t(m_space_id, restore_page_no)
          << " of datafile '" << m_filepath
          << "' could not be found in the doublewrite buffer.";
    }
    return (DB_CORRUPTION);
  }

  const lsn_t dblwr_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

  if (found && reduced_lsn != LSN_MAX && reduced_lsn > dblwr_lsn) {
    ib::fatal(UT_LOCATION_HERE, ER_REDUCED_DBLWR_PAGE_FOUND, m_filepath,
              page_id.space(), page_id.page_no());
  }

  const uint32_t flags = fsp_header_get_field(page, FSP_SPACE_FLAGS);

  const page_size_t page_size(flags);

  ut_a(page_get_page_no(page) == restore_page_no);

  ib::info(ER_IB_MSG_413) << "Restoring page "
                          << page_id_t(m_space_id, restore_page_no)
                          << " of datafile '" << m_filepath
                          << "' from the doublewrite buffer. Writing "
                          << page_size.physical() << " bytes into file '"
                          << m_filepath << "'";

  IORequest request(IORequest::WRITE);

  /* Note: The pages are written out as uncompressed because we don't
  have the compression algorithm information at this point. */

  request.disable_compression();

  return (os_file_write(request, m_filepath, m_handle, page, 0,
                        page_size.physical()));
}
