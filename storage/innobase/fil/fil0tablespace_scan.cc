/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

/** @file fil/fil0tablespace_scan.cc
The scanned tablespaces at startup */

#include "fil0tablespace_scan.h"
#include "os0thread-create.h" /* par_for */
#include "scope_guard.h"

namespace ib::fil {

size_t Scanned_tablespace_dir_and_files::add(Space_read_info space_info,
                                             const std::string &name) {
  const auto space_id = space_info.space_id;
  ut_a(space_id != TRX_SYS_SPACE);

  if (undo_truncate::is_reserved(space_id)) {
    ut_ad(!Fil_path::has_suffix(IBD, name.c_str()));
  } else {
    ut_ad(!Fil_path::has_suffix(IBU, name.c_str()));

    if (0 == strncmp(name.c_str(), "undo_", 5)) {
      ib::warn(ER_IB_MSG_TABLESPACE_NAME_FORMAT_LIKE_UNDO_BUT_ID_IS_NOT,
               name.c_str(), ulong{space_id});
    }
  }

  auto &names = m_id_to_scan_info[space_id];

  names.push_back(Space_scan_info{name, space_info.physical_page_size});

  return names.size();
}

void Scanned_tablespace_dirs::warn_ignore(std::string ignore_path,
                                          const char *reason) {
  ib::warn(ER_IB_MSG_IGNORE_SCAN_PATH, ignore_path.c_str(), reason);
}

void Scanned_tablespace_dirs::add_path(const std::string &path_in) {
  /* Ignore an invalid path. */
  if (path_in == "") {
    return;
  }
  if (path_in == "/") {
    warn_ignore(path_in,
                "the root directory '/' is not allowed to be scanned.");
    return;
  }
  if (std::string::npos != path_in.find('*')) {
    warn_ignore(path_in, "it contains '*'.");
    return;
  }

  /* Assume this path is a directory and put a trailing slash on it. */
  std::string dir_in(path_in);
  Fil_path::append_separator(dir_in);

  Fil_path found_path(dir_in, true);

  /* Exclude this path if it is a duplicate of a path already stored or
  if a previously stored path is an ancestor. Remove any previously stored
  path that is a descendant of this path. */
  for (auto it = m_scanned_dirs_and_files.cbegin();
       it != m_scanned_dirs_and_files.cend();
       /* No op */) {
    if (it->root().is_same_as(found_path)) {
      /* The exact same path is obviously ignored, so there is no need to
      log a warning. */
      return;
    }

    /* Check if dir_abs_path is an ancestor of this path */
    if (it->root().is_ancestor(found_path)) {
      /* Descendant directories will be scanned recursively, so don't
      add it to the scan list. Log a warning. */
      std::string reason = "it is a sub-directory of '";
      reason += it->root().abs_path();
      warn_ignore(path_in, reason.c_str());
      return;
    }

    if (found_path.is_ancestor(it->root())) {
      /* This path is an ancestor of an existing dir in fil_system::m_dirs.
      The settings have overlapping locations.  Put a note about it to
      the error log. The undo_dir is added last, so if it is an ancestor,
      the descendant was listed as a datafile directory. So always issue
      this message*/
      std::string reason = "it is a sub-directory of '";
      reason += found_path;
      warn_ignore(it->root().path(), reason.c_str());

      /* It might also be an ancestor to another dir as well, so keep looking.
      We must delete this descendant because we know that this ancestor path
      will be inserted and all its descendants will be scanned. */
      it = m_scanned_dirs_and_files.erase(it);
    } else {
      it++;
    }
  }

  m_scanned_dirs_and_files.push_back(
      Scanned_tablespace_dir_and_files{found_path.path()});
  return;
}

void Scanned_tablespace_dirs::add_paths(const std::string &str,
                                        const std::string &delimiters) {
  std::string::size_type start = 0;
  std::string::size_type end = 0;

  /* Scan until 'start' reaches the end of the string (npos) */
  for (;;) {
    start = str.find_first_not_of(delimiters, end);
    if (std::string::npos == start) {
      break;
    }

    end = str.find_first_of(delimiters, start);

    const auto path = str.substr(start, end - start);

    add_path(path);
  }
}

void Scanned_tablespace_dirs::duplicate_check(
    const Const_iter &start, const Const_iter &end, size_t thread_id,
    std::mutex *mutex, Space_id_set *unique, Space_id_set *duplicates) {
  size_t count = 0;
  bool printed_msg = false;
  auto start_time = std::chrono::steady_clock::now();

  for (auto it = start; it != end; ++it, ++m_checked) {
    const std::string filename = it->second;
    auto &files = m_scanned_dirs_and_files[it->first];
    const std::string phy_filename = files.path() + filename;

    /* Read the space id from the first (few) page(s) of the file. */
    const auto space_info = get_tablespace_info(phy_filename);
    const auto space_id = space_info.space_id;

    if (space_id != 0 && space_id != dict_sys_t::s_invalid_space_id) {
      std::lock_guard<std::mutex> guard(*mutex);

      auto ret = unique->insert(space_id);

      size_t n_files = files.add(space_info, filename);

      /* n_files > 1 => space_id has more than 1 files.
         !ret.second => there is already an entry for this space_id. */
      if (n_files > 1 || !ret.second) {
        duplicates->insert(space_id);
      }

    } else if (space_id != 0 &&
               Fil_path::is_undo_tablespace_name(phy_filename)) {
      ib::info(ER_IB_MSG_373) << "Can't determine the undo file tablespace"
                              << " ID for '" << phy_filename << "', could be"
                              << " an undo truncate in progress";

    } else {
      ib::info(ER_IB_MSG_374) << "Ignoring '" << phy_filename << "' invalid"
                              << " tablespace ID in the header";
    }

    ++count;

    if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
      ib::info(ER_IB_MSG_375) << "Thread# " << thread_id << " - Checked "
                              << count << "/" << (end - start) << " files";

      start_time = std::chrono::steady_clock::now();

      printed_msg = true;
    }
  }

