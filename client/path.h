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
extern "C"
{
#include <dirent.h>
#include <my_dir.h>
}
#include <string>

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
  Path(std::string *path) : m_ptr(path), m_fptr(&m_filename)
  {
    trim();
    m_path.clear();
  }

  Path(std::string *path, std::string *filename) : m_ptr(path),
    m_fptr(filename)
  {
    trim();
    m_path.clear();
  }

  Path(void) { m_ptr= &m_path; m_fptr= &m_filename; trim(); }

  Path(const std::string &s) { path(s); m_fptr= &m_filename; }

  Path(const Path &p) { m_path= p.m_path; m_filename= p.m_filename; m_ptr= &m_path; m_fptr= &m_filename; }

  bool getcwd(void)
  {
    char path[MAX_PATH_LENGTH];
    if (::getcwd(path, MAX_PATH_LENGTH) == 0)
       return false;
    m_ptr->clear();
    m_ptr->append(path);
    trim();
    return true;
  }

  bool validate_filename()
  {
    size_t idx= m_fptr->find(PATH_SEPARATOR);
    if (idx != std::string::npos)
      return false;
    return true;
  }

  void trim()
  {
    if (m_ptr->length() <= 1)
      return;
    std::string::iterator it= m_ptr->end();
    --it;

    while((*it) == PATH_SEPARATOR_C)
    {
      m_ptr->erase(it--);
    }
  }

  void parent_directory(Path *out)
  {
    size_t idx= m_ptr->rfind(PATH_SEPARATOR);
    if (idx == std::string::npos)
    {
      out->path("");
    }
    out->path(m_ptr->substr(0, idx));
  }

  Path &up()
  {
    size_t idx= m_ptr->rfind(PATH_SEPARATOR);
    if (idx == std::string::npos)
    {
      m_ptr->clear();
    }
    m_ptr->assign(m_ptr->substr(0, idx));
    return *this;
  }

  Path &append(const std::string &path)
  {
    if (m_ptr->length() > 1 && path[0] != PATH_SEPARATOR_C)
      m_ptr->append(PATH_SEPARATOR);
    m_ptr->append(path);
    trim();
    return *this;
  }

  void path(std::string *p)
  {
    m_ptr= p;
    trim();
  }

  void path(const std::string &p)
  {
    m_path.clear();
    m_path.append(p);
    m_ptr= &m_path;
    trim();
  }

  void filename(const std::string &f)
  {
    m_filename= f;
    m_fptr= &m_filename;
  }
  void filename(std::string *f) { m_fptr= f; m_filename.clear(); }
  void path(const Path &p) { path(p.m_path); }
  void filename(const Path &p) { path(p.m_filename); }

  void qpath(const std::string &qp)
  {
    size_t idx= qp.rfind(PATH_SEPARATOR);
    if (idx == std::string::npos)
    {
      m_filename= qp;
      m_path.clear();
      m_ptr= &m_path;
      m_fptr= &m_filename;
      return;
    }
    filename(qp.substr(idx + 1, qp.size() - idx));
    path(qp.substr(0, idx));
  }

  bool is_qualified_path()
  {
    return m_fptr->length() > 0;
  }

  bool exists()
  {
    if (!is_qualified_path())
    {
      DIR *dir= opendir(m_ptr->c_str());
      if (dir == 0)
        return false;
      return true;
    }
    else
    {
      MY_STAT s;
      std::string qpath(*m_ptr);
      qpath.append(PATH_SEPARATOR).append(*m_fptr);
      if (my_stat(qpath.c_str(), &s, MYF(0)) == NULL)
        return false;
      return true;
    }
  }

  std::string to_str()
  {
    std::string qpath(*m_ptr);
    if (m_fptr->length() != 0)
    {
      qpath.append(PATH_SEPARATOR);
      qpath.append(*m_fptr);
    }
    return qpath;
  }

  friend std::ostream &operator<<(std::ostream &op, const Path &p);
private:
  std::string m_path;
  std::string *m_ptr;
  std::string *m_fptr;
  std::string m_filename;
};

std::ostream &operator<<(std::ostream &op, const Path &p)
{
  return op << *(p.m_ptr);
}
