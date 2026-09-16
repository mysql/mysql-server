/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
*/

#pragma once

#include "api0api.h"
#include "db0err.h"
#include "trx0purge.h"

namespace ib::fil {

using Space_id_set = std::set<space_id_t>;
using Dirs = std::vector<std::string>;

/* uint16_t is the index into Scanned_tablespace_dirs::m_scanned_dirs_and_files
 */
using Scanned_files = std::vector<std::pair<uint16_t, std::string>>;

/** A directory and tablespace files discovered under it during startup. */
class Scanned_tablespace_dir_and_files {
 public:
  /* Info populated by opening and reading first few pages from a tablespace
  file. */
  struct Space_read_info {
    /* Space id */
    space_id_t space_id{};

    /* Physical page size of tablespace */
    size_t physical_page_size{};
  };

  /** Structure to represent space information collected after directory
  scanning. */
  struct Space_scan_info {
    /* Tablespce file name */
    std::string file_name{};

    /* physical page size of tablespace */
    size_t physical_page_size{};
  };

  using Space_scan_infos = std::vector<Space_scan_info>;
  using Space_id_to_scan_info =
      std::unordered_map<space_id_t, Space_scan_infos>;

  /** Default constructor
  @param[in]    dir             Directory that the files are under */
  explicit Scanned_tablespace_dir_and_files(const std::string &dir)
      : m_id_to_scan_info(), m_dir(dir) {
    ut_ad(Fil_path::is_separator(dir.back()));
  }

  /** Add a space to filename mapping.
  @param[in]    space_info      Tablespace Info
  @param[in]    name            File name.
  @return number of files that map to the space ID */
  [[nodiscard]] size_t add(Space_read_info space_info, const std::string &name);

  /** Get the file names that map to a space ID
  @param[in]    space_id        Tablespace ID
  @return the filenames that map to space id */
  [[nodiscard]] Space_scan_infos *find_by_id(space_id_t space_id) {
    ut_ad(space_id != TRX_SYS_SPACE);

    auto it = m_id_to_scan_info.find(space_id);
    if (it != m_id_to_scan_info.end()) {
      return &it->second;
    }

    return nullptr;
  }

  /** Remove the entry for the space ID.
  @param[in]    space_id        Tablespace ID mapping to remove
  @return true if erase successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    ut_ad(space_id != TRX_SYS_SPACE);

    const auto n_erased = m_id_to_scan_info.erase(space_id);
    return (n_erased == 1);
  }

  /** Clear all the tablespace data. */
  void clear() { m_id_to_scan_info.clear(); }

  /** @return m_dir */
  [[nodiscard]] const Fil_path &root() const { return m_dir; }

  /** @return the directory path specified by the user. */
  [[nodiscard]] const std::string &path() const { return m_dir.path(); }

  void get_space_ids(std::vector<space_id_t> &space_ids) {
    for (auto &item : m_id_to_scan_info) {
      space_ids.push_back(item.first);
    }
  }

#ifdef UNIV_DEBUG
  void print_mapping() {
    for (auto &info : m_id_to_scan_info) {
      const auto space_id = info.first;
      for (const auto &scan_info : info.second) {
        ib::info() << "MAPPING : [" << space_id << "] = Name : '"
                   << scan_info.file_name
                   << "'. Page Size : " << scan_info.physical_page_size;
      }
    }
  }
#endif

 private:
  /** Mapping from tablespace ID to data filenames.
  Note: The file names in m_id_to_scan_info are relative to m_dir. */
  Space_id_to_scan_info m_id_to_scan_info;

  /** Top level directory where the above files were found. */
  Fil_path m_dir;
};

/** A helper class to handle partition file names during server upgrade. */
class Partition_file_names_upgrader {
 public:
  ~Partition_file_names_upgrader() {
#ifndef UNIV_HOTBACKUP
    /* Revert to old names if downgrading after upgrade failure. */
    if (srv_downgrade_partition_files) {
      rename_partition_files(true);
    }
#endif /* !UNIV_HOTBACKUP */

    /* Clear all accumulated old files. */
    m_old_paths.clear();
  }

