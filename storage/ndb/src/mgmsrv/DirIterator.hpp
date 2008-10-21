/* Copyright (C) 2008 Sun Microsystems, Inc.

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

#ifndef DirIterator_HPP
#define DirIterator_HPP

#include <dirent.h>

class DirIterator {
  DIR* m_dirp;
public:
  DirIterator():
    m_dirp(NULL) {};
  ~DirIterator() {
    closedir(m_dirp);
  }

  int open(const char* path){
    if ((m_dirp= opendir(path)) == NULL){
      return -1;
    }
    return 0;
  }

  const char* next_file(void){
    struct dirent* dp;
    while ((dp= readdir(m_dirp)) != NULL &&
           dp->d_type != DT_REG)
      ;
    return dp ? dp->d_name : NULL;
  }
};

#endif
