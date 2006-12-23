/* Copyright (C) 2003 MySQL AB

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


#include "NDBT_Output.hpp"

FileOutputStream gerr_fileoutputstream(stderr);
FileOutputStream gwarning_fileoutputstream(stderr);
FileOutputStream ginfo_fileoutputstream(stdout);
FileOutputStream gdebug_fileoutputstream(stdout);

FilteredNdbOut g_err(gerr_fileoutputstream,         0, 2);
FilteredNdbOut g_warning(gwarning_fileoutputstream, 1, 2);
FilteredNdbOut g_info(ginfo_fileoutputstream,       2, 2);
FilteredNdbOut g_debug(gdebug_fileoutputstream,     3, 2);

void
setOutputLevel(int i){
  g_err.setLevel(i);
  g_warning.setLevel(i);
  g_info.setLevel(i);
  g_debug.setLevel(i);
}
