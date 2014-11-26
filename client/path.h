/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef PATH_UTIL_INCLUDED
#define PATH_UTIL_INCLUDED
#include <dirent.h>
#include <pwd.h>
#include "my_dir.h"
#include <string>
#include <ostream>

#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_C '/'
#define MAX_PATH_LENGTH 512

/**
  A helper class for handling file paths. The class can handle the memory
  on its own or it can wrap an external string.
  @note This is a rather trivial wrapper which doesn't handle malformed paths
  or filenames very well.
  @see unittest/gunit/path-t.cc

*/
class Path
{
public:
  Path();

  Path(const std::string &s);

  Path(const Path &p);

  bool getcwd();

  bool validate_filename();

  void trim();

  void parent_directory(Path *out);

  Path &up();

  Path &append(const std::string &path);

  Path &filename_append(const std::string &ext);

  void path(const std::string &p);

  void filename(const std::string &f);

  void path(const Path &p);

  void filename(const Path &p);

  bool qpath(const std::string &qp);

  bool is_qualified_path();

  bool exists();

  const std::string to_str();

  bool empty();
  
  void get_homedir();

  friend std::ostream &operator<<(std::ostream &op, const Path &p);
private:
  std::string m_path;
  std::string m_filename;
};

std::ostream &operator<<(std::ostream &op, const Path &p);

#endif
