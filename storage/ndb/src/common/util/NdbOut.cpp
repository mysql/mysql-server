/* Copyright (c) 2003, 2023, Oracle and/or its affiliates.


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

#include "portlib/ndb_compiler.h"
#include <ndb_global.h>

#include <NdbOut.hpp>
#include <OutputStream.hpp>

/* Initialized in ndb_init() */
NdbOut ndbout;
NdbOut ndberr;

static const char * fms[] = {
  "%d", "0x%02x",      // Int8
  "%u", "0x%02x",      // Uint8
  "%d", "0x%04x",      // Int16
  "%u", "0x%04x",      // Uint16
  "%d", "0x%08x",      // Int32
  "%u", "0x%08x",      // Uint32
  "%lld", "0x%016llx", // Int64
  "%llu", "0x%016llx", // Uint64
  "%llu", "0x%016llx"  // UintPtr
};

NdbOut& 
NdbOut::operator<<(Int8 v)   { m_out->print(fms[0+isHex],(int)v); return *this;}
NdbOut& 
NdbOut::operator<<(Uint8 v)  { m_out->print(fms[2+isHex],(int)v); return *this;}
NdbOut& 
NdbOut::operator<<(Int16 v)  { m_out->print(fms[4+isHex],(int)v); return *this;}
NdbOut& 
NdbOut::operator<<(Uint16 v) { m_out->print(fms[6+isHex],(int)v); return *this;}
NdbOut& 
NdbOut::operator<<(Int32 v)  { m_out->print(fms[8+isHex], v); return *this;}
NdbOut& 
NdbOut::operator<<(Uint32 v) { m_out->print(fms[10+isHex], v); return *this;}
NdbOut& 
NdbOut::operator<<(Int64 v)  { m_out->print(fms[12+isHex], v); return *this;}
NdbOut& 
NdbOut::operator<<(Uint64 v) { m_out->print(fms[14+isHex], v); return *this;}
NdbOut& 
NdbOut::operator<<(unsigned long int v) { return *this << (Uint64) v; }

NdbOut& 
NdbOut::operator<<(const char* val){ m_out->print("%s", val ? val : "(null)"); return * this; }
NdbOut& 
NdbOut::operator<<(const void* val){ m_out->print("%p", val); return * this; }
NdbOut&
NdbOut::operator<<(BaseString &val){ return *this << val.c_str(); }

NdbOut& 
NdbOut::operator<<(float val){ m_out->print("%f", (double)val); return * this;}
NdbOut& 
NdbOut::operator<<(double val){ m_out->print("%f", val); return * this; }

NdbOut& NdbOut::endline()
{
  isHex = 0; // Reset hex to normal, if user forgot this
  m_out->println("%s", "");
  return flushline(false);
}

NdbOut& NdbOut::flushline(bool force)
{
  if (force || m_autoflush)
  {
    m_out->flush();
  }
  return *this;
}

NdbOut& NdbOut::setHexFormat(int _format)
{
  isHex = (_format == 0 ? 0 : 1);
  return *this;
}

NdbOut& NdbOut::hexdump(const Uint32* words, size_t count)
{
  char buf[90 * 11 + 4 + 1];
  size_t offset = BaseString::hexdump(buf, sizeof(buf), words, count);
  m_out->write(buf, offset);  
  return *this;
}

NdbOut::NdbOut(OutputStream & out, bool autoflush)
  : m_out(& out), isHex(0), m_autoflush(autoflush)
{
}

NdbOut::NdbOut()
  : m_out(nullptr), isHex(0)
{
   /**
    * m_out set to NULL!
    */
}

NdbOut::~NdbOut()
{
   /**
    *  don't delete m_out, as it's a reference given to us.
    *  i.e we don't "own" it
    */
}

void
NdbOut::print(const char * fmt, ...)
{
  if (fmt == nullptr)
  {
    /*
     Function was called with fmt being NULL, this is an error
     but handle it gracefully by simpling printing nothing
     instead of continuing down the line with the NULL pointer.

     Catch problem with an assert in debug compile.
    */
    assert(false);
    return;
  }

  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  *this << buf;
  va_end(ap);
}

void
NdbOut::println(const char * fmt, ...)
{
  if (fmt == nullptr)
  {
    /*
     Function was called with fmt being NULL, this is an error
     but handle it gracefully by simpling printing nothing
     instead of continuing down the line with the NULL pointer.

     Catch problem with an assert in debug compile.
    */
    assert(false);
    *this << endl;
    return;
  }

  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  size_t len = BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  if (len > sizeof(buf) - 2) len = sizeof(buf) - 2;
  memcpy(&buf[len], "\n", 2);
  *this << buf;
  flushline(false);
  va_end(ap);
}

static
void
vndbout_c(const char * fmt, va_list ap) ATTRIBUTE_FORMAT(printf, 1, 0);

static
void
vndbout_c(const char * fmt, va_list ap)
{
  if (fmt == nullptr)
  {
    /*
     Function was called with fmt being NULL, this is an error
     but handle it gracefully by simpling printing an empty newline
     instead of continuing down the line with the NULL pointer.

     Catch problem with an assert in debug compile.
    */
    assert(false);
    ndbout << endl; // Empty newline
    return;
  }

  char buf[1000];
  size_t len = BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  if (len > sizeof(buf) - 2) len = sizeof(buf) - 2;
  memcpy(&buf[len], "\n", 2);
  ndbout << buf;
  ndbout.flushline(false);
}

extern "C"
void
ndbout_c(const char * fmt, ...){
  va_list ap;

  va_start(ap, fmt);
  vndbout_c(fmt, ap);
  va_end(ap);
}

FilteredNdbOut::FilteredNdbOut(OutputStream & out, 
			       int threshold, int level)
  : NdbOut(out) {
  m_level = level;
  m_threshold = threshold;
  m_org = &out;
  m_null = new NullOutputStream();
  setLevel(level);
}

FilteredNdbOut::~FilteredNdbOut(){
  delete m_null;
}

void
FilteredNdbOut::setLevel(int i){
  m_level = i;
  if(m_level >= m_threshold){
    m_out = m_org;
  } else {
    m_out = m_null;
  }
}

void
FilteredNdbOut::setThreshold(int i){
  m_threshold = i;
  setLevel(m_level);
}

int
FilteredNdbOut::getLevel() const {
  return m_level;
}
int
FilteredNdbOut::getThreshold() const {
  return m_threshold;
}

static FileOutputStream ndbouts_fileoutputstream(nullptr);
static FileOutputStream ndberrs_fileoutputstream(nullptr);

void
NdbOut_Init()
{
  new (&ndbouts_fileoutputstream) FileOutputStream(stdout);
  new (&ndbout) NdbOut(ndbouts_fileoutputstream);

  new (&ndberrs_fileoutputstream) FileOutputStream(stderr);
  new (&ndberr) NdbOut(ndberrs_fileoutputstream);
}

void
NdbOut_ReInit(OutputStream* stdout_ostream,
              OutputStream* stderr_ostream)
{
  /**
   * Re-initialise ndbout and ndberr globals with different OutputStreams
   * Not thread safe, should be done at process start
   */

  /**
   * Following probably can be removed as destructors(same file) are empty,
   * but are present to handle any future changes
   */
    ndbout.~NdbOut();
    //ndberr.~NdbErr();

   new (&ndbout) NdbOut(*stdout_ostream);
   new (&ndberr) NdbOut(*stderr_ostream);
}
