/*****************************************************************************

Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

/** @file fsp/fsp0space.cc
 Multi file, shared, system tablespace implementation.

 Created 2012-11-16 by Sunny Bains as srv/srv0space.cc
 Refactored 2013-7-26 by Kevin Lewis
 *******************************************************/

#include <stdlib.h>
#include <sys/types.h>

#include "dict0load.h"
#include "fsp0sysspace.h"
#ifndef UNIV_HOTBACKUP
#include "ha_prototypes.h"
#include "mem0mem.h"

/** The server header file is included to access opt_initialize global variable.
If server passes the option for create/open DB to SE, we should remove such
direct reference to server header and global variable */
#include "mysqld.h"
#endif /* !UNIV_HOTBACKUP */
#include "os0file.h"
#ifndef UNIV_HOTBACKUP
#include "row0mysql.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0start.h"
#include "trx0sys.h"
#include "ut0new.h"

/** The control info of the system tablespace. */
SysTablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
SysTablespace srv_tmp_space;

/** If the last data file is auto-extended, we add this many pages to it
at a time. We have to make this public because it is a config variable. */
ulong sys_tablespace_auto_extend_increment;

#ifdef UNIV_DEBUG
/** Control if extra debug checks need to be done for temporary tablespace.
Default = true that is disable such checks.
This variable is not exposed to end-user but still kept as variable for
developer to enable it during debug. */
bool srv_skip_temp_table_checks_debug = true;
#endif /* UNIV_DEBUG */

/** Put the pointer to the next byte after a valid file name. Note that we must
step over the ':' in a Windows filepath.
A Windows path normally looks like "C:\ibdata\ibdata1:1G", but a Windows raw
partition may have a specification like "\\.\C::1Gnewraw" or
"\\.\PHYSICALDRIVE2:1Gnewraw".
@param[in]      ptr             system tablespace file path spec
@return next character in string after the file name */
char *SysTablespace::parse_file_name(char *ptr) {
  const char *start = ptr;

  while ((*ptr != ':' && *ptr != '\0') ||
         (ptr != start && *ptr == ':' &&
          (*(ptr + 1) == '\\' || *(ptr + 1) == '/' || *(ptr + 1) == ':'))) {
    ptr++;
  }

  return (ptr);
}

/** Convert a numeric string representing a number of bytes
optionally ending in upper or lower case G, M, or K,
to a number of megabytes, rounding down to the nearest megabyte.
Then return the number of pages in the file.
@param[in,out]  ptr     Pointer to a numeric string
@return the number of pages in the file. */
page_no_t SysTablespace::parse_units(char *&ptr) {
  char *endp;
  ulint num = strtoul(ptr, &endp, 10);
  ulint megs;

  ptr = endp;

  switch (*ptr) {
    case 'G':
    case 'g':
      megs = num * 1024;
      ++ptr;
      break;

    case 'M':
    case 'm':
      megs = num;
      ++ptr;
      break;
    case 'K':
    case 'k':
      megs = num / 1024;
      ++ptr;
      break;
    default:
      megs = num / (1024 * 1024);
      break;
  }

  return (static_cast<page_no_t>(megs * (1024 * 1024 / UNIV_PAGE_SIZE)));
}

