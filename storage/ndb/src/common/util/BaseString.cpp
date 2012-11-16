/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* -*- c-basic-offset: 4; -*- */
#include <ndb_global.h>
#include <BaseString.hpp>
#include "basestring_vsnprintf.h"

BaseString::BaseString()
{
    m_chr = new char[1];
    if (m_chr == NULL)
    {
      errno = ENOMEM;
      m_len = 0;
      return;
    }
    m_chr[0] = 0;
    m_len = 0;
}

BaseString::BaseString(const char* s)
{
    if (s == NULL)
    {
      m_chr = NULL;
      m_len = 0;
      return;
    }
    const size_t n = strlen(s);
    m_chr = new char[n + 1];
    if (m_chr == NULL)
    {
      errno = ENOMEM;
      m_len = 0;
      return;
    }
    memcpy(m_chr, s, n + 1);
    m_len = (unsigned)n;
}

BaseString::BaseString(const char * s, size_t n)
{
  if (s == NULL || n == 0)
  {
    m_chr = NULL;
    m_len = 0;
    return;
  }
  m_chr = new char[n + 1];
  if (m_chr == NULL)
  {
    errno = ENOMEM;
    m_len = 0;
    return;
  }
  memcpy(m_chr, s, n);
  m_chr[n] = 0;
  m_len = (unsigned)n;
}

BaseString::BaseString(const BaseString& str)
{
    const char* const s = str.m_chr;
    const size_t n = str.m_len;
    if (s == NULL)
    {
      m_chr = NULL;
      m_len = 0;
      return;
    }
    char* t = new char[n + 1];
    if (t == NULL)
    {
      errno = ENOMEM;
      m_chr = NULL;
      m_len = 0;
      return;
    }
    memcpy(t, s, n + 1);
    m_chr = t;
    m_len = (unsigned)n;
}

BaseString::~BaseString()
{
    delete[] m_chr;
}

BaseString&
BaseString::assign(const char* s)
{
    if (s == NULL)
    {
      if (m_chr)
        delete[] m_chr;
      m_chr = NULL;
      m_len = 0;
      return *this;
    }
    size_t n = strlen(s);
    char* t = new char[n + 1];
    if (t)
    {
      memcpy(t, s, n + 1);
    }
    else
    {
      errno = ENOMEM;
      n = 0;
    }
    delete[] m_chr;
    m_chr = t;
    m_len = (unsigned)n;
    return *this;
}

BaseString&
BaseString::assign(const char* s, size_t n)
{
    char* t = new char[n + 1];
    if (t)
    {
      memcpy(t, s, n);
      t[n] = 0;
    }
    else
    {
      errno = ENOMEM;
      n = 0;
    }
    delete[] m_chr;
    m_chr = t;
    m_len = (unsigned)n;
    return *this;
}

BaseString&
BaseString::assign(const BaseString& str, size_t n)
{
    if (n > str.m_len)
	n = str.m_len;
    return assign(str.m_chr, n);
}

BaseString&
BaseString::append(const char* s)
{
    if (s == NULL)
      return *this;

    size_t n = strlen(s);
    char* t = new char[m_len + n + 1];
    if (t)
    {
      memcpy(t, m_chr, m_len);
      memcpy(t + m_len, s, n + 1);
    }
    else
    {
      errno = ENOMEM;
      m_len = 0;
      n = 0;
    }
    delete[] m_chr;
    m_chr = t;
    m_len += (unsigned)n;
    return *this;
}

BaseString&
BaseString::append(char c) {
    return appfmt("%c", c);
}

BaseString&
BaseString::append(const BaseString& str)
{
    return append(str.m_chr);
}

BaseString&
BaseString::append(const Vector<BaseString> &vector,
		   const BaseString &separator) {
    for(unsigned i=0;i<vector.size(); i++) {
	append(vector[i]);
	if(i<vector.size()-1)
	    append(separator);
    }
    return *this;
}