  /** Add a path to the list of old paths for partitions.
  @param[in]  path  path to be added. */
  void add_old_path(const std::string &path) { m_old_paths.push_back(path); }

  /** Rename partition files during upgrade.
  @param[in]  revert     if true, revert to old names */
  void rename_partition_files(bool revert);

  /** Get modified name for partition file. During upgrade we change all
  partition files to have lower case separator and partition name.
  @param[in]      old_path        old file name and path
  @param[in]      extn            file extension suffix
  @param[out]     new_path        modified new name for partitioned file
  @return true, iff name needs modification. */
  [[nodiscard]] static bool get_partition_file(const std::string &old_path,
                                               ib_file_suffix extn,
                                               std::string &new_path);

  /** Rename partition file.
  @param[in]      old_path        old file path
  @param[in]      extn            file extension suffix
  @param[in]      revert          if true, rename from new to old file
  @param[in]      import          if called during import */
  static void rename_partition_file(const std::string &old_path,
                                    ib_file_suffix extn, bool revert,
                                    bool import);

 private:
  /** Old file paths during 5.7 upgrade. It will be populated by
  Scanned_tablespace_dirs::scan() */
  std::vector<std::string> m_old_paths;
};

/** Directories scanned during startup and the files discovered. */
class Scanned_tablespace_dirs {
 public:
  using Result =
      std::pair<std::string,
                Scanned_tablespace_dir_and_files::Space_scan_infos *>;

  /** Constructor */
  Scanned_tablespace_dirs() : m_scanned_dirs_and_files(), m_checked() {}

  /** Normalize and save a directory to scan for IBD and IBU datafiles
  before recovery.
  @param[in]  directory    directory to scan for IBD and IBU files */
  void add_scan_dir(const std::string &directory);

  /** Normalize and save a list of directories to scan for IBD and IBU
  datafiles before recovery.
  @param[in]  directories  Directories to scan for IBD and IBU files */
  void add_scan_dirs(const std::string &directories);

  /** Discover tablespaces by reading the header from IBD files.
  @return DB_SUCCESS if all goes well */
  [[nodiscard]] dberr_t scan();

  /** Clear all the tablespace file data but leave the list of
  scanned directories in place. */
  void clear() {
    for (auto &dir : m_scanned_dirs_and_files) {
      dir.clear();
    }

    m_checked = 0;
  }

  /** Erase a space ID to filename mapping.
  @param[in]    space_id        Tablespace ID to erase
  @return true if successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    for (auto &dir : m_scanned_dirs_and_files) {
      if (dir.erase_path(space_id)) {
        return true;
      }
    }

    return false;
  }

  /* Find the first matching space ID -> name mapping.
  @param[in]    space_id        Tablespace ID
  @return directory searched and pointer to names that map to the
          tablespace ID */
  [[nodiscard]] Result find_by_id(space_id_t space_id) {
    for (auto &dir : m_scanned_dirs_and_files) {
      const auto names = dir.find_by_id(space_id);

      if (names != nullptr) {
        return (Result{dir.path(), names});
      }
    }

    return (Result{"", nullptr});
  }

  /** Determine if this Fil_path contains the path provided.
  @param[in]  path  file or directory path to compare.
  @return true if this Fil_path contains path */
  [[nodiscard]] bool contains(const std::string &path) const {
    const Fil_path descendant{path};

    for (const auto &dir : m_scanned_dirs_and_files) {
      if (dir.root().is_same_as(descendant) ||
          dir.root().is_ancestor(descendant)) {
        return true;
      }
    }
    return false;
  }

  /** Get the list of directories that InnoDB knows about.
  @return the list of directories 'dir1;dir2;....;dirN' */
  [[nodiscard]] std::string get_dirs() const {
    std::string dirs;

    ut_ad(!m_scanned_dirs_and_files.empty());

    for (const auto &dir : m_scanned_dirs_and_files) {
      dirs.append(dir.root());
      dirs.push_back(FIL_PATH_SEPARATOR);
    }

    dirs.pop_back();

    ut_ad(!dirs.empty());

    return dirs;
  }

