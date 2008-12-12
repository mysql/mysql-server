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

struct innodb_enhancement {
       const char *file;
       const char *name;
       const char *comment;
       const char *link;
}innodb_enhancements[] = {
{"innodb_show_enhancements","I_S.PERCONA_INNODB_ENHANCEMENTS","","http://www.percona.com/docs/wiki/percona-innodb-plugin:innodb_show_enhancements"},
{"innodb_show_status","Improvements to SHOW INNODB STATUS","Memory information and lock info fixes","http://www.percona.com/docs/wiki/percona-innodb-plugin:innodb_show_status"},
{"innodb_io","Improvements to InnoDB IO","","http://www.percona.com/docs/wiki/percona-innodb-plugin:innodb_io"},
{"innodb_rw_lock","InnoDB RW-lock fixes","Useful for 8+ cores SMP systems","http://www.percona.com/docs/wiki/percona-innodb-plugin:innodb_rw_lock"},
{"optimistic_margine_count","Fix of buffer_pool mutex","Decreases contention on buffer_pool mutex","http://www.percona.com/docs/wiki/percona-innodb-plugin:optimistic_margine_count"},
{NULL, NULL, NULL, NULL}
};