  if (printed_msg) {
    ib::info(ER_IB_MSG_376) << "Checked " << count << " files";
  }
}

void Scanned_tablespace_dirs::print_duplicates(const Space_id_set &duplicates) {
  /* Print the duplicate names to the error log. */
  for (auto space_id : duplicates) {
    Dirs files;

    for (auto &dir : m_scanned_dirs_and_files) {
      const auto names = dir.find_by_id(space_id);

      if (names == nullptr) {
        continue;
      }
      for (auto &name : *names) {
        files.push_back(name.file_name);
      }
    }

    /* Fixes the order in the mtr tests. */
    std::sort(files.begin(), files.end());

    ut_a(files.size() > 1);

    std::ostringstream oss;

    oss << "Tablespace ID: " << space_id << " = [";

    for (size_t i = 0; i < files.size(); ++i) {
      oss << "'" << files[i] << "'";

      if (i < files.size() - 1) {
        oss << ", ";
      }
    }

    oss << "]" << std::endl;

    ib::error(ER_IB_MSG_377) << oss.str();
  }
}

void Scanned_tablespace_dirs::add_scan_dir(const std::string &in_directory) {
  std::string directory(in_directory);

  Fil_path::normalize(directory);

  add_path(directory);
}

void Scanned_tablespace_dirs::add_scan_dirs(const std::string &in_directories) {
  std::string directories(in_directories);

  Fil_path::normalize(directories);

  std::string separators;

  separators.push_back(FIL_PATH_SEPARATOR);

  add_paths(directories, separators);
}

