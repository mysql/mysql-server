/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __UTIL_BASESTRING_HPP_INCLUDED__
#define __UTIL_BASESTRING_HPP_INCLUDED__

#include <ndb_global.h>
#include <util/Vector.hpp>
#include "Bitmask.hpp"

/**
 * @class BaseString
 * @brief Null terminated strings
 */
class BaseString {
public:
  /** @brief Constructs an empty string */
  BaseString();

  /** @brief Constructs a copy of a char * */
  BaseString(const char* s);

  /** @brief Constructs a copy of a char * with length */
  BaseString(const char* s, size_t len);

  /** @brief Constructs a copy of another BaseString */
  BaseString(const BaseString& str);

  /** @brief Destructor */
  ~BaseString();

  /** @brief Returns a C-style string */
  const char* c_str() const;

  /** @brief Returns the length of the string */
  unsigned length() const;

  /** @brief Checks if the string is empty */
  bool empty() const;

  /** @brief Clear a string */
  void clear();

  /** @brief Convert to uppercase */
  BaseString& ndb_toupper();

  /** @brief Convert to lowercase */
  BaseString& ndb_tolower();

  /** @brief Assigns from a char * */
  BaseString& assign(const char* s);

  /** @brief Assigns from another BaseString */
  BaseString& assign(const BaseString& str);

  /** @brief Assigns from char *s, with maximum length n */
  BaseString& assign(const char* s, size_t n);

  /** @brief Assigns from another BaseString, with maximum length n */
  BaseString& assign(const BaseString& str, size_t n);

  /** 
   * Assings from a Vector of BaseStrings, each Vector entry
   * separated by separator.
   *
   * @param vector Vector of BaseStrings to append
   * @param separator Separation between appended strings
   */
  BaseString& assign(const Vector<BaseString> &vector,
		     const BaseString &separator = BaseString(" "));

  /** @brief Appends a char * to the end */
  BaseString& append(const char* s);

  /** @brief Appends a char to the end */
  BaseString& append(char c);

  /** @brief Appends another BaseString to the end */
  BaseString& append(const BaseString& str);

  /** 
   * Appends a Vector of BaseStrings to the end, each Vector entry
   * separated by separator.
   *
   * @param vector Vector of BaseStrings to append
   * @param separator Separation between appended strings
   */
  BaseString& append(const Vector<BaseString> &vector,
		     const BaseString &separator = BaseString(" "));

  /** @brief Assigns from a format string a la printf() */
  BaseString& assfmt(const char* ftm, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);

  /** @brief Appends a format string a la printf() to the end */
  BaseString& appfmt(const char* ftm, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);

  /**
   * Split a string into a vector of strings. Separate the string where
   * any character included in separator exists.
   * Maximally maxSize entries are added to the vector, if more separators
   * exist in the string, the remainder of the string will be appended
   * to the last entry in the vector.
   * The vector will not be cleared, so any existing strings in the
   * vector will remain.
   *
   * @param separator characters separating the entries
   * @param vector where the separated strings will be stored
   * @param maximum number of strings extracted
   *
   * @returns the number of string added to the vector
   */
  int split(Vector<BaseString> &vector, 
	    const BaseString &separator = BaseString(" "),
	    int maxSize = -1) const;

  /**
   * Returns the index of the first occurance of the character c.
   *
   * @params c character to look for
   * @params pos position to start searching from
   * @returns index of character, of -1 if no character found
   */
  ssize_t indexOf(char c, size_t pos = 0) const;

  /**
   * Returns the index of the first occurance of the string needle
   *
   * @params needle string to search for
   * @params pos position to start searching from
   * @returns index of character, of -1 if no character found
   */
  ssize_t indexOf(const char * needle, size_t pos = 0) const;

  /**
   * Returns the index of the last occurance of the character c.
   *
   * @params c character to look for
   * @returns index of character, of -1 if no character found
   */
  ssize_t lastIndexOf(char c) const;
  
  /**
   * Returns a subset of a string
   *
   * @param start index of first character
   * @param stop index of last character
   * @return a new string
   */
  BaseString substr(ssize_t start, ssize_t stop = -1) const;

  /**
   *  @brief Assignment operator
   */
  BaseString& operator=(const BaseString& str);

  /** @brief Compare two strings */
  bool operator<(const BaseString& str) const;
  /** @brief Are two strings equal? */
  bool operator==(const BaseString& str) const;
  /** @brief Are two strings equal? */
  bool operator==(const char *str) const;
  /** @brief Are two strings not equal? */
  bool operator!=(const BaseString& str) const;
  /** @brief Are two strings not equal? */
  bool operator!=(const char *str) const;

