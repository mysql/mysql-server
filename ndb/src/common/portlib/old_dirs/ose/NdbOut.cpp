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

#include "NdbOut.hpp"

#if defined NDB_SOFTOSE
#include <dbgprintf.h>
#define printfunc dbgprintf
#else
#define printfunc printf
#endif

static char const* const endlineString = "\r\n";

static int CtrlC = 0;
NdbOut ndbout;


NdbOut& NdbOut::operator<<(int aVal)
{
  char* format;
  char HexFormat[] = "0x%08x";
  char DecFormat[] = "%d";
  if (isHexFormat == 1)
    format = HexFormat;
  else
    format = DecFormat;

  printfunc(format, aVal);
  return *this;
}

NdbOut& NdbOut::operator<<(char* pVal)
{
  printfunc("%s", pVal);
  return *this;
}

NdbOut& NdbOut::endline()
{
  isHexFormat = 0; // Reset hex to normal, if user forgot this
  printfunc(endlineString);
  return *this;
}

NdbOut& NdbOut::flushline()
{
  isHexFormat = 0; // Reset hex to normal, if user forgot this
  return *this;
}

NdbOut& NdbOut::setHexFormat(int _format)
{
  isHexFormat = _format;
  return *this;
}

NdbOut::NdbOut()
{
  CtrlC = 0;
  isHexFormat = 0;
}

NdbOut::~NdbOut()
{
}



extern "C"
void 
ndbout_c(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf << endl;
  va_end(ap);
}
