/*****************************************************************************

Copyright (c) 2013, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file fsp/fsp0space.cc
 General shared tablespace implementation.

 Created 2012-11-16 by Sunny Bains as srv/srv0space.cc
 *******************************************************/

#include "ha_prototypes.h"

#include "fsp0fsp.h"
#include "fsp0space.h"
#include "fsp0sysspace.h"
#include "os0file.h"

#include "my_sys.h"

/** Check if two tablespaces have common data file names.
@param other_space	Tablespace to check against this.
@return true if they have the same data filenames and paths */
bool Tablespace::intersection(const Tablespace *other_space) {
  files_t::const_iterator end = other_space->m_files.end();

  for (files_t::const_iterator it = other_space->m_files.begin(); it != end;
       ++it) {
    if (find(it->m_filename)) {
      return (true);
    }
  }

  return (false);
}

/** Frees the memory allocated by the SysTablespace object. */
void Tablespace::shutdown() {
  files_t::iterator end = m_files.end();

  for (files_t::iterator it = m_files.begin(); it != end; ++it) {
    it->shutdown();
  }

  m_files.clear();

  m_space_id = SPACE_UNKNOWN;
}

/** Note that the data file was found.
@param[in,out] file	Data file object to set */
void Tablespace::file_found(Datafile &file) {
  /* Note that the file exists and can be opened
  in the appropriate mode. */
  file.m_exists = true;

  file.set_open_flags(&file == &m_files.front() ? OS_FILE_OPEN_RETRY
                                                : OS_FILE_OPEN);
}

/** Open or Create the data files if they do not exist.
@param[in]	is_temp	whether this is a temporary tablespace
@return DB_SUCCESS or error code */
dberr_t Tablespace::open_or_create(bool is_temp) {
  fil_space_t *space = NULL;
  dberr_t err = DB_SUCCESS;

  ut_ad(!m_files.empty());

  files_t::iterator begin = m_files.begin();
  files_t::iterator end = m_files.end();

  for (files_t::iterator it = begin; it != end; ++it) {
    if (it->m_exists) {
      err = it->open_or_create(m_ignore_read_only ? false : srv_read_only_mode);
    } else {
      err = it->open_or_create(m_ignore_read_only ? false : srv_read_only_mode);

      /* Set the correct open flags now that we have
      successfully created the file. */
      if (err == DB_SUCCESS) {
        file_found(*it);
      }
    }

    if (err != DB_SUCCESS) {
      break;
    }

    bool atomic_write;

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
    if (!srv_use_doublewrite_buf) {
      atomic_write = fil_fusionio_enable_atomic_write(it->m_handle);
    } else {
      atomic_write = false;
    }
#else
    atomic_write = false;
#endif /* !NO_FALLOCATE && UNIV_LINUX */

    /* We can close the handle now and open the tablespace
    the proper way. */
    it->close();

    if (it == begin) {
      /* First data file. */

      ulint flags;

      flags = fsp_flags_set_page_size(0, univ_page_size);

      /* Create the tablespace entry for the multi-file
      tablespace in the tablespace manager. */
      space =
          fil_space_create(m_name, m_space_id, flags,
                           is_temp ? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);
    }

    ut_ad(fil_validate());

    /* Create the tablespace node entry for this data file. */
    if (!fil_node_create(it->m_filepath, it->m_size, space, false,
                         atomic_write)) {
      err = DB_ERROR;
      break;
    }
  }

  return (err);
}

/** Find a filename in the list of Datafiles for a tablespace
@return true if the filename exists in the data files */
bool Tablespace::find(const char *filename) {
  files_t::const_iterator end = m_files.end();

  for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {
    if (innobase_strcasecmp(filename, it->m_filename) == 0) {
      return (true);
    }
  }

  return (false);
}

/** Delete all the data files. */
void Tablespace::delete_files() {
  files_t::iterator end = m_files.end();

  for (files_t::iterator it = m_files.begin(); it != end; ++it) {
    it->close();

    bool file_pre_exists;
    bool success = os_file_delete_if_exists(innodb_data_file_key,
                                            it->m_filepath, &file_pre_exists);

    if (success && file_pre_exists) {
      ib::info(ER_IB_MSG_430) << "Removed temporary tablespace data"
                                 " file: \""
                              << it->m_name << "\"";
    }
  }
}

/** Use the ADD DATAFILE path to create a Datafile object and add it to the
front of m_files.
Parse the datafile path into a path and a filename with extension 'ibd'.
This datafile_path provided may or may not be an absolute path, but it
must end with the extension .ibd and have a basename of at least 1 byte.

Set tablespace m_path member and add a Datafile with the filename.
@param[in]	datafile_added	full path of the tablespace file. */
dberr_t Tablespace::add_datafile(const char *datafile_added) {
  /* The path provided ends in ".ibd".  This was assured by
  validate_create_tablespace_info() */
  ut_d(const char *dot = strrchr(datafile_added, '.'));
  ut_ad(dot != NULL && Fil_path::has_ibd_suffix(dot));

  std::string filepath{datafile_added};

  Fil_path::normalize(filepath);

  /* If the path is an absolute path, separate it onto m_path and a
  basename. For relative paths, make the whole thing a basename so that
  it can be appended to the datadir. */
  bool is_abs_path = Fil_path::is_absolute_path(filepath);
  size_t dirlen = (is_abs_path ? dirname_length(filepath.c_str()) : 0);

  /* If the pathname contains a directory separator, fill the
  m_path member which is the default directory for files in this
  tablespace. Leave it null otherwise. */
  if (dirlen > 0) {
    set_path(filepath.c_str(), dirlen);
  }

  Datafile value(m_name, m_flags, FIL_IBD_FILE_INITIAL_SIZE, 0);

  /* Now add a new Datafile and set the filepath using the m_path
  created above. */
  m_files.push_back(value);

  Datafile *datafile = &m_files.back();

  if (dirlen > 0) {
    filepath.erase(0, dirlen);
  }

  datafile->make_filepath(m_path, filepath.c_str(), IBD);

  return (DB_SUCCESS);
}
