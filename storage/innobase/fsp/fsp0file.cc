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

/** Release the resources. */
void Datafile::shutdown() {
  close();

  free_filepath();

  if (m_encryption_key != nullptr) {
    ut::free(m_encryption_key);
    m_encryption_key = nullptr;
  }

  if (m_encryption_iv != nullptr) {
    ut::free(m_encryption_iv);
    m_encryption_iv = nullptr;
  }
}
dberr_t Datafile::open_read_only() {
  bool success = false;
  ut_ad(m_handle.m_file == OS_FILE_CLOSED);

  /* This function can be called for file objects that do not need
  to be opened, which is the case when the m_filepath is NULL */
  if (m_filepath == nullptr) {
    return (DB_ERROR);
  }

  m_handle = os_file_create_simple_no_error_handling(
      innodb_data_file_key, m_filepath, OS_FILE_OPEN, OS_FILE_READ_ONLY,
      &success);

  if (success) {
    m_exists = true;

    return (DB_SUCCESS);
  }

  return (DB_CANNOT_OPEN_FILE);
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

ut::Expected<ut::unique_ptr_aligned<byte[]>> Datafile::read_first_page(
    space_id_t space_id, uint32_t physical_page_size) {
  ut_a(is_open());

  /* Align the memory for unbuffered IO. */
  auto page =
      ut::make_unique_aligned<byte[]>(UNIV_PAGE_SIZE, physical_page_size);

  /* Don't want unnecessary complaints about partial reads. The first page will
  not be compressed nor encrypted, we don't need to even consider transforming
  it. */
  IORequest request{IORequest::Type::READ |
                    IORequest::Type::DISABLE_PARTIAL_IO_WARNINGS |
                    IORequest::Type::NO_COMPRESSION};

  ulint n_read = 0;

  if (const auto err = os_file_read_no_error_handling(
          request, m_filename, m_handle, page.get(), 0, physical_page_size,
          &n_read);
      err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_393) << "Cannot read first page of '" << m_filepath
                             << "' " << ut_strerr(err);
    return ut::Unexpected{err};
  }
  ut_a(n_read == physical_page_size);

#ifndef UNIV_HOTBACKUP
  /* If Double-Write Buffer is available, in case the page is corrupted, check
  if it can be recovered from the Double-Write Buffer. */
  if (!recv_sys->dblwr->empty()) {
    const auto logical_page_size =
        std::max((uint32)UNIV_PAGE_SIZE, physical_page_size);
    /* This will set page_for_recovery to either the page.get() supplied or a
    page from Double-Write Buffer, or nullptr if it is corrupted and can't be
    recovered. Check `get_first_page_content_for_recovery()`'s documentation. */
    auto page_for_recovery =
        recv_sys->dblwr->get_first_page_content_for_recovery(
            space_id,
            page_size_t{physical_page_size, logical_page_size,
                        physical_page_size < logical_page_size},
            m_filepath, page.get());

    if (page_for_recovery == nullptr) {
      if (physical_page_size == logical_page_size) {
        /* A case where physical and logical page size are equal, but table is
        compressed */
        page_for_recovery =
            recv_sys->dblwr->get_first_page_content_for_recovery(
                space_id,
                page_size_t{physical_page_size, logical_page_size, true},
                m_filepath, page.get());
      }

      if (page_for_recovery == nullptr) {
        ib::error(ER_IB_MSG_CORRUPTED_PAGE_NOT_RECOVERABLE,
                  page_id_t(space_id, 0).to_string().c_str(), m_filepath);
        return ut::Unexpected{DB_CORRUPTION};
      }
    }
    if (page_for_recovery != page.get()) {
      memcpy(page.get(), page_for_recovery, physical_page_size);
    }
  }
#endif

  return page;
}

void Datafile::extract_fields_from_first_page(const byte *page) {
  m_first_page_fields_cache.m_space_flags = fsp_header_get_flags(page);
  m_first_page_fields_cache.m_space_id = fsp_header_get_space_id(page);
  m_first_page_fields_cache.m_server_version =
      fsp_header_get_server_version(page);
  m_first_page_fields_cache.m_space_version =
      fsp_header_get_space_version(page);
  m_first_page_fields_cache.m_flush_lsn = mach_read_from_8(page + FIL_PAGE_LSN);
  m_first_page_fields_cache.m_is_valid = true;
}

