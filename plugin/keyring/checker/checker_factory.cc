/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "checker_factory.h"

#include <stddef.h>

#include "checker_ver_1_0.h"
#include "checker_ver_2_0.h"

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