BaseString&
BaseString::assfmt(const char *fmt, ...)
{
    char buf[1];
    va_list ap;
    int l;

    /* Figure out how long the formatted string will be. A small temporary
     * buffer is used, because I don't trust all implementations to work
     * when called as vsnprintf(NULL, 0, ...).
     */
    va_start(ap, fmt);
    l = basestring_vsnprintf(buf, sizeof(buf), fmt, ap) + 1;
    va_end(ap);
    if(l > (int)m_len) {
        char *t = new char[l];
        if (t == NULL)
        {
          errno = ENOMEM;
          return *this;
        }
	delete[] m_chr;
	m_chr = t;
    }
    va_start(ap, fmt);
    l = basestring_vsnprintf(m_chr, l, fmt, ap);
    assert(l == (int)strlen(m_chr));
    va_end(ap);
    m_len = (unsigned)strlen(m_chr);
    return *this;
}

BaseString&
BaseString::appfmt(const char *fmt, ...)
{
    char buf[1];
    va_list ap;
    int l;

    /* Figure out how long the formatted string will be. A small temporary
     * buffer is used, because I don't trust all implementations to work
     * when called as vsnprintf(NULL, 0, ...).
     */
    va_start(ap, fmt);
    l = basestring_vsnprintf(buf, sizeof(buf), fmt, ap) + 1;
    va_end(ap);
    char *tmp = new char[l];
    if (tmp == NULL)
    {
      errno = ENOMEM;
      return *this;
    }
    va_start(ap, fmt);
    basestring_vsnprintf(tmp, l, fmt, ap);
    va_end(ap);
    append(tmp);
    delete[] tmp;
    return *this;
}

BaseString&
BaseString::operator=(const BaseString& str)
{
    if (this != &str) {
	this->assign(str);
    }
    return *this;
}

int
BaseString::split(Vector<BaseString> &v,
		  const BaseString &separator,
		  int maxSize) const {
    char *str = strdup(m_chr);
    int i, start, len, num = 0;
    len = (int)strlen(str);
    for(start = i = 0;
	(i <= len) && ( (maxSize<0) || ((int)v.size()<=maxSize-1) );
	i++) {
	if(strchr(separator.c_str(), str[i]) || i == len) {
	    if(maxSize < 0 || (int)v.size() < maxSize-1)
		str[i] = '\0';
	    v.push_back(BaseString(str+start));
	    num++;
	    start = i+1;
	}
    }
    free(str);

    return num;
}

ssize_t
BaseString::indexOf(char c, size_t pos) const {

  if (pos >= m_len)
    return -1;

    char *p = strchr(m_chr + pos, c);
    if(p == NULL)
	return -1;
    return (ssize_t)(p-m_chr);
}

ssize_t
BaseString::indexOf(const char * needle, size_t pos) const {

  if (pos >= m_len)
    return -1;

    char *p = strstr(m_chr + pos, needle);
    if(p == NULL)
	return -1;
    return (ssize_t)(p-m_chr);
}

ssize_t
BaseString::lastIndexOf(char c) const {
    char *p;
    p = strrchr(m_chr, c);
    if(p == NULL)
	return -1;
    return (ssize_t)(p-m_chr);
}

BaseString
BaseString::substr(ssize_t start, ssize_t stop) const {
    if(stop < 0)
	stop = length();
    ssize_t len = stop-start;
    if(len <= 0)
	return BaseString("");
    BaseString s;
    s.assign(m_chr+start, len);
    return s;
}

static bool
iswhite(char c) {
  switch(c) {
  case ' ':
  case '\t':
    return true;
  default:
    return false;
  }
  /* NOTREACHED */
}