dberr_t Scanned_tablespace_dirs::scan() {
  Scanned_files ibd_files;
  Scanned_files undo_files;
  uint16_t count = 0;
  bool print_msg = false;
  auto start_time = std::chrono::steady_clock::now();

  /* Should be trivial to parallelize the scan and ID check. */
  for (const auto &dir : m_scanned_dirs_and_files) {
    const auto real_path_dir = dir.root().abs_path();

    ut_a(Fil_path::is_separator(dir.path().back()));
    ut_a(Fil_path::is_separator(real_path_dir.back()));

    ib::info(ER_IB_MSG_SCANNING_DIR, dir.path().c_str());

    /* Walk the sub-tree of dir. */

    Dir_Walker::walk(real_path_dir, true, [&](const std::string &path) {
      /* If it is a file and the suffix matches ".ibd"
      or the undo file name format then store it for
      determining the space ID. */

      ut_a(path.length() > real_path_dir.length());
      ut_a(Fil_path::get_file_type(path) != OS_FILE_TYPE_DIR);

      /* Check if need to alter partition file names to lower case. */
      std::string new_path;

      if (m_partition_file_names_upgrader.get_partition_file(path, IBD,
                                                             new_path)) {
        /* Note all old file names to be renamed. */
        ut_ad(!new_path.empty());
        m_partition_file_names_upgrader.add_old_path(path);
      } else {
        new_path.assign(path);
      }

      /* Make the filename relative to the directory that was scanned. */
      std::string file = new_path.substr(real_path_dir.length());

      if (file.size() <= 4) {
        return;
      }

      using Value = Scanned_files::value_type;

      if (Fil_path::has_suffix(IBD, file.c_str())) {
        ibd_files.push_back(Value{count, file});

      } else if (Fil_path::is_undo_tablespace_name(file)) {
        undo_files.push_back(Value{count, file});
      }

      if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
        ib::info(ER_IB_MSG_380)
            << "Files found so far: " << ibd_files.size() << " data files"
            << " and " << undo_files.size() << " undo files";

        start_time = std::chrono::steady_clock::now();
        print_msg = true;
      }
    });

    ++count;
  }

  /* Rename all old partition files. */
  m_partition_file_names_upgrader.rename_partition_files(false);

  if (print_msg) {
    ib::info(ER_IB_MSG_381) << "Found " << ibd_files.size() << " '.ibd' and "
                            << undo_files.size() << " undo files";
  }

  Space_id_set unique;
  Space_id_set duplicates;

  /* Get the number of additional threads needed to scan the files. */
  size_t n_threads = fil_get_scan_threads(ibd_files.size());

  if (n_threads > 0) {
    ib::info(ER_IB_MSG_382)
        << "Using " << (n_threads + 1) << " threads to"
        << " scan " << ibd_files.size() << " tablespace files";
  }

  std::mutex m;

  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  using std::placeholders::_4;
  using std::placeholders::_5;
  using std::placeholders::_6;

  std::function<void(const Const_iter &, const Const_iter &, size_t,
                     std::mutex *, Space_id_set *, Space_id_set *)>
      check = std::bind(&Scanned_tablespace_dirs::duplicate_check, this, _1, _2,
                        _3, _4, _5, _6);

  par_for(PFS_NOT_INSTRUMENTED, ibd_files, n_threads, check, &m, &unique,
          &duplicates);

  duplicate_check(undo_files.begin(), undo_files.end(), n_threads, &m, &unique,
                  &duplicates);

  ut_a(m_checked == ibd_files.size() + undo_files.size());

  ib::info(ER_IB_MSG_383) << "Completed space ID check of " << m_checked.load()
                          << " files.";

  dberr_t err;

  if (!duplicates.empty()) {
    ib::error(ER_IB_MSG_384)
        << "Multiple files found for the same tablespace ID:";

    print_duplicates(duplicates);

    err = DB_FAIL;
  } else {
    err = DB_SUCCESS;
  }

  return err;
}

