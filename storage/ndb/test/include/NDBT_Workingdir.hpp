/*
   Copyright (c) 2009, 2022, Oracle and/or its affiliates.

   Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef NDBT_WORKINGDIR_HPP
#define NDBT_WORKINGDIR_HPP

#include "util/require.h"
#include <NdbDir.hpp>
#include <BaseString.hpp>

class NDBT_Workingdir
{
  NdbDir::Temp m_temp;
  BaseString m_wd;
public:

  NDBT_Workingdir(const char* dirname)
  {
    const char* tmp_path = m_temp.path();
    char* ndbt_tmp = getenv("NDBT_TMP_DIR");
    if (ndbt_tmp)
      tmp_path = ndbt_tmp;
    require(tmp_path);

    m_wd.assfmt("%s%s%s%d", tmp_path, DIR_SEPARATOR, dirname,
                (int)NdbProcess::getpid());
    if (access(m_wd.c_str(), F_OK) == 0)
      NdbDir::remove_recursive(m_wd.c_str());
    if (!NdbDir::create(m_wd.c_str()))
      abort();
  }

  ~NDBT_Workingdir()
  {
    if (access(m_wd.c_str(), F_OK) == 0)
      NdbDir::remove_recursive(m_wd.c_str());
  }

  const char* path(void) const
  {
    return m_wd.c_str();
  }

};

#endif