dberr_t Datafile::validate_to_dd(const byte *page, space_id_t space_id,
                                 uint32_t flags, const std::string &filepath,
                                 bool for_import) {
  dberr_t err;

  extract_fields_from_first_page(page);

  /* Validate this single-table-tablespace with the data dictionary,
  but do not compare the DATA_DIR flag, in case the tablespace was
  remotely located. */
  err = validate_first_page(page, space_id, filepath, for_import);
  if (err != DB_SUCCESS) {
    return (err);
  }

  if (FSP_FLAGS_ARE_NOT_SET(flags) && fsp_is_dd_tablespace(space_id)) {
    return (DB_SUCCESS);
  }

  /* Make sure the datafile we found matched the space ID.
  If the datafile is a file-per-table tablespace then also match
  the row format and zip page size. */

  /* We exclude SDI & DATA_DIR space flags because they are not stored
  in table flags in dictionary */

  if (!((get_cached_space_flags() ^ flags) &
        ~(FSP_FLAGS_MASK_DATA_DIR | FSP_FLAGS_MASK_SHARED |
          FSP_FLAGS_MASK_SDI))) {
    /* Datafile matches the tablespace expected. */
    return (DB_SUCCESS);
  }

  /* For a shared tablespace, it is possible that encryption flag updated in
  the ibd file, but the server crashed before DD flags are updated. Exclude
  encryption flags for that scenario. */
  if ((FSP_FLAGS_GET_ENCRYPTION(flags) !=
       FSP_FLAGS_GET_ENCRYPTION(get_cached_space_flags())) &&
      fsp_is_shared_tablespace(flags)) {
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
    /* Note this tablespace id down and assert that it is in the list of
    tablespaces for which encryption is being resumed. */
    flag_mismatch_spaces.push_back(space_id);
#endif
#endif /* !UNIV_HOTBACKUP */

    if (!((get_cached_space_flags() ^ flags) &
          ~(FSP_FLAGS_MASK_ENCRYPTION | FSP_FLAGS_MASK_DATA_DIR |
            FSP_FLAGS_MASK_SHARED | FSP_FLAGS_MASK_SDI))) {
      return (DB_SUCCESS);
    }
  }

  /* else do not use this tablespace. */
  m_is_valid = false;

  ib::error(ER_IB_MSG_394)
      << "In file '" << filepath
      << "', tablespace id and"
         " flags are "
      << get_cached_space_id() << " and " << get_cached_space_flags()
      << ", but in"
         " the InnoDB data dictionary they are "
      << space_id << " and " << flags
      << ". Have you moved InnoDB .ibd files around without"
         " using the commands DISCARD TABLESPACE and IMPORT TABLESPACE?"
         " "
      << TROUBLESHOOT_DATADICT_MSG;

  return (DB_ERROR);
}

dberr_t Datafile::validate_for_recovery(space_id_t space_id,
                                        uint32_t expected_physical_page_size) {
  ut_ad(!srv_read_only_mode);
  ut_a(is_open());
  m_is_valid = false;

  const auto first_page =
      read_first_page(space_id, expected_physical_page_size);
  if (!first_page) {
    ib::error(ER_IB_MSG_DATAFILE_VALIDATION_ERROR, "Cannot read first page",
              m_filepath, (ulong)space_id, (ulong)0, TROUBLESHOOT_DATADICT_URL);
    close();
    return DB_CORRUPTION;
  }

  extract_fields_from_first_page(first_page->get());

#ifndef UNIV_HOTBACKUP
  ut_ad(physical_page_size() == expected_physical_page_size);
#else
  /* In MEB we don't have the expected physical page size extracted from the
  tablespace scanning, nor we run the Double-write recovery. Therefore, after
  reading the header, we extract FSP flags of the tablespace and the page size
  from these flags and use it to re-read the first page. */
  if (physical_page_size() != expected_physical_page_size) {
    return validate_for_recovery(space_id, physical_page_size());
  }
#endif /* !UNIV_HOTBACKUP */

  /* validate_first_page() -> fsp_header_validate() ->
  fil_space_read_name_and_filepath() will acquire the fil shard mutex. If there
  is any other thread that tries to open this file, it will have the fil
  mutex and will wait for this file to open. It will not succeed on Windows
  as we don't open the file for shared write. This should not happen as long as
  redo log recovery apply is single thread, but we need to close this file
  either way, so we do that early. We can't execute it before the
  validate_for_recovery() recurse call above. */
  close();

  const auto err =
      validate_first_page(first_page->get(), space_id, m_filepath, false);

  return err;
}

dberr_t Datafile::validate_first_page(const byte *page, space_id_t space_id,
                                      const std::string &filepath,
                                      bool for_import) {
  ut_a(m_encryption_key == nullptr);
  ut_a(m_encryption_iv == nullptr);
  m_encryption_key = static_cast<byte *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));
  m_encryption_iv = static_cast<byte *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));

  Encryption_key encryption_key{m_encryption_key, m_encryption_iv};
  uint32_t validated_space_flags;
  const auto err = fsp_header_validate(page, space_id, validated_space_flags,
                                       filepath, for_import, encryption_key);
  ut_a(get_cached_space_flags() == validated_space_flags);

#ifndef UNIV_HOTBACKUP
  /* Set encryption operation in progress based on operation type at page 0. */
  m_encryption_op_in_progress = fsp_header_encryption_op_type_in_progress(
      page, page_size_t{get_cached_space_flags()});
#endif /* UNIV_HOTBACKUP */

  if (recv_recovery_is_on() && memcmp(encryption_key.m_key, encryption_key.m_iv,
                                      Encryption::KEY_LEN) == 0) {
    ut::free(m_encryption_key);
    ut::free(m_encryption_iv);
    m_encryption_key = nullptr;
    m_encryption_iv = nullptr;
  }

  m_is_valid = err == DB_SUCCESS;
  ut_a(get_cached_space_id() == fsp_header_get_space_id(page));
  ut_a(get_cached_server_version() == fsp_header_get_server_version(page));
  ut_a(get_cached_space_version() == fsp_header_get_space_version(page));
  return err;
}