  /**
   * Trim string from <i>delim</i>
   */
  BaseString& trim(const char * delim = " \t");
  
  /**
   * Return c-array with strings suitable for execve
   * When whitespace is detected, the characters '"' and '\' are honored,
   * to make it possible to give arguments containing whitespace.
   * The semantics of '"' and '\' match that of most Unix shells.
   */
  static char** argify(const char *argv0, const char *src);

  /**
   * Trim string from <i>delim</i>
   */
  static char* trim(char * src, const char * delim);

  /**
   * snprintf on some platforms need special treatment
   */
  static int snprintf(char *str, size_t size, const char *format, ...)
    ATTRIBUTE_FORMAT(printf, 3, 4);
  static int vsnprintf(char *str, size_t size, const char *format, va_list ap)
    ATTRIBUTE_FORMAT(printf, 3, 0);

  template<unsigned size>
  static BaseString getText(const Bitmask<size>& mask) {
    return BaseString::getText(size, mask.rep.data);
  }

  template<unsigned size>
  static BaseString getPrettyText(const Bitmask<size>& mask) {
    return BaseString::getPrettyText(size, mask.rep.data);
  }
  template<unsigned size>
  static BaseString getPrettyTextShort(const Bitmask<size>& mask) {
    return BaseString::getPrettyTextShort(size, mask.rep.data);
  }

  template<unsigned size>
  static BaseString getText(const BitmaskPOD<size>& mask) {
    return BaseString::getText(size, mask.rep.data);
  }

  template<unsigned size>
  static BaseString getPrettyText(const BitmaskPOD<size>& mask) {
    return BaseString::getPrettyText(size, mask.rep.data);
  }
  template<unsigned size>
  static BaseString getPrettyTextShort(const BitmaskPOD<size>& mask) {
    return BaseString::getPrettyTextShort(size, mask.rep.data);
  }

  static BaseString getText(unsigned size, const Uint32 data[]);
  static BaseString getPrettyText(unsigned size, const Uint32 data[]);
  static BaseString getPrettyTextShort(unsigned size, const Uint32 data[]);

  static size_t hexdump(char * buf, size_t len, const Uint32 * wordbuf, size_t numwords);
private:
  char* m_chr;
  unsigned m_len;
  friend bool operator!(const BaseString& str);
};

inline const char*
BaseString::c_str() const
{
  return m_chr;
}

inline unsigned
BaseString::length() const
{
  return m_len;
}

inline bool
BaseString::empty() const
{
  return m_len == 0;
}

inline void
BaseString::clear()
{
  delete[] m_chr;
  m_chr = new char[1];
  m_chr[0] = 0;
  m_len = 0;
}

inline BaseString&
BaseString::ndb_toupper() {
  for(unsigned i = 0; i < length(); i++)
    m_chr[i] = toupper(m_chr[i]);
  return *this;
}

inline BaseString&
BaseString::ndb_tolower() {
  for(unsigned i = 0; i < length(); i++)
    m_chr[i] = tolower(m_chr[i]);
  return *this;
}

inline bool
BaseString::operator<(const BaseString& str) const
{
    return strcmp(m_chr, str.m_chr) < 0;
}

inline bool
BaseString::operator==(const BaseString& str) const
{
    return strcmp(m_chr, str.m_chr)  ==  0;
}

inline bool
BaseString::operator==(const char *str) const
{
    return strcmp(m_chr, str) == 0;
}

inline bool
BaseString::operator!=(const BaseString& str) const
{
    return strcmp(m_chr, str.m_chr)  !=  0;
}

inline bool
BaseString::operator!=(const char *str) const
{
    return strcmp(m_chr, str) != 0;
}

inline bool
operator!(const BaseString& str)
{
    return str.m_chr == NULL;
}

inline BaseString&
BaseString::assign(const BaseString& str)
{
    return assign(str.m_chr);
}

inline BaseString&
BaseString::assign(const Vector<BaseString> &vector,
		   const BaseString &separator) {
  assign("");
  return append(vector, separator);
}

/**
 * Return pointer and length for key to use when BaseString is
 * used as Key in HashMap
 */
const void * BaseString_get_key(const void* key, size_t* key_length);

#endif /* !__UTIL_BASESTRING_HPP_INCLUDED__ */
