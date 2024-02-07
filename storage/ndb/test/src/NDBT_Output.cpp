/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "NDBT_Output.hpp"

FileOutputStream gerr_fileoutputstream(stderr);
FileOutputStream gwarning_fileoutputstream(stderr);
FileOutputStream ginfo_fileoutputstream(stdout);
FileOutputStream gdebug_fileoutputstream(stdout);

FilteredNdbOut g_err(gerr_fileoutputstream, 0, 2);
FilteredNdbOut g_warning(gwarning_fileoutputstream, 1, 2);
FilteredNdbOut g_info(ginfo_fileoutputstream, 2, 2);
FilteredNdbOut g_debug(gdebug_fileoutputstream, 3, 2);

void setOutputLevel(int i) {
  g_err.setLevel(i);
  g_warning.setLevel(i);
  g_info.setLevel(i);
  g_debug.setLevel(i);
}