char **
BaseString::argify(const char *argv0, const char *src) {
    Vector<char *> vargv;
    
    if(argv0 != NULL)
    {
      char *t = strdup(argv0);
      if (t == NULL)
      {
        errno = ENOMEM;
        return NULL;
      }
      if (vargv.push_back(t))
      {
        free(t);
        return NULL;
      }
    }
    
    char *tmp = new char[strlen(src)+1];
    if (tmp == NULL)
    {
      for(unsigned i = 0; i < vargv.size(); i++)
        free(vargv[i]);
      errno = ENOMEM;
      return NULL;
    }
    char *dst = tmp;
    const char *end = src + strlen(src);
    /* Copy characters from src to destination, while compacting them
     * so that all whitespace is compacted and replaced by a NUL-byte.
     * At the same time, add pointers to strings in the vargv vector.
     * When whitespace is detected, the characters '"' and '\' are honored,
     * to make it possible to give arguments containing whitespace.
     * The semantics of '"' and '\' match that of most Unix shells.
     */
    while(src < end && *src) {
	/* Skip initial whitespace */
	while(src < end && *src && iswhite(*src))
	    src++;
	
	char *begin = dst;
	while(src < end && *src) {
	    /* Handle '"' quotation */
	    if(*src == '"') {
		src++;
		while(src < end && *src && *src != '"') {
		    if(*src == '\\')
			src++;
		    *dst++ = *src++;
		}
		src++;
		if(src >= end)
		    goto end;
	    }
	    
	    /* Handle '\' */
	    if(*src == '\\')
		src++;
	    else if(iswhite(*src))
		break;

	    /* Actually copy characters */
	    *dst++ = *src++;
	}
	
	/* Make sure the string is properly terminated */
	*dst++ = '\0';
	src++;

        {
          char *t = strdup(begin);
          if (t == NULL)
          {
            delete[] tmp;
            for(unsigned i = 0; i < vargv.size(); i++)
              free(vargv[i]);
            errno = ENOMEM;
            return NULL;
          }
          if (vargv.push_back(t))
          {
            free(t);
            delete[] tmp;
            for(unsigned i = 0; i < vargv.size(); i++)
              free(vargv[i]);
            return NULL;
          }
        }
    }
 end:
    
    delete[] tmp;
    if (vargv.push_back(NULL))
    {
      for(unsigned i = 0; i < vargv.size(); i++)
        free(vargv[i]);
      return NULL;
    }
    
    /* Convert the C++ Vector into a C-vector of strings, suitable for
     * calling execv().
     */
    char **argv = (char **)malloc(sizeof(*argv) * (vargv.size()));
    if(argv == NULL)
    {
        for(unsigned i = 0; i < vargv.size(); i++)
          free(vargv[i]);
        errno = ENOMEM;
	return NULL;
    }
    
    for(unsigned i = 0; i < vargv.size(); i++){
	argv[i] = vargv[i];
    }
    
    return argv;
}

BaseString&
BaseString::trim(const char * delim){
    trim(m_chr, delim);
    m_len = (unsigned)strlen(m_chr);
    return * this;
}

char*
BaseString::trim(char * str, const char * delim){
    int len = (int)strlen(str) - 1;
    for(; len > 0 && strchr(delim, str[len]); len--)
      ;

    int pos = 0;
    for(; pos <= len && strchr(delim, str[pos]); pos++)
      ;

    if(pos > len){
	str[0] = 0;
	return 0;
    } else {
	memmove(str, &str[pos], len - pos + 1);
	str[len-pos+1] = 0;
    }
    
    return str;
}

int
BaseString::vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
  return(basestring_vsnprintf(str, size, format, ap));
}

int
BaseString::snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int ret= basestring_vsnprintf(str, size, format, ap);
  va_end(ap);
  return(ret);
}

BaseString
BaseString::getText(unsigned size, const Uint32 data[])
{
  BaseString to;
  char * buf = (char*)malloc(32*size+1);
  if (buf)
  {
    BitmaskImpl::getText(size, data, buf);
    to.append(buf);
    free(buf);
  }
  return to;
}

BaseString
BaseString::getPrettyText(unsigned size, const Uint32 data[])
{
  const char* delimiter = "";
  unsigned found = 0;
  const unsigned MAX_BITS = sizeof(Uint32) * 8 * size;
  BaseString to;
  for (unsigned i = 0; i < MAX_BITS; i++)
  {
    if (BitmaskImpl::get(size, data, i))
    {
      to.appfmt("%s%d", delimiter, i);
      found++;
      if (found < BitmaskImpl::count(size, data) - 1)
        delimiter = ", ";
      else
        delimiter = " and ";
    }
  }
  return to;
}

