/*
   Copyright (c) 2011, 2012, Monty Program Ab

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

/*
  Extract properties of a windows service binary path
*/
#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h> 
typedef struct mysqld_service_properties_st
{
  char mysqld_exe[MAX_PATH];
  char inifile[MAX_PATH];
  char datadir[MAX_PATH];
  int  version_major;
  int  version_minor;
  int  version_patch;
} mysqld_service_properties;

extern int get_mysql_service_properties(const wchar_t *bin_path, 
  mysqld_service_properties *props);

#ifdef __cplusplus
}
#endif
