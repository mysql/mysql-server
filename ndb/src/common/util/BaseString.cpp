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

/* -*- c-basic-offset: 4; -*- */
#include <ndb_global.h>
#include <BaseString.hpp>

BaseString::BaseString()
{
    m_chr = new char[1];
    m_chr[0] = 0;
    m_len = 0;
}

BaseString::BaseString(const char* s)
{
    const size_t n = strlen(s);
    m_chr = new char[n + 1];
    memcpy(m_chr, s, n + 1);
    m_len = n;
}

BaseString::BaseString(const BaseString& str)
{
    const char* const s = str.m_chr;
    const size_t n = str.m_len;
    char* t = new char[n + 1];
    memcpy(t, s, n + 1);
    m_chr = t;
    m_len = n;
}

BaseString::~BaseString()
{
    delete[] m_chr;
}

BaseString&
BaseString::assign(const char* s)
{
    const size_t n = strlen(s);
    char* t = new char[n + 1];
    memcpy(t, s, n + 1);
    delete[] m_chr;
    m_chr = t;
    m_len = n;
    return *this;
}

BaseString&
BaseString::assign(const char* s, size_t n)
{
    char* t = new char[n + 1];
    memcpy(t, s, n);
    t[n] = 0;
    delete[] m_chr;
    m_chr = t;
    m_len = n;
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
    const size_t n = strlen(s);
    char* t = new char[m_len + n + 1];
    memcpy(t, m_chr, m_len);
    memcpy(t + m_len, s, n + 1);
    delete[] m_chr;
    m_chr = t;
    m_len += n;
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
    for(size_t i=0;i<vector.size(); i++) {
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
    l = vsnprintf(buf, sizeof(buf), fmt, ap) + 1;
    va_end(ap);
    if(l > (int)m_len) {
	delete[] m_chr;
	m_chr = new char[l];
    }
    va_start(ap, fmt);
    vsnprintf(m_chr, l, fmt, ap);
    va_end(ap);
    m_len = strlen(m_chr);
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
    l = vsnprintf(buf, sizeof(buf), fmt, ap) + 1;
    va_end(ap);
    char *tmp = new char[l];
    va_start(ap, fmt);
    vsnprintf(tmp, l, fmt, ap);
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
    len = strlen(str);
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
BaseString::indexOf(char c) {
    char *p;
    p = strchr(m_chr, c);
    if(p == NULL)
	return -1;
    return (ssize_t)(p-m_chr);
}

ssize_t
BaseString::lastIndexOf(char c) {
    char *p;
    p = strrchr(m_chr, c);
    if(p == NULL)
	return -1;
    return (ssize_t)(p-m_chr);
}

BaseString
BaseString::substr(ssize_t start, ssize_t stop) {
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
	vargv.push_back(strdup(argv0));
    
    char *tmp = new char[strlen(src)+1];
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
	
	vargv.push_back(strdup(begin));
    }
 end:
    
    delete[] tmp;
    vargv.push_back(NULL);
    
    /* Convert the C++ Vector into a C-vector of strings, suitable for
     * calling execv().
     */
    char **argv = (char **)malloc(sizeof(*argv) * (vargv.size()));
    if(argv == NULL)
	return NULL;
    
    for(size_t i = 0; i < vargv.size(); i++){
	argv[i] = vargv[i];
    }
    
    return argv;
}

BaseString&
BaseString::trim(const char * delim){
    trim(m_chr, delim);
    m_len = strlen(m_chr);
    return * this;
}

char*
BaseString::trim(char * str, const char * delim){
    int len = strlen(str) - 1;
    for(; len > 0 && strchr(delim, str[len]); len--);
    
    int pos = 0;
    for(; pos <= len && strchr(delim, str[pos]); pos++);
    
    if(pos > len){
	str[0] = 0;
	return 0;
    } else {
	memmove(str, &str[pos], len - pos + 1);
	str[len-pos+1] = 0;
    }
    
    return str;
}


#ifdef TEST_BASE_STRING

/*
g++ -g -Wall -o tbs -DTEST_BASE_STRING -I$NDB_TOP/include/util \
        -I$NDB_TOP/include/portlib BaseString.cpp
valgrind ./tbs
*/

int main()
{
    BaseString s("abc");
    BaseString t(s);
    s.assign("def");
    t.append("123");
    assert(s == "def");
    assert(t == "abc123");
    s.assign("");
    t.assign("");
    for (unsigned i = 0; i < 1000; i++) {
	s.append("xyz");
	t.assign(s);
	assert(strlen(t.c_str()) % 3 == 0);
    }

    {
	BaseString s(":123:abc:;:foo:");
	Vector<BaseString> v;
	assert(s.split(v, ":;") == 7);

	assert(v[0] == "");
	assert(v[1] == "123");
	assert(v[2] == "abc");
	assert(v[3] == "");
	assert(v[4] == "");
	assert(v[5] == "foo");
	assert(v[6] == "");
    }

    {
	BaseString s(":123:abc:foo:bar");
	Vector<BaseString> v;
	assert(s.split(v, ":;", 4) == 4);

	assert(v[0] == "");
	assert(v[1] == "123");
	assert(v[2] == "abc");
	assert(v[3] == "foo:bar");

	BaseString n;
	n.append(v, "()");
	assert(n == "()123()abc()foo:bar");
	n = "";
	n.append(v);
	assert(n == " 123 abc foo:bar");
    }

    {
	assert(BaseString("hamburger").substr(4,2) == "");
	assert(BaseString("hamburger").substr(3) == "burger");
	assert(BaseString("hamburger").substr(4,8) == "urge");
	assert(BaseString("smiles").substr(1,5) == "mile");
	assert(BaseString("012345").indexOf('2') == 2);
	assert(BaseString("hej").indexOf('X') == -1);
    }

    {
	assert(BaseString(" 1").trim(" ") == "1");
	assert(BaseString("1 ").trim(" ") == "1");
	assert(BaseString(" 1 ").trim(" ") == "1");
	assert(BaseString("abc\t\n\r kalleabc\t\r\n").trim("abc\t\r\n ") == "kalle");
	assert(BaseString(" ").trim(" ") == "");
    }
    return 0;
}

#endif

template class Vector<char *>;
template class Vector<BaseString>;