BaseString
BaseString::getPrettyTextShort(unsigned size, const Uint32 data[])
{
  const char* delimiter = "";
  const unsigned MAX_BITS = sizeof(Uint32) * 8 * size;
  BaseString to;
  for (unsigned i = 0; i < MAX_BITS; i++)
  {
    if (BitmaskImpl::get(size, data, i))
    {
      to.appfmt("%s%d", delimiter, i);
      delimiter = ",";
    }
  }
  return to;
}

const void*
BaseString_get_key(const void* key, size_t* key_length)
{
  const BaseString* str = (const BaseString*)key;
  *key_length = str->length();
  return str->c_str();
}

#ifdef TEST_BASE_STRING

#include <NdbTap.hpp>

TAPTEST(BaseString)
{
    BaseString s("abc");
    BaseString t(s);
    s.assign("def");
    t.append("123");
    OK(s == "def");
    OK(t == "abc123");
    s.assign("");
    t.assign("");
    for (unsigned i = 0; i < 1000; i++) {
	s.append("xyz");
	t.assign(s);
	OK(strlen(t.c_str()) % 3 == 0);
    }

    {
	BaseString s(":123:abc:;:foo:");
	Vector<BaseString> v;
	OK(s.split(v, ":;") == 7);

	OK(v[0] == "");
	OK(v[1] == "123");
	OK(v[2] == "abc");
	OK(v[3] == "");
	OK(v[4] == "");
	OK(v[5] == "foo");
	OK(v[6] == "");
    }

    {
	BaseString s(":123:abc:foo:bar");
	Vector<BaseString> v;
	OK(s.split(v, ":;", 4) == 4);

	OK(v[0] == "");
	OK(v[1] == "123");
	OK(v[2] == "abc");
	OK(v[3] == "foo:bar");

	BaseString n;
	n.append(v, "()");
	OK(n == "()123()abc()foo:bar");
	n = "";
	n.append(v);
	OK(n == " 123 abc foo:bar");
    }

    {
	OK(BaseString("hamburger").substr(4,2) == "");
	OK(BaseString("hamburger").substr(3) == "burger");
	OK(BaseString("hamburger").substr(4,8) == "urge");
	OK(BaseString("smiles").substr(1,5) == "mile");
	OK(BaseString("012345").indexOf('2') == 2);
	OK(BaseString("hej").indexOf('X') == -1);
    }

    {
	OK(BaseString(" 1").trim(" ") == "1");
	OK(BaseString("1 ").trim(" ") == "1");
	OK(BaseString(" 1 ").trim(" ") == "1");
	OK(BaseString("abc\t\n\r kalleabc\t\r\n").trim("abc\t\r\n ") == "kalle");
	OK(BaseString(" ").trim(" ") == "");
    }

    // Tests for BUG#38662
    BaseString s2(NULL);
    BaseString s3;
    BaseString s4("elf");

    OK(s3.append((const char*)NULL) == "");
    OK(s4.append((const char*)NULL) == "elf");
    OK(s4.append(s3) == "elf");
    OK(s4.append(s2) == "elf");
    OK(s4.append(s4) == "elfelf");

    OK(s3.assign((const char*)NULL).c_str() == NULL);
    OK(s4.assign((const char*)NULL).c_str() == NULL);
    OK(s4.assign(s4).c_str() == NULL);

    //tests for Bug #45733 Cluster with more than 4 storage node 
    for(int i=0;i<20;i++) 
    {
#define BIG_ASSFMT_OK(X) do{u_int x=(X);OK(s2.assfmt("%*s",x,"Z").length() == x);}while(0)
      BIG_ASSFMT_OK(8);
      BIG_ASSFMT_OK(511);
      BIG_ASSFMT_OK(512);
      BIG_ASSFMT_OK(513);
      BIG_ASSFMT_OK(1023);
      BIG_ASSFMT_OK(1024);
      BIG_ASSFMT_OK(1025);
      BIG_ASSFMT_OK(20*1024*1024);
    }

    return 1; // OK
}

#endif

template class Vector<BaseString>;