  /** Get the list of tablespace IDs discovered.
  @param[out] space_ids List of tablespace ids */
  void get_found_space_ids(std::vector<space_id_t> &space_ids) {
    for (auto &dir : m_scanned_dirs_and_files) {
      dir.get_space_ids(space_ids);
    }
  }

#ifdef UNIV_DEBUG
  void print_mapping() {
    for (auto &dir : m_scanned_dirs_and_files) {
      dir.print_mapping();
    }
  }
#endif

 private:
  /** Print the duplicate filenames for a tablespace ID to the log
  @param[in]    duplicates      Duplicate tablespace IDs*/
  void print_duplicates(const Space_id_set &duplicates);

  /** first=dir path from the user, second=files found under first. */
  using Scanned_dirs_and_files = std::vector<Scanned_tablespace_dir_and_files>;

  /** Report a warning that a path is being ignored and include the reason. */
  void warn_ignore(std::string path_in, const char *reason);

  /** Add a single path specification to this list of tablespace directories.
  Convert it to an absolute path. Check if the path is valid.  Ignore
  unreadable, duplicate or invalid directories.
  @param[in]  str  Path specification to tokenize */
  void add_path(const std::string &str);

  /** Add a delimited list of path specifications to this list of tablespace
  directories. Convert relative paths to absolute paths. Check if the paths
  are valid.  Ignore unreadable, duplicate or invalid directories.
  @param[in]    str             Path specification to tokenize
  @param[in]    delimiters      Delimiters */
  void add_paths(const std::string &str, const std::string &delimiters);

  using Const_iter = Scanned_files::const_iterator;

  /** Check for duplicate tablespace IDs.
  @param[in]      start       Start of slice
  @param[in]      end         End of slice
  @param[in]      thread_id   Thread ID
  @param[in,out]  mutex       Mutex protecting the global state
  @param[in,out]  unique      To check for duplicates
  @param[in,out]  duplicates  Duplicate space IDs found */
  void duplicate_check(const Const_iter &start, const Const_iter &end,
                       size_t thread_id, std::mutex *mutex,
                       Space_id_set *unique, Space_id_set *duplicates);

  /** Get the tablespace ID from an .ibd and/or an undo tablespace. If the
  read failed or the ID is 0 on the first page or there is a mismatch of
  space_ids stored in FSP_SPACE_ID and FIL_PAGE_SPACE_ID, then try finding
  the ID with get_tablespace_info_heavy(). This function should only be called
  during server startup.
  @param[in]    filename        File name to check
  @return s_invalid_space_id if not found, otherwise the space ID */
  [[nodiscard]] Scanned_tablespace_dir_and_files::Space_read_info
  get_tablespace_info(const std::string &filename) const;

 private:
  /** If normal file read isn't sufficient to get the space id and page size,
  use this heavy method which opens files by guessing different page sizes and
  read pages from file to determine space_id. */
  [[nodiscard]] Scanned_tablespace_dir_and_files::Space_read_info
  get_tablespace_info_heavy(const std::string &filename) const;

  /** Directories scanned and the files discovered under them. */
  Scanned_dirs_and_files m_scanned_dirs_and_files;

  /** Number of files checked. */
  std::atomic_size_t m_checked;

  /** An object to handle partition file names during upgrade. */
  Partition_file_names_upgrader m_partition_file_names_upgrader;
};

/** Represents infrastructure needed for tablespace files scanning during
server bootstrap. */
class Tablespace_scanning {
 public:
  /** Check if a path is known to InnoDB.
  @param[in]    path            Path to check
  @return true if path is known to InnoDB */
  [[nodiscard]] bool is_known_path(const std::string &path) {
    return m_scanned_dirs.contains(path);
  }

  /** Erase a space ID to filename mapping.
  @param[in]    space_id        Tablespace ID to erase
  @return true if successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    ut_a(!is_cleared());
    return m_scanned_dirs.erase_path(space_id);
  }

  /** Free the data structures required for recovery. */
  void clear() {
    ut_a_eq(m_state, Scanning_state::INITED);

    m_scanned_dirs.clear();
    m_state = Scanning_state::CLEARED;
  }

  /** Normalize and save a directory to scan for IBD and IBU datafiles
  before recovery.
  @param[in]  directory    directory to scan for IBD and IBU files */
  void add_scan_dir(const std::string &directory) {
    ut_a_eq(m_state, Scanning_state::UNINITED);

    m_scanned_dirs.add_scan_dir(directory);
  }

