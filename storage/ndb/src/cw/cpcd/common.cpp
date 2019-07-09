/*
   Copyright (C) 2003-2006 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "common.hpp"
#include <logger/Logger.hpp>

#ifndef _WIN32
#include <pwd.h>
#endif

#include <Properties.hpp>
#include <BaseString.hpp>

int debug = 0;

Logger logger;

#ifndef _WIN32
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
#endif

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
