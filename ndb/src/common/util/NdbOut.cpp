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

#include <NdbOut.hpp>
#include <OutputStream.hpp>

static FileOutputStream ndbouts_fileoutputstream(stdout);
NdbOut ndbout(ndbouts_fileoutputstream);

static const char * fms[] = {
  "%d", "0x%02x",      // Int8
  "%u", "0x%02x",      // Uint8
  "%d", "0x%04x",      // Int16
  "%u", "0x%04x",      // Uint16
  "%d", "0x%08x",      // Int32
  "%u", "0x%08x",      // Uint32
  "%lld", "0x%016llx", // Int64
  "%llu", "0x%016llx"  // Uint64
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
  m_out->println("");
  m_out->flush();
  return *this;
}

NdbOut& NdbOut::flushline()
{
  m_out->flush();
  return *this;
}

NdbOut& NdbOut::setHexFormat(int _format)
{
  isHex = (_format == 0 ? 0 : 1);
  return *this;
}

NdbOut::NdbOut(OutputStream & out) 
  : m_out(& out)
{
  isHex = 0;
}

NdbOut::~NdbOut()
{
}

void
NdbOut::print(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf;
  va_end(ap);
}

void
NdbOut::println(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf << endl;
  va_end(ap);
}

extern "C"
void 
ndbout_c(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  ndbout << buf << endl;
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