  /** Normalize and save a list of directories to scan for IBD and IBU
  datafiles before recovery.
  @param[in]  directories  Directories to scan for IBD and IBU files */
  void add_scan_dirs(const std::string &directories) {
    m_scanned_dirs.add_scan_dirs(directories);
  }

  /** Discover tablespaces by reading the header from IBD files.
  @return DB_SUCCESS if all goes well */
  [[nodiscard]] dberr_t scan() {
    ut_a(m_state == Scanning_state::UNINITED);

    dberr_t err = m_scanned_dirs.scan();
    if (err != DB_SUCCESS) {
      return err;
    }

    m_state = Scanning_state::INITED;
    return err;
  }

  /** Get the list of directories that InnoDB knows about.
  @return the list of directories 'dir1;dir2;....;dirN' */
  [[nodiscard]] std::string get_dirs() { return m_scanned_dirs.get_dirs(); }

  /** Check if file for a given space_id is found during scan.
  @param[in]  space_id  Tablespace id
  @return true if file is found, false otherwise. */
  [[nodiscard]] bool is_tablespace_file_found(const space_id_t space_id) {
    auto result = get_scanned_filename_by_space_id(space_id);
    return (result.second != nullptr);
  }

  /** Fetch the file name opened for a space_id from the file map.
  @param[in]   space_id  tablespace ID
  @return Tablespace file name if Tablespace with the id is found, nullopt
  otherwise */
  [[nodiscard]] std::optional<std::string> get_tablespace_file_by_id(
      space_id_t space_id);

#ifndef UNIV_HOTBACKUP
  /** Get the physical page size of a Tablespace given it's space id.
  @param[in]   space_id   Tablespace id
  @return Tablespace page size if Tablespace with the given id is found, nullopt
  otherwise */
  [[nodiscard]] std::optional<size_t> get_tablespace_page_size(
      space_id_t space_id);

  /** This function should be called after recovery has completed.
  Check for tablespace files for which we did not see any MLOG_FILE_DELETE
  or MLOG_FILE_RENAME record. These could not be recovered
  @return true if there were some filenames missing for which we had to
  ignore redo log records during the apply phase */
  [[nodiscard]] bool check_missing_tablespaces();
#endif /* !UNIV_HOTBACKUP */

  /** Returns true if mapping has been initialized. */
  [[nodiscard]] bool is_inited() { return (m_state == Scanning_state::INITED); }

  /** Returns true if mapping has been cleared. */
  [[nodiscard]] bool is_cleared() {
    return (m_state == Scanning_state::CLEARED);
  }

  /** Returns the list of tablespace ids found during scan
  @param[out]  space_ids  List of tablespace ids discovered. */
  void get_found_space_ids(std::vector<space_id_t> &space_ids) {
    ut_a_eq(m_state, Scanning_state::INITED);
    m_scanned_dirs.get_found_space_ids(space_ids);
  }

#ifdef UNIV_DEBUG
  void print_mapping() { m_scanned_dirs.print_mapping(); }
#endif

 private:
  /** Fetch the file names opened for a space_id during recovery.
  @param[in]  space_id  Tablespace ID to lookup
  @return pair of top level directory scanned and names that map
          to space_id or nullptr if not found. */
  [[nodiscard]] Scanned_tablespace_dirs::Result
  get_scanned_filename_by_space_id(space_id_t space_id) {
    ut_a_eq(m_state, Scanning_state::INITED);
    return m_scanned_dirs.find_by_id(space_id);
  }

  /** Tablespace directories scanned at startup */
  Scanned_tablespace_dirs m_scanned_dirs;

  enum class Scanning_state { UNINITED, INITED, CLEARED };
  friend std::ostream &operator<<(std::ostream &stream,
                                  const Scanning_state &state) {
    return stream << static_cast<int>(state);
  }
  Scanning_state m_state{Scanning_state::UNINITED};
};
} /* namespace ib::fil */

extern ut::unique_ptr<ib::fil::Tablespace_scanning> tablespace_scanning;
extern bool lower_case_file_system;