/** Parse the input params and populate member variables.
@param[in]      filepath_spec   path to data files
@param[in]      supports_raw    true if the tablespace supports raw devices
@return true on success parse */
bool SysTablespace::parse_params(const char *filepath_spec, bool supports_raw) {
  char *filepath;
  page_no_t size;
  ulint n_files = 0;

  ut_ad(m_last_file_size_max == 0);
  ut_ad(!m_auto_extend_last_file);

  char *input_str = mem_strdup(filepath_spec);
  char *ptr = input_str;

  /*---------------------- PASS 1 ---------------------------*/
  /* First calculate the number of data files and check syntax. */
  while (*ptr != '\0') {
    filepath = ptr;

    ptr = parse_file_name(ptr);

    if (ptr == filepath) {
      ib::error(ER_IB_MSG_431) << "File Path Specification '" << filepath_spec
                               << "' is missing a file name.";

      ut::free(input_str);
      return (false);
    }

    if (*ptr == '\0') {
      ib::error(ER_IB_MSG_432) << "File Path Specification '" << filepath_spec
                               << "' is missing a file size.";

      ut::free(input_str);
      return (false);
    }

    ptr++;

    size = parse_units(ptr);

    if (size == 0) {
    invalid_size:
      ib::error(ER_IB_MSG_433)
          << "Invalid File Path Specification: '" << filepath_spec
          << "'. An invalid file size was specified.";

      ut::free(input_str);
      return (false);
    }

    if (0 == strncmp(ptr, ":autoextend", (sizeof ":autoextend") - 1)) {
      ptr += (sizeof ":autoextend") - 1;

      if (0 == strncmp(ptr, ":max:", (sizeof ":max:") - 1)) {
        ptr += (sizeof ":max:") - 1;

        page_no_t max = parse_units(ptr);

        if (max < size) {
          goto invalid_size;
        }
      }

      if (*ptr == ';') {
        ib::error(ER_IB_MSG_434)
            << "Invalid File Path Specification: '" << filepath_spec
            << "'. Only the last"
               " file defined can be 'autoextend'.";

        ut::free(input_str);
        return (false);
      }
    }

    if (0 == strncmp(ptr, "new", (sizeof "new") - 1)) {
      ptr += (sizeof "new") - 1;
    }

    if (0 == strncmp(ptr, "raw", (sizeof "raw") - 1)) {
      if (!supports_raw) {
        ib::error(ER_IB_MSG_435)
            << "Invalid File Path Specification: '" << filepath_spec
            << "' Tablespace"
               " doesn't support raw devices";

        ut::free(input_str);
        return (false);
      }

      ptr += (sizeof "raw") - 1;
    }

    ++n_files;

    if (*ptr == ';') {
      ptr++;
    } else if (*ptr != '\0') {
      ptr[0] = '\0';
      ib::error(ER_IB_MSG_436)
          << "File Path Specification: '" << filepath_spec
          << "' has unrecognized characters after '" << input_str << "'";

      ut::free(input_str);
      return (false);
    }
  }

  if (n_files == 0) {
    ib::error(ER_IB_MSG_437) << "File Path Specification: '" << filepath_spec
                             << "' must contain"
                                " at least one data file definition";

    ut::free(input_str);
    return (false);
  }

  /*---------------------- PASS 2 ---------------------------*/
  /* Then store the actual values to our arrays */
  ptr = input_str;
  ulint order = 0;

  while (*ptr != '\0') {
    filepath = ptr;

    ptr = parse_file_name(ptr);

    if (*ptr == ':') {
      /* Make filepath a null-terminated string */
      *ptr = '\0';
      ptr++;
    }

    size = parse_units(ptr);
    ut_ad(size > 0);

    if (0 == strncmp(ptr, ":autoextend", (sizeof ":autoextend") - 1)) {
      m_auto_extend_last_file = true;

      ptr += (sizeof ":autoextend") - 1;

      if (0 == strncmp(ptr, ":max:", (sizeof ":max:") - 1)) {
        ptr += (sizeof ":max:") - 1;

        m_last_file_size_max = parse_units(ptr);
      }
    }

    m_files.push_back(Datafile(filepath, flags(), size, order));
    Datafile *datafile = &m_files.back();
    datafile->make_filepath(path(), filepath, NO_EXT);

    if (0 == strncmp(ptr, "new", (sizeof "new") - 1)) {
      ptr += (sizeof "new") - 1;
    }

    if (0 == strncmp(ptr, "raw", (sizeof "raw") - 1)) {
      ut_a(supports_raw);

      ptr += (sizeof "raw") - 1;

      /* Initialize new raw device only during initialize */
      m_files.back().m_type =
#ifndef UNIV_HOTBACKUP
          opt_initialize ? SRV_NEW_RAW : SRV_OLD_RAW;
#else  /* !UNIV_HOTBACKUP */
          SRV_OLD_RAW;
#endif /* !UNIV_HOTBACKUP */
    }

    if (*ptr == ';') {
      ++ptr;
    }
    order++;
  }

  ut_ad(n_files == ulint(m_files.size()));

  ut::free(input_str);

  return (true);
}

