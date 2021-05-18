/*
   Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef string_helpers_hpp
#define string_helpers_hpp

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>

using std::string;
using std::wstring;
using std::ostream;
using std::istringstream;
using std::ostringstream;
using std::wistringstream;

/************************************************************
 * String Helper Functions
 ************************************************************/

/**
 * Returns a multi-byte representation of a wide-character string.
 * Flaws: no character set conversions, multiple copy operations.
 */
inline string
toS(const wstring& ws, const wstring& vdefault = L"") {
    //sprintf(charptr,"%ls",wsdtring.c_str()); 
    string val(ws.begin(), ws.end());
    if (val.length() == 0)
        val.append(vdefault.begin(), vdefault.end());
    return val;
}

/**
 * Returns true if the argument string is, ignoring case, "true",
 * the default value if the string is empty, or false, otherwise.
 */
inline bool
toB(const wstring& ws, bool vdefault = false) {
    // compile problems with manipulators
    //    bool r;
    //    wistringstream(ws) >> ios_base::boolalpha >> r;
    //    wistringstream(ws) >> setiosflags(ios_base::boolalpha) >> r;
    // worse, seems to return true for empty strings:
    //    wistringstream wss(ws);
    //    wss.flags(ios_base::boolalpha);
    //    wss >> r;
    // problems with transform & tolower(), yields empty string t:
    //    std::transform(ws.begin(), ws.end(), t.begin(), ::tolower);
    //    std::transform(s.begin(), s.end(), t.begin(),
    //                   static_cast< int (*)(int) >(std::tolower));
    //    return (t.compare("true") == 0);
    // so, short & direct:
    bool val;
    if (ws.length() == 0) {
        val = vdefault;
    } else {
        val = ((ws.length() == 4)
               && (ws[0] == L'T' || ws[0] == L't')
               && (ws[1] == L'R' || ws[1] == L'r')
               && (ws[2] == L'U' || ws[2] == L'u')
               && (ws[3] == L'E' || ws[3] == L'e'));
    }
    return val;
}

/**
 * Returns the integral value of a wide character string, the default value
 * if the string is empty, or the error value if the conversion has failed.
 */
template< typename I >
inline I
toI(const wstring& ws, I vdefault, I verror) {
    I val;
    if (ws.length() == 0) {
        val = vdefault;
    } else {
        wistringstream wiss(ws);
        wiss >> val;
        if (wiss.fail() || !wiss.eof()) {
            val = verror;
        }
    }
    return val;
}

/**
 * Returns a basic value as a string (e.g., for expressions).
 */
inline string
toString(bool i) {
    // easier than stream manipulators or setting formatting flags:
    //cout << ios_base::boolalpha << ...;
    //const ios_base::fmtflags f = cout.flags();
    //cout.flags(ios_base::boolalpha); cout << ...; cout.flags(f);
    return (i ? "true" : "false");
}

/**
 * Returns a basic value as a string (e.g., for expressions).
 */
inline string
toString(int i) {
    // if crashes (JNI) with gcc and operator<<(ostream &, long/int):
    //char s[256]; snprintf(s, 256, "%d", i); return string(s);
    ostringstream o;
    o << i;
    return o.str();
}

/**
 * Returns a basic value as a string (e.g., for expressions).
 */
inline string
toString(unsigned int i) {
    // if crashes (JNI) with gcc and operator<<(ostream &, long/int):
    //char s[256]; snprintf(s, 256, "%d", i); return string(s);
    ostringstream o;
    o << i;
    return o.str();
}

/**
 * Returns a basic value as a string (e.g., for expressions).
 */
inline string
toString(long i) {
    // if crashes (JNI) with gcc and operator<<(ostream &, long/int):
    //char s[256]; snprintf(s, 256, "%d", i); return string(s);
    ostringstream o;
    o << i;
    return o.str();
}

/**
 * Returns a basic value as a string (e.g., for expressions).
 */
inline string
toString(unsigned long i) {
    // if crashes (JNI) with gcc and operator<<(ostream &, long/int):
    //char s[256]; snprintf(s, 256, "%d", i); return string(s);
    ostringstream o;
    o << i;
    return o.str();
}

/**
 * Returns a string representation of container elements separated by blanks.
 */
template < typename InputIterator >
inline string
toString(InputIterator begin, InputIterator end) {
    ostringstream r;
    r << "[ ";
    typedef typename InputIterator::value_type T; // type of elements
    std::copy(begin, end, std::ostream_iterator< T >(r, " "));
    r << "]";
    return r.str();
}

/**
 * Returns a string representation of container elements separated by blanks.
 */
template < typename Container >
inline string
toString(Container c) {
    return toString(c.begin(), c.end());
}

/**
 * Splits a string by a delimiter character into tokens written to iterator;
 * the delimiter is discarded, empty tokens are preserved.
 */
template < typename OutputIterator >
inline void
split(const string &str, char delim, OutputIterator result) {
    istringstream is(str);
    string s;
    while (getline(is, s, delim))
        *result++ = s;
}

/**
 * Splits a string by a delimiter character into tokens written to iterator;
 * the delimiter is discarded, empty tokens are preserved.
 */
inline bool
startsWith(const string &str, const string &prefix) {
    return !str.compare(0, prefix.size(), prefix);
}

#endif // string_helpers_hpp