Scanned_tablespace_dir_and_files::Space_read_info
Scanned_tablespace_dirs::get_tablespace_info_heavy(
    const std::string &filename) const {
  bool success = false;
  pfs_os_file_t handle = os_file_create_simple_no_error_handling(
      innodb_data_file_key, filename.c_str(), OS_FILE_OPEN, OS_FILE_READ_ONLY,
      &success);
  /* We must be able to open the file */
  ut_a(success);
  const auto handle_guard =
      create_scope_guard([&handle]() { os_file_close(handle); });

  const os_offset_t file_size = os_file_get_size(handle);
  ut_a(file_size != (os_offset_t)-1);

  space_id_t space_id = UINT32_UNDEFINED;
  /* Assuming a page size, read the space_id from each page and store it in a
  map. Find out which space_id is agreed on by majority of the pages.  Choose
  that space_id. */
  size_t page_size = UNIV_ZIP_SIZE_MIN;
  for (; page_size <= UNIV_PAGE_SIZE_MAX; page_size <<= 1) {
    /* map[space_id] = count of pages */
    typedef std::map<space_id_t, ulint, std::less<space_id_t>,
                     ut::allocator<std::pair<const space_id_t, ulint>>>
        Pages;
    Pages verify;
    size_t page_count = 64;
    size_t valid_pages = 0;

    /* Adjust the number of pages to analyze based on file size */
    while ((page_count * page_size) > file_size) {
      --page_count;
    }

    ib::info(ER_IB_MSG_405)
        << "Page size:" << page_size << ". Pages to analyze:" << page_count;

    const auto page_frame =
        ut::make_unique_aligned<byte[]>(UNIV_PAGE_SIZE_MAX, UNIV_PAGE_SIZE_MAX);
    byte *page = page_frame.get();

    /* Inner loop which tries to read page with the page_size assumed above. */
    for (size_t j = 0; j < page_count; ++j) {
      dberr_t err;
      ulint n_bytes = j * page_size;
      IORequest request(IORequest::Type::READ);
      bool encrypted = false;

      err = os_file_read(request, filename.c_str(), handle, page, n_bytes,
                         page_size);

      switch (err) {
        case DB_SUCCESS:
          break;

        case DB_IO_DECRYPT_FAIL:
          /* At this stage, even if the page decryption failed, we don't have to
          report error now. Currently, only the space_id will be read from the
          page header.  Since page header is unencrypted, we will ignore the
          decryption error for now. */
          encrypted = true;
          break;

        case DB_IO_DECOMPRESS_FAIL:
          /* If the page was compressed on the fly then try and decompress the
          page */
          n_bytes = os_file_compressed_page_size(page);

          if (n_bytes != ULINT_UNDEFINED) {
            err = os_file_read(request, filename.c_str(), handle, page,
                               page_size, UNIV_PAGE_SIZE_MAX);

            if (err != DB_SUCCESS) {
              ib::info(ER_IB_MSG_406) << "READ FAIL: "
                                      << "page_no:" << j;
              continue;
            }
          }
          break;

        default:
          ib::info(ER_IB_MSG_407) << "READ FAIL: page_no:" << j;
          continue;
      }

      bool noncompressed_ok = false;

      /* For noncompressed pages, the page size must be equal to
      univ_page_size.physical(). */
      if (page_size == univ_page_size.physical()) {
        BlockReporter reporter(false, page, univ_page_size, false);

        noncompressed_ok = !reporter.is_corrupted();
      }

      bool compressed_ok = false;

      /* file-per-table tablespaces can be compressed with the same physical
      and logical page size. General tablespaces must have different physical
      and logical page sizes in order to be compressed. For this check, assume
      the page is compressed if univ_page_size.logical() <= 16k and the
      page_size we are checking <= univ_page_size.logical(). */
      if (!encrypted && univ_page_size.logical() <= UNIV_PAGE_SIZE_DEF &&
          page_size <= univ_page_size.logical()) {
        const page_size_t compr_page_size(page_size, univ_page_size.logical(),
                                          true);

        BlockReporter reporter(false, page, compr_page_size, false);

        compressed_ok = !reporter.is_corrupted();
      }

      if (noncompressed_ok || compressed_ok || encrypted) {
        space_id_t space_id = page_get_space_id(page);
        space_id_t page_id = page_get_page_no(page);

        if (space_id > 0 && page_id == j) {
          ib::info(ER_IB_MSG_408)
              << "VALID: space:" << space_id << " page_no:" << j
              << " page_size:" << page_size;

          ++valid_pages;

          ++verify[space_id];
        }
      }
    }

    /* We need at least 3 pages to confirm the space id */
    if (valid_pages < 3) {
      continue;
    }

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

          space_id = it->first;
          return {space_id, page_size};
        }
      }
    }
  }

  return {space_id, page_size};
}

