/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <NdbMain.h>
#include <NdbEnv.h>

void rmdir_recurs(const char* path){
  DIR* dirp;
  struct dirent * dp;
  char buf[255];

  dirp = opendir(path);
  while ((dp = readdir(dirp)) != NULL){
    if ((strcmp(".", dp->d_name) != 0) && (strcmp("..", dp->d_name) != 0)) {
      sprintf(buf, "%s/%s", path, dp->d_name);
      //      printf(" %s\n", buf);
      if (remove(buf) == 0){
        printf("."); //printf("Removed: %s\n", buf); // The file was removed
      } else { 
        // The file was not removed, try to remove it as a directory
        if(rmdir(buf) == 0){
          ; //printf("Removed dir: %s\n", buf); // The dir was removed
        } else {
          // The directory was not removed, call this function again recursively
          ; //printf("Call rm_dir: %s\n", buf);
          rmdir_recurs(buf);
          rmdir(buf);
        }      
      }
    }
  }
  closedir(dirp);
}

NDB_COMMAND(init_rm, "init_rm", "init_rm [path to dir]", "Removes all files and dirs below [path to dir], default = /d/ndb/fs. WARNING can remove a lot of useful files!", 4096){

  if(argc == 2){
    printf("Removing all files and dirs in %s\n", argv[1]);
    rmdir_recurs(argv[1]);
  } else if(argc == 1){
    printf("Removing all files and dirs in /d/ndb/fs\n");
    rmdir_recurs("/d/ndb/fs");
  }    
  printf("\n");

  return 0;


}

