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

#include <OutputStream.hpp>
#include "NdbOut.hpp"
#include "rep_version.hpp"

int replogEnabled;

/**
 * @todo  This should be implemented using MySQLs dbug library
 */
#if 0
extern "C"
void 
DBUG_PRINT(const char * fmt, ...)
{
#ifdef DBUG
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf << endl;
  va_end(ap);
#endif
}
#endif

extern "C"
void 
replog(const char * fmt, ...)
{
  if (replogEnabled)
  {
    va_list ap;
    char buf[1000];
    
    va_start(ap, fmt);
    if (fmt != 0)
      vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    ndbout << buf << endl;
    va_end(ap);
  }
}

extern "C"
void 
rlog(const char * fmt, ...)
{
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf;
  va_end(ap);
}