Scanned_tablespace_dir_and_files::Space_read_info
Scanned_tablespace_dirs::get_tablespace_info(
    const std::string &filename) const {
  pfs_os_file_t file;
  bool success;

  /* Open the file with O_DIRECT flag for faster access */
  file = os_file_create(innodb_data_file_key, filename.c_str(), OS_FILE_OPEN,
                        OS_DATA_FILE_FOR_SPACE_ID_READ, true, &success);
  if (!success) {
    auto err = os_file_get_and_log_last_error();
    ib::warn(ER_IB_MSG_372)
        << "Unable to open '" << filename << "' error:" << err;
    return {dict_sys_t::s_invalid_space_id, std::numeric_limits<size_t>::max()};
  }

  space_id_t space_id = dict_sys_t::s_invalid_space_id;

  auto buf = ut::make_unique_aligned<byte[]>(srv_page_size, srv_page_size);

  IORequest request(IORequest::Type::READ);
  ulint bytes_read = 0;
  /* Disable the warning if we try to read compressed tablespace which has
  data less than the read size i.e., srv_page_size */
  request.disable_partial_io_warnings();

  dberr_t err =
      os_file_read_no_error_handling(request, filename.c_str(), file, buf.get(),
                                     0, srv_page_size, &bytes_read);
  os_file_close(file);

  DBUG_EXECUTE_IF("invalid_header", bytes_read = 0;);

  if (err != DB_SUCCESS || (bytes_read != srv_page_size)) {
    /* Reading from the first page failed, falling back to heavy duty method */
    return get_tablespace_info_heavy(filename);
  }

  /* Read the space_id from buf at offset FIL_PAGE_SPACE_ID */
  space_id = fsp_header_get_space_id(buf.get());

  if (space_id == 0 || space_id == SPACE_UNKNOWN) {
    /* Try the more heavy duty method */
    return get_tablespace_info_heavy(filename);
  }

  return {space_id, fsp_header_get_page_size(buf.get()).physical()};
}

void Partition_file_names_upgrader::rename_partition_files(bool revert) {
#ifndef UNIV_HOTBACKUP
  /* If revert, then we are downgrading after upgrade failure from 5.7 */
  ut_ad(!revert || srv_downgrade_partition_files);

  if (m_old_paths.empty()) {
    return;
  }

  ut_ad(!lower_case_file_system);

  for (auto &old_path : m_old_paths) {
    ut_ad(Fil_path::has_suffix(IBD, old_path));
    ut_ad(dict_name::is_partition(old_path));

    rename_partition_file(old_path, IBD, revert, false);
  }
#endif /* !UNIV_HOTBACKUP */
}

#ifndef UNIV_HOTBACKUP
void Partition_file_names_upgrader::rename_partition_file(
    const std::string &old_path, ib_file_suffix extn, bool revert,
    bool import) {
  std::string new_path;

  if (!get_partition_file(old_path, extn, new_path)) {
    ut_d(ut_error);
    ut_o(return);
  }

  ut_ad(!new_path.empty());

  bool old_exists = os_file_exists(old_path.c_str());
  bool new_exists = os_file_exists(new_path.c_str());

  static bool print_upgrade = true;
  static bool print_downgrade = true;
  bool ret = false;

  if (revert) {
    /* Check if rename is required. */
    if (!new_exists || old_exists) {
      return;
    }
    ret = os_file_rename(innodb_data_file_key, new_path.c_str(),
                         old_path.c_str());
    ut_ad(ret);

    if (ret && print_downgrade) {
      ib::info(ER_IB_MSG_DOWNGRADE_PARTITION_FILE, new_path.c_str(),
               old_path.c_str());
      print_downgrade = false;
    }
    return;
  }

  /* Check if rename is required. */
  if (new_exists || !old_exists) {
    return;
  }

  ret =
      os_file_rename(innodb_data_file_key, old_path.c_str(), new_path.c_str());

  if (!ret) {
    /* File rename failed. */
    ut_d(ut_error);
    ut_o(return);
  }

  if (import) {
    ib::info(ER_IB_MSG_UPGRADE_PARTITION_FILE_IMPORT, old_path.c_str(),
             new_path.c_str());
    return;
  }

  if (print_upgrade) {
    ib::info(ER_IB_MSG_UPGRADE_PARTITION_FILE, old_path.c_str(),
             new_path.c_str());
    print_upgrade = false;
  }
}
#endif /* !UNIV_HOTBACKUP */

