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

#ifndef __UTIL_BASESTRING_HPP_INCLUDED__
#define __UTIL_BASESTRING_HPP_INCLUDED__

#include <ndb_global.h>
#include <Vector.hpp>

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

  /** @brief Convert to uppercase */
  void ndb_toupper();

  /** @brief Convert to lowercase */
  void ndb_tolower();

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
  BaseString& assfmt(const char* ftm, ...);

  /** @brief Appends a format string a la printf() to the end */
  BaseString& appfmt(const char* ftm, ...);

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
   * @returns index of character, of -1 if no character found
   */
  ssize_t indexOf(char c);

  /**
   * Returns the index of the last occurance of the character c.
   *
   * @params c character to look for
   * @returns index of character, of -1 if no character found
   */
  ssize_t lastIndexOf(char c);
  
  /**
   * Returns a subset of a string
   *
   * @param start index of first character
   * @param stop index of last character
   * @return a new string
   */
  BaseString substr(ssize_t start, ssize_t stop = -1);

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
  static int snprintf(char *str, size_t size, const char *format, ...);
  static int vsnprintf(char *str, size_t size, const char *format, va_list ap);
private:
  char* m_chr;
  unsigned m_len;
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
BaseString::ndb_toupper() {
  for(unsigned i = 0; i < length(); i++)
    m_chr[i] = toupper(m_chr[i]);
}

inline void
BaseString::ndb_tolower() {
  for(unsigned i = 0; i < length(); i++)
    m_chr[i] = tolower(m_chr[i]);
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

#endif /* !__UTIL_BASESTRING_HPP_INCLUDED__ */