/** Frees the memory allocated by the parse method. */
void SysTablespace::shutdown() {
  Tablespace::shutdown();

  m_auto_extend_last_file = false;
  m_last_file_size_max = 0;
  m_created_new_raw = false;
  m_is_tablespace_full = false;
  m_sanity_checks_done = false;
}

/** Verify the size of the physical file.
@param[in]      file    data file object
@return DB_SUCCESS if OK else error code. */
dberr_t SysTablespace::check_size(Datafile &file) {
  os_offset_t size = os_file_get_size(file.m_handle);
  ut_a(size != (os_offset_t)-1);

  /* Under some error conditions like disk full scenarios
  or file size reaching filesystem limit the data file
  could contain an incomplete extent at the end. When we
  extend a data file and if some failure happens, then
  also the data file could contain an incomplete extent.
  So we need to round the size downward to a megabyte. */

  page_no_t rounded_size_pages = get_pages_from_size(size);

  /* If last file */
  if (&file == &m_files.back() && m_auto_extend_last_file) {
    if (file.m_size > rounded_size_pages ||
        (m_last_file_size_max > 0 &&
         m_last_file_size_max < rounded_size_pages)) {
      ib::error(ER_IB_MSG_438)
          << "The Auto-extending " << name() << " data file '"
          << file.filepath()
          << "' is"
             " of a different size "
          << rounded_size_pages
          << " pages (rounded down to MB) than specified"
             " in the .cnf file: initial "
          << file.m_size << " pages, max " << m_last_file_size_max
          << " (relevant if non-zero) pages!";
      return (DB_ERROR);
    }

    file.m_size = rounded_size_pages;
  }

  if (rounded_size_pages != file.m_size) {
    ib::error(ER_IB_MSG_439)
        << "The " << name() << " data file '" << file.filepath()
        << "' is of a different size " << rounded_size_pages
        << " pages (rounded down to MB)"
           " than the "
        << file.m_size
        << " pages specified in"
           " the .cnf file!";
    return (DB_ERROR);
  }

  return (DB_SUCCESS);
}

/** Set the size of the file.
@param[in,out]  file    data file object
@return DB_SUCCESS or error code */
dberr_t SysTablespace::set_size(Datafile &file) {
  ut_a(!srv_read_only_mode || m_ignore_read_only);

  /* We created the data file and now write it full of zeros */
  ib::info(ER_IB_MSG_440)
      << "Setting file '" << file.filepath() << "' size to "
      << (file.m_size >> (20 - UNIV_PAGE_SIZE_SHIFT))
      << " MB."
         " Physically writing the file full; Please wait ...";

  bool success = os_file_set_size(
      file.m_filepath, file.m_handle, 0,
      static_cast<os_offset_t>(file.m_size) << UNIV_PAGE_SIZE_SHIFT, true);

  if (success) {
    ib::info(ER_IB_MSG_441)
        << "File '" << file.filepath() << "' size is now "
        << (file.m_size >> (20 - UNIV_PAGE_SIZE_SHIFT)) << " MB.";
  } else {
    ib::error(ER_IB_MSG_442)
        << "Could not set the file size of '" << file.filepath()
        << "'. Probably out of disk space";

    return (DB_ERROR);
  }

  return (DB_SUCCESS);
}

/** Create a data file.
@param[in,out]  file    data file object
@return DB_SUCCESS or error code */
dberr_t SysTablespace::create_file(Datafile &file) {
  dberr_t err = DB_SUCCESS;

  ut_a(!file.m_exists);
  ut_a(!srv_read_only_mode || m_ignore_read_only);

  switch (file.m_type) {
    case SRV_NEW_RAW:

      /* The partition is opened, not created; then it is
      written over */
      m_created_new_raw = true;

      [[fallthrough]];

    case SRV_OLD_RAW:

      srv_start_raw_disk_in_use = true;

      [[fallthrough]];

    case SRV_NOT_RAW:
      err =
          file.open_or_create(m_ignore_read_only ? false : srv_read_only_mode);
      break;
  }

  if (err == DB_SUCCESS && file.m_type != SRV_OLD_RAW) {
    err = set_size(file);
  }

  return (err);
}

