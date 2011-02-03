/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBOUT_H
#define NDBOUT_H

#ifdef	__cplusplus

#include <ndb_types.h>
#include <util/BaseString.hpp>
#include <ndb_global.h>

/**
 * Class used for outputting logging messages to screen.
 * Since the output capabilities are different on different platforms
 * this middle layer class should be used for all output messages
 */

/* 
   Example usage:
   
   #include "NdbOut.hpp"
   
   / *  Use ndbout as you would use cout:
   
        ndbout << "Hello World! "<< 1 << " Hello again" 
               << 67 << anIntegerVar << "Hup << endl;
   
   
   / * Use ndbout_c as you would use printf:
   
       ndbout_c("Hello World %d\n", 1);
*/

class NdbOut;
class OutputStream;
class NullOutputStream;

/*  Declare a static variable of NdbOut as ndbout */
extern NdbOut ndbout, ndberr;

class NdbOut
{
public:
  NdbOut& operator<<(NdbOut& (* _f)(NdbOut&));
  NdbOut& operator<<(Int8);
  NdbOut& operator<<(Uint8);
  NdbOut& operator<<(Int16);
  NdbOut& operator<<(Uint16);
  NdbOut& operator<<(Int32);
  NdbOut& operator<<(Uint32);
  NdbOut& operator<<(Int64);
  NdbOut& operator<<(Uint64);
  NdbOut& operator<<(long unsigned int);
  NdbOut& operator<<(const char*);
  NdbOut& operator<<(const unsigned char*);
  NdbOut& operator<<(BaseString &);
  NdbOut& operator<<(const void*);
  NdbOut& operator<<(float);
  NdbOut& operator<<(double);
  NdbOut& endline(void);
  NdbOut& flushline(void);
  NdbOut& setHexFormat(int _format);
  
  NdbOut();
  NdbOut(OutputStream &, bool autoflush = true);
  virtual ~NdbOut();

  void print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  void println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  
  OutputStream * m_out;
private:
  void choose(const char * fmt,...);
  int isHex;
  bool m_autoflush;
};

#ifdef NDB_WIN
typedef int(*NdbOutF)(char*);
extern NdbOutF ndbout_svc;
#endif

inline NdbOut& NdbOut::operator<<(NdbOut& (* _f)(NdbOut&)) {
  (* _f)(*this); 
  return * this; 
}

inline NdbOut&  endl(NdbOut& _NdbOut) { 
  return _NdbOut.endline(); 
}

inline NdbOut&  flush(NdbOut& _NdbOut) { 
  return _NdbOut.flushline(); 
}

inline  NdbOut& hex(NdbOut& _NdbOut) {
  return _NdbOut.setHexFormat(1);
}

inline NdbOut& dec(NdbOut& _NdbOut) {
  return _NdbOut.setHexFormat(0);
}
extern "C"
void ndbout_c(const char * fmt, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
extern "C"
void vndbout_c(const char * fmt, va_list ap);

class FilteredNdbOut : public NdbOut {
public:
  FilteredNdbOut(OutputStream &, int threshold = 0, int level = 0);
  virtual ~FilteredNdbOut();

  void setLevel(int i);
  void setThreshold(int i);

  int getLevel() const;
  int getThreshold() const;
  
private:
  int m_threshold, m_level;
  OutputStream * m_org;
  NullOutputStream * m_null;
};

#else
void ndbout_c(const char * fmt, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
#endif

#endif