bool Partition_file_names_upgrader::get_partition_file(
    const std::string &old_path [[maybe_unused]],
    ib_file_suffix extn [[maybe_unused]],
    std::string &new_path [[maybe_unused]]) {
  /* Safe check. Never needed on Windows. */
#ifdef _WIN32
  return false;
#else /* WIN32 */

#ifndef UNIV_HOTBACKUP
  /* Needed only for case sensitive file system. */
  if (lower_case_file_system) {
    return false;
  }

  /* Skip if not right file extension. */
  if (!Fil_path::has_suffix(extn, old_path)) {
    return false;
  }

  /* Check if partitioned table. */
  if (!dict_name::is_partition(old_path)) {
    return false;
  }

  std::string table_name;
  /* Get Innodb dictionary name from file path. */
  if (!Fil_path::parse_file_path(old_path, extn, table_name)) {
    ut_d(ut_error);
    ut_o(return false);
  }
  ut_ad(!table_name.empty());

  /* Rebuild partition table name with lower case. */
  std::string save_name(table_name);
  dict_name::rebuild(table_name);

  if (save_name.compare(table_name) == 0) {
    return false;
  }

  /* Build new partition file name. */
  new_path = Fil_path::make_new_path(old_path, table_name, extn);
  ut_ad(!new_path.empty());
#endif /* !UNIV_HOTBACKUP */

  return true;
#endif /* WIN32 */
}

[[nodiscard]] std::optional<std::string>
Tablespace_scanning::get_tablespace_file_by_id(const space_id_t space_id) {
  auto result = get_scanned_filename_by_space_id(space_id);
  if (result.second == nullptr) {
    return std::nullopt;
  }

  /* Duplicates should have been sorted out by now. */
  ut_a(result.second->size() == 1);
  return result.first + result.second->front().file_name;
}

#ifndef UNIV_HOTBACKUP

[[nodiscard]] std::optional<size_t>
Tablespace_scanning::get_tablespace_page_size(const space_id_t space_id) {
  auto result = get_scanned_filename_by_space_id(space_id);
  if (result.second == nullptr) {
    return std::nullopt;
  }

  /* Duplicates should have been sorted out by now. */
  ut_a(result.second->size() == 1);
  return result.second->front().physical_page_size;
}

bool Tablespace_scanning::check_missing_tablespaces() {
  bool missing = false;

  /* Called in single threaded mode, no need to acquire the mutex. */

  recv_sys->dblwr->check_missing_tablespaces();

  for (auto space_id : recv_sys->missing_ids) {
    /* space_id can't belong to recv_sys->deleted, because whenever we insert
    an id into it, we remove it from recv_sys->missing_ids, and we insert into
    recv_sys->missing_ids only if it's not in recv_sys->deleted.
    No space id should be present in both containers. */
    ut_a(recv_sys->deleted.count(space_id) == 0);

    ut_a(!get_tablespace_file_by_id(space_id));

    if (fsp_is_undo_tablespace(space_id)) {
      /* This could happen if an undo truncate is in progress because undo
      tablespace construction is not redo logged. The DD is updated at the end
      and may be out of sync. */
      continue;
    }

    ib::error(ER_IB_MSG_TABLESPACE_FILE_NOT_FOUND, ulong{space_id});
    missing = true;
  }

  return missing;
}
#endif /* !UNIV_HOTBACKUP */

} /* namespace ib::fil */