/** Open a data file.
@param[in,out]  file    data file object
@return DB_SUCCESS or error code */
dberr_t SysTablespace::open_file(Datafile &file) {
  dberr_t err = DB_SUCCESS;

  ut_a(file.m_exists);

  switch (file.m_type) {
    case SRV_NEW_RAW:
      /* The partition is opened, not created; then it is
      written over */
      m_created_new_raw = true;

      [[fallthrough]];

    case SRV_OLD_RAW:
      srv_start_raw_disk_in_use = true;

      if (srv_read_only_mode && !m_ignore_read_only) {
        ib::error(ER_IB_MSG_443)
            << "Can't open a raw device '" << file.m_filepath
            << "' when"
               " --innodb-read-only is set";

        return (DB_ERROR);
      }

      [[fallthrough]];

    case SRV_NOT_RAW:
      err =
          file.open_or_create(m_ignore_read_only ? false : srv_read_only_mode);

      if (err != DB_SUCCESS) {
        return (err);
      }
      break;
  }

  switch (file.m_type) {
    case SRV_NEW_RAW:
      /* Set file size for new raw device. */
      err = set_size(file);
      break;

    case SRV_NOT_RAW:
      /* Check file size for existing file. */
      err = check_size(file);
      break;

    case SRV_OLD_RAW:
      err = DB_SUCCESS;
      break;
  }

  if (err != DB_SUCCESS) {
    file.close();
  }

  return (err);
}

#ifndef UNIV_HOTBACKUP

dberr_t SysTablespace::read_lsn_and_check_flags(lsn_t *flushed_lsn) {
  /* Only relevant for the system tablespace. */
  ut_ad(space_id() == TRX_SYS_SPACE);

  files_t::iterator it = m_files.begin();

  ut_a(it->m_exists);
  ut_ad(it->is_open());
  dberr_t err = it->read_first_page();

  if (err != DB_SUCCESS) {
    return (err);
  }

  ut_a(it->order() == 0);

  err = recv_sys->dblwr->load();

  if (err != DB_SUCCESS) {
    return (err);
  }

  err = recv_sys->dblwr->reduced_load();

  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Check the contents of the first page of the first datafile. */
  err = it->validate_first_page(space_id(), flushed_lsn, false);
  if (err != DB_SUCCESS) {
    /* Perhaps the first page was torn? Recover it and validate again. */
    if (it->open_or_create(srv_read_only_mode) != DB_SUCCESS ||
        it->restore_from_doublewrite(0) != DB_SUCCESS) {
      it->close();
      return err;
    }
    err = it->validate_first_page(space_id(), flushed_lsn, false);
    if (err != DB_SUCCESS) {
      return err;
    }
  }
  ut_ad(!it->is_open());

  /* The flags of srv_sys_space do not have SDI Flag set.
  Update the flags of system tablespace to indicate the presence
  of SDI */
  set_flags(it->flags());

  return (DB_SUCCESS);
}

/** Check if a file can be opened in the correct mode.
@param[in,out]  file    data file object
@param[out]     reason  exact reason if file_status check failed.
@return DB_SUCCESS or error code. */
dberr_t SysTablespace::check_file_status(const Datafile &file,
                                         file_status_t &reason) {
  os_file_stat_t stat;

  memset(&stat, 0x0, sizeof(stat));

  dberr_t err =
      os_file_get_status(file.m_filepath, &stat, true,
                         m_ignore_read_only ? false : srv_read_only_mode);

  reason = FILE_STATUS_VOID;
  /* File exists but we can't read the rw-permission settings. */
  switch (err) {
    case DB_FAIL:
      ib::error(ER_IB_MSG_445)
          << "os_file_get_status() failed on '" << file.filepath()
          << "'. Can't determine file permissions";
      err = DB_ERROR;
      reason = FILE_STATUS_RW_PERMISSION_ERROR;
      break;

    case DB_SUCCESS:

      /* Note: stat.rw_perm is only valid for "regular" files */

      if (stat.type == OS_FILE_TYPE_FILE) {
        if (!stat.rw_perm) {
          const char *p = (!srv_read_only_mode || m_ignore_read_only)
                              ? "writable"
                              : "readable";

          ib::error(ER_IB_MSG_446) << "The " << name() << " data file"
                                   << " '" << file.name() << "' must be " << p;

          err = DB_ERROR;
          reason = FILE_STATUS_READ_WRITE_ERROR;
        }

      } else {
        /* Not a regular file, bail out. */
        ib::error(ER_IB_MSG_447)
            << "The " << name() << " data file '" << file.name()
            << "' is not a regular"
               " InnoDB data file.";

        err = DB_ERROR;
        reason = FILE_STATUS_NOT_REGULAR_FILE_ERROR;
      }
      break;

    case DB_NOT_FOUND:
      break;

    default:
      ut_d(ut_error);
  }

  return (err);
}

