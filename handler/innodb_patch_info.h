/* Copyright (C) 2002-2006 MySQL AB
  
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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                      /* gcc class implementation */
#endif

struct innodb_patch {
       const char *file;
       const char *name;
       const char *version;
       const char *author;
       const char *license;
       const char *comment;
}innodb_patches[] = {
{"innodb_show_patches.patch","I_S.INNODB_PATCHES","1.0","Percona","GPLv2",""},
{"innodb_show_status.patch","Fixes to SHOW INNODB STATUS","1.0","Percona","GPLv2","Memory information and lock info fixes"},
{"innodb_io_patches.patch","Patches to InnoDB IO","1.0","Percona","GPLv2",""},
{"innodb_rw_lock.patch","InnoDB RW-lock fixes","1.0","Percona","GPLv2","Useful for 8+ cores SMP systems"},
{NULL, NULL, NULL, NULL, NULL, NULL}
};
