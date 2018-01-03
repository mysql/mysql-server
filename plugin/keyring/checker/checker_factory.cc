/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/keyring/checker/checker_factory.h"

#include <stddef.h>

#include "plugin/keyring/checker/checker_ver_1_0.h"
#include "plugin/keyring/checker/checker_ver_2_0.h"

namespace keyring {

Checker* CheckerFactory::getCheckerForVersion(std::string version)
{
  if(version == keyring_file_version_1_0)
    return new CheckerVer_1_0();
  else if (version == keyring_file_version_2_0)
    return new CheckerVer_2_0();
  return NULL;
}

}//namespace keyring