/** Note that the data file was not found.
@param[in]      file            data file object
@param[in]      create_new_db   true if a new instance to be created
@return DB_SUCCESS or error code */
dberr_t SysTablespace::file_not_found(Datafile &file, bool create_new_db) {
  file.m_exists = false;

  if (srv_read_only_mode && !m_ignore_read_only) {
    ib::error(ER_IB_MSG_448) << "Can't create file '" << file.filepath()
                             << "' when --innodb-read-only is set";

    return (DB_ERROR);

  } else if (&file == &m_files.front()) {
    /* First data file. */

    if (space_id() == TRX_SYS_SPACE && create_new_db) {
      ib::info(ER_IB_MSG_449)
          << "The first " << name() << " data file '" << file.name()
          << "' did not exist."
             " A new tablespace will be created!";
    }

  } else {
    ib::info(ER_IB_MSG_450) << "Need to create a new " << name()
                            << " data file '" << file.name() << "'.";
  }

  /* We allow add new files at end even if dict_init_mode is
  not creating files. */
  if (!create_new_db && (&file == &m_files.front())) {
    return (DB_SUCCESS);
  }

  /* Set the file create mode. */
  switch (file.m_type) {
    case SRV_NOT_RAW:
      file.set_open_flags(OS_FILE_CREATE);
      break;

    case SRV_NEW_RAW:
    case SRV_OLD_RAW:
      file.set_open_flags(OS_FILE_OPEN_RAW);
      break;
  }

  return (DB_SUCCESS);
}

/** Note that the data file was found.
@param[in,out]  file    data file object */
void SysTablespace::file_found(Datafile &file) {
  /* Note that the file exists and can be opened
  in the appropriate mode. */
  file.m_exists = true;

  /* Set the file open mode */
  switch (file.m_type) {
    case SRV_NOT_RAW:
      file.set_open_flags(&file == &m_files.front() ? OS_FILE_OPEN_RETRY
                                                    : OS_FILE_OPEN);
      break;

    case SRV_NEW_RAW:
    case SRV_OLD_RAW:
      file.set_open_flags(OS_FILE_OPEN_RAW);
      break;
  }
}

/** Check the data file specification.
@param[in]  create_new_db     True if a new database is to be created
@param[in]  min_expected_size Minimum expected tablespace size in bytes
@return DB_SUCCESS if all OK else error code */
dberr_t SysTablespace::check_file_spec(bool create_new_db,
                                       ulint min_expected_size) {
  if (m_files.size() >= 1000) {
    ib::error(ER_IB_MSG_451) << "There must be < 1000 data files in " << name()
                             << " but " << m_files.size()
                             << " have been"
                                " defined.";

    return (DB_ERROR);
  }

  if (get_sum_of_sizes() < min_expected_size / UNIV_PAGE_SIZE) {
    ib::error(ER_IB_MSG_452) << "Tablespace size must be at least "
                             << min_expected_size / (1024 * 1024) << " MB";

    return (DB_ERROR);
  }

  dberr_t err = DB_SUCCESS;

  ut_a(!m_files.empty());

  /* If there is more than one data file and the last data file
  doesn't exist, that is OK. We allow adding of new data files. */

  files_t::iterator begin = m_files.begin();
  files_t::iterator end = m_files.end();

  for (files_t::iterator it = begin; it != end; ++it) {
    file_status_t reason_if_failed;
    err = check_file_status(*it, reason_if_failed);

    if (err == DB_NOT_FOUND) {
      err = file_not_found(*it, create_new_db);

      if (err != DB_SUCCESS) {
        break;
      }

    } else if (err != DB_SUCCESS) {
      if (reason_if_failed == FILE_STATUS_READ_WRITE_ERROR) {
        const char *p = (!srv_read_only_mode || m_ignore_read_only)
                            ? "writable"
                            : "readable";
        ib::error(ER_IB_MSG_453) << "The " << name() << " data file"
                                 << " '" << it->name() << "' must be " << p;
      }

      ut_a(err != DB_FAIL);
      break;

    } else if (create_new_db && !(*it).is_raw_type()) {
      ib::error(ER_IB_MSG_454)
          << "The " << name() << " data file '" << begin->m_name
          << "' was not found but"
             " one of the other data files '"
          << it->m_name << "' exists.";

      err = DB_ERROR;
      break;

    } else {
      ut_ad(err == DB_SUCCESS);
      file_found(*it);
    }
  }

  /* We assume doublewirte blocks in the first data file. */
  if (err == DB_SUCCESS && begin->m_size < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 3) {
    ib::error(ER_IB_MSG_455)
        << "The " << name() << " data file "
        << "'" << begin->name() << "' must be at least "
        << TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 3 * UNIV_PAGE_SIZE / (1024 * 1024)
        << " MB";

    err = DB_ERROR;
  }

  return (err);
}

