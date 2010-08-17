/* Copyright (C) 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbDir_HPP
#define NdbDir_HPP

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
    const char* next_file(void);
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

  /*
    Create directory
  */
  static bool create(const char *path);

  /*
    Remove directory recursively
      dir - path to directory that should be removed
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
