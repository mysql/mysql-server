/*
   Copyright 2009 Sun Microsystems, Inc.

   All rights reserved. Use is subject to license terms.

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


#include <NDBT_Find.hpp>

void
NDBT_find_binary(BaseString& name, const char* binary_name,
                 const char* first_path, ...)
{

  Vector<BaseString> paths;

  // Push all the different paths to a list
  const char* str = first_path;
  va_list args;
  va_start(args, first_path);
  do
  {
    BaseString path;
    path.assfmt("%s", str);
    paths.push_back(path);
  } while ((str = va_arg(args, const char*)) != NULL);
  va_end(args);

  // Loop the list of paths and see if the binary exists
  for (unsigned i = 0; i < paths.size(); i++)
  {
    BaseString path;
    path.assfmt("%s/%s", paths[i].c_str(), binary_name);
    if (access(path.c_str(), F_OK) == 0)
    {
      // Sucess, found the binary. Convert path to absolute and return it
      char realpath_buf[PATH_MAX];
#ifndef _WIN32
      if (realpath(path.c_str(), realpath_buf) == NULL)
      {
        fprintf(stderr, "Could not convert '%s' to realpath\n", path.c_str());
        abort();
      }
#else
      int ret= GetFullPathName(path.c_str(), sizeof(realpath_buf),
                               realpath_buf, NULL);
      if (ret == 0 || ret >= sizeof(realpath_buf))
      {
        fprintf(stderr, "Could not convert '%s' with GetFullPathName\n",
                path.c_str());
        abort();
      }
#endif

      name.assign(realpath_buf);
      return;
    }
  }

  // Failed to find the binary in any of the supplied paths
  BaseString searched;
  searched.append(paths, ", ");
  fprintf(stderr, "Could not find '%s' in '%s'\n",
          binary_name, searched.c_str());
  abort();
}

void
NDBT_find_ndb_mgmd(BaseString& path)
{
  NDBT_find_binary(path, "ndb_mgmd",
                   "../../src/mgmsrv",
                   "../storage/ndb/src/mgmsrv",
                   "../libexec",
                   "../sbin",
                   "../bin",
                   NULL);
}

