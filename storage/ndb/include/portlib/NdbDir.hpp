/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NdbDir_HPP
#define NdbDir_HPP

#ifdef _WIN32
typedef int mode_t; // Only dummy usage on Windows
#else
#include <sys/types.h>  // mode_t
#endif

class NdbDir {
public:
  class Iterator {
    class DirIteratorImpl& m_impl;
    Iterator(const Iterator&);  // not impl
    Iterator& operator=(const Iterator&); // not impl
  public:
    Iterator();
    ~Iterator();

    int open(const char* path);
    void close(void);

    /*
      Return the next regular file or NULL if no more file found
    */
    const char* next_file(void);

    /*
      Return the next entry(file, dir, symlink etc.) or NULL if no
      more entries found
    */
    const char* next_entry(void);
  };

  class Temp {
    const char* m_path;
    Temp(const Temp&);  // not impl
    Temp& operator=(const Temp&); // not impl
  public:
    Temp();
    ~Temp();
    const char* path(void) const;
  };

  static mode_t u_r(void);
  static mode_t u_w(void);
  static mode_t u_x(void);
  static mode_t u_rwx(void) { return (u_r() | u_w() | u_x()); }

  static mode_t g_r(void);
  static mode_t g_w(void);
  static mode_t g_x(void);
  static mode_t g_rwx(void) { return (g_r() | g_w() | g_x()); }

  static mode_t o_r(void);
  static mode_t o_w(void);
  static mode_t o_x(void);
  static mode_t o_rwx(void) { return (o_r() | o_w() | o_x()); }

  /*
    Create directory
     path - path to directory to create
     mode - mode for the directory to create
     ignore_existing - don't print or return error if directory
                       already exist
  */
  static bool create(const char *path,
                     mode_t mode = u_rwx(),
                     bool ignore_existing = false);

  /*
    Remove directory recursively
      path - path to directory that should be removed
      only_contents - only remove the contents of the directory

  */
  static bool remove_recursive(const char* path, bool only_contents = false);

  /*
    Remove empty directory
  */
  static bool remove(const char* path);

  /*
    Change working directory
  */
  static int chdir(const char* path);

};

#endif