dberr_t SysTablespace::open_or_create(bool is_temp, bool create_new_db,
                                      page_no_t *sum_new_sizes,
                                      lsn_t *flush_lsn) {
  dberr_t err = DB_SUCCESS;
  fil_space_t *space = nullptr;

  ut_ad(!m_files.empty());

  if (sum_new_sizes) {
    *sum_new_sizes = 0;
  }

  files_t::iterator begin = m_files.begin();
  files_t::iterator end = m_files.end();

  ut_ad(begin->order() == 0);

  for (files_t::iterator it = begin; it != end; ++it) {
    if (it->m_exists) {
      err = open_file(*it);

      /* For new raw device increment new size. */
      if (sum_new_sizes && it->m_type == SRV_NEW_RAW) {
        *sum_new_sizes += it->m_size;
      }

    } else {
      err = create_file(*it);

      if (sum_new_sizes) {
        *sum_new_sizes += it->m_size;
      }

      /* Set the correct open flags now that we have
      successfully created the file. */
      if (err == DB_SUCCESS) {
        /* We ignore new_db OUT parameter here
        as the information is known at this stage */
        file_found(*it);
      }
    }

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  if (flush_lsn != nullptr) {
    if (create_new_db) {
      /* There are no data files, so we assign the initial value
      to flush_lsn instead of reading it from disk. */
      *flush_lsn = LOG_START_LSN + LOG_BLOCK_HDR_SIZE;
    } else {
      /* Validate the header page in the first datafile in the
      system tablespace and read flush_lsn from the validated
      header page. */
      err = read_lsn_and_check_flags(flush_lsn);
      if (err != DB_SUCCESS) {
        return (err);
      }
    }
  }

  /* Close the current handles, add space and file info to the
  fil_system cache and the Data Dictionary, and re-open them
  in file_system cache so that they stay open until shutdown. */
  ulint node_counter = 0;
  for (files_t::iterator it = begin; it != end; ++it) {
    it->close();
    it->m_exists = true;

    if (it == begin) {
      /* First data file. */

      /* Create the tablespace entry for the multi-file
      tablespace in the tablespace manager. */
      space =
          fil_space_create(name(), space_id(), flags(),
                           is_temp ? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);
    }

    ut_ad(fil_validate());

    page_no_t max_size =
        (++node_counter == m_files.size()
             ? (m_last_file_size_max == 0 ? PAGE_NO_MAX : m_last_file_size_max)
             : it->m_size);

    /* Add the datafile to the fil_system cache. */
    if (!fil_node_create(it->m_filepath, it->m_size, space,
                         it->m_type != SRV_NOT_RAW, max_size)) {
      err = DB_ERROR;
      break;
    }
  }

  return (err);
}
#endif /* !UNIV_HOTBACKUP */

/**
@return next increment size */
page_no_t SysTablespace::get_increment() const {
  page_no_t increment;

  if (m_last_file_size_max == 0) {
    increment = get_autoextend_increment();
  } else {
    increment = m_last_file_size_max - last_file_size();
  }

  if (increment > get_autoextend_increment()) {
    increment = get_autoextend_increment();
  }

  return (increment);
}
