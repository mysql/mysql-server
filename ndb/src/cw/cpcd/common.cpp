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

#include <ndb_global.h>

#include "common.hpp"
#include <logger/Logger.hpp>
#include <pwd.h>

#include <Properties.hpp>
#include <BaseString.hpp>

int debug = 0;

Logger logger;

int
runas(const char * user){
  if(user == 0 || strlen(user) == 0){
    return 0;
  }
  struct passwd * pw = getpwnam(user);
  if(pw == 0){
    logger.error("Can't find user to %s", user);
    return -1;
  }
  uid_t uid = pw->pw_uid;
  gid_t gid = pw->pw_gid;
  int res = setgid(gid);
  if(res != 0){
    logger.error("Can't change group to %s(%d)", user, gid);
    return res;
  }

  res = setuid(uid);
  if(res != 0){
    logger.error("Can't change user to %s(%d)", user, uid);
  }
  return res;
}

int
insert(const char * pair, Properties & p){
  BaseString tmp(pair);
  
  tmp.trim(" \t\n\r");

  Vector<BaseString> split;
  tmp.split(split, ":=", 2);

  if(split.size() != 2)
    return -1;

  p.put(split[0].trim().c_str(), split[1].trim().c_str()); 

  return 0;
}

int
insert_file(FILE * f, class Properties& p, bool break_on_empty){
  if(f == 0)
    return -1;

  while(!feof(f)){
    char buf[1024];
    fgets(buf, 1024, f);
    BaseString tmp = buf;

    if(tmp.length() > 0 && tmp.c_str()[0] == '#')
      continue;

    if(insert(tmp.c_str(), p) != 0 && break_on_empty)
      break;
  }

  return 0;
}

int
insert_file(const char * filename, class Properties& p){
  FILE * f = fopen(filename, "r");
  int res = insert_file(f, p);
  if(f) fclose(f);
  return res;
}

int
parse_config_file(struct getargs args[], int num_arg, const Properties& p){
  Properties::Iterator it(&p);
  for(const char * name = it.first(); name != 0; name = it.next()){
    bool found = false;
    for(int i = 0; i<num_arg; i++){
      if(strcmp(name, args[i].long_name) != 0)
	continue;
      
      found = true;

      const char * tmp;
      p.get(name, &tmp);

      int t = 1;

      switch(args[i].type){
      case arg_integer:{
	int val = atoi(tmp);
	if(args[i].value){
	  *((int*)args[i].value) = val;
	}
      }
	break;
      case arg_string:
	if(args[i].value){
	  *((const char**)args[i].value) = tmp;
	}
	break;
      case arg_negative_flag:
	t = 0;
      case arg_flag:
	if(args[i].value){
	  if(!strcasecmp(tmp, "y") || 
	     !strcasecmp(tmp, "on") ||
	     !strcasecmp(tmp, "true") ||
	     !strcasecmp(tmp, "1")){
	    *((int*)args[i].value) = t;	    
	  }
	  if(!strcasecmp(tmp, "n") || 
	     !strcasecmp(tmp, "off") ||
	     !strcasecmp(tmp, "false") ||
	     !strcasecmp(tmp, "0")){
	    *((int*)args[i].value) = t;	    
	  }
	}
	t = 1;
	break;
      case arg_strings:
      case arg_double:
      case arg_collect:
      case arg_counter:
	break;
      }
    }
    if(!found) {
      printf("Unknown parameter: %s\n", name);
      return 1;
    }
  }
  return 0;
}
