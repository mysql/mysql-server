/*****************************************************************************

Copyright (c) 1995, 2019, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file include/fil0purge.h
 The file purge interface

 Created 1/11/2019 Jianwei.zhao
 *******************************************************/
#ifndef fil0purge_h
#define fil0purge_h

#include <atomic>

#include "univ.i"
#include "ut0mutex.h"

/** File node structure for background purge thread. */
struct file_purge_node_t {
  using List_node = UT_LIST_NODE_T(file_purge_node_t);

  /** File name */
  char *m_file_path;

  /** Unique id within log DDL table */
  ulint m_log_ddl_id;

  /** Link to file_purge_system->list */
  List_node LIST;
};

/* File purge system */
class File_purge {
  using File_list = UT_LIST_BASE_NODE_T(file_purge_node_t);

  /* */
  static constexpr const char *PREFIX = "#FP_";

 public:

  /** Constructor */
  explicit File_purge(ulint thread_id, time_t start_time);

  virtual ~File_purge();

  ulint get_thread_id() { return m_thread_id; }
  /**
    Add file into purge list

    @param[in]    id        Log DDL record id
    @param[int]   path      The temporary file name

    @retval       false     Success
    @retval       true      Failure
  */
  bool add_file(ulint id, const char *path);

  /**
    Purge the first file node by purge max size.

    @param[in]    size      Purge max size (bytes)
    @param[in]    force     Whether unlink file directly

    @retval       -1        Error
    @retval       0         Success & no file purge
    @retval       >0        How many purged operation
  */
  int purge_file(ulint size, bool force);
  /**
    The data file list length

    @retval       >=0
  */
  ulint length();
  /**
    Purge all the data file cached in list.

    @param[in]    size      Purge max size (bytes)
    @param[in]    force     Purge little by little
                            or unlink directly.
  */
  void purge_all(ulint size, bool force);

  /**
    Remove the file node from list

    @param[in]    node      File purge node pointer
  */
  void remove_file(file_purge_node_t *node);

  /**
    Generate a unique temporary file name.

    @param[in]    filepath      Original file name

    @retval       file name     Generated file name
  */
  char *generate_file(const char *filepath);

  /* Set work directory */
  void set_dir(const char *dir) { m_dir = dir; }

  /* Get work directory */
  const char *get_dir() { return m_dir; }

  /* Get next unique id number */
  ulint next_id();

  /** Disable */
  File_purge(File_purge &) = delete;
  File_purge(const File_purge &) = delete;
  File_purge &operator=(const File_purge &) = delete;

 private:
  /** Protect file node list */
  ib_mutex_t m_mutex;
  /** File node list */
  File_list m_list;
  /** File purge thread id */
  ulint m_thread_id;
  /** Server start time */
  time_t m_start_time;
  /* Unique id within instance */
  std::atomic<ulint> m_id;
  /** The directory will hold renamed file */
  const char *m_dir;
};

extern File_purge *file_purge_sys;

/**
  Drop a single-table tablespace and rename the data file as temporary file.
  This deletes the fil_space_t if found and rename the file on disk.

  @param[in]      space_id      Tablespace id
  @param[in]      filepath      File path of tablespace to delete

  @retval         error code */
extern dberr_t row_purge_single_table_tablespace(space_id_t space_id,
                                                 const char *filepath);

/**
  Rename the ibd data file and delete the releted files

  @param[in]      old_filepath  The original data file
  @param[in]      new_filepath  The new data file

  @retval         False         Success
  @retval         True          Failure
*/
extern bool fil_purge_file(const char *old_filepath, const char *new_filepath);

/**
  Drop or purge single table tablespace

  @param[in]    space_id      tablespace id
  @param[in]    filepath      tablespace data file path


  @retval     DB_SUCCESS or error
*/
extern dberr_t row_drop_or_purge_single_table_tablespace(space_id_t space_id,
                                                         const char *filepath);
#endif
