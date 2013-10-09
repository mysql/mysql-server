/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

/*
 * Properties.hpp
 *
 */

#ifndef Properties_hpp
#define Properties_hpp

#include <map>
#include <string>
#include <ios>
#include <iostream>
#include <istream>
#include <ostream>
#include <streambuf>
#include <fstream>
#include <cassert>
#include <climits>

namespace utils {
    
using std::map;
using std::wstring;
using std::ios_base;
using std::istream;
//using std::wistream;
using std::ostream;
//using std::wostream;
using std::streambuf;
//using std::wstreambuf;

/**
 * The Properties class is a specialized map container that stores
 * elements composed of a key string and a corresponding value string.
 *
 * This class offers load/store functions to read properties from or
 * save them to a std::streambuf, stream, or file.  The format for reading
 * and writing properties is the same used by Java Properties:
 *   http://java.sun.com/javase/6/docs/api/java/util/Properties.html
 *   http://java.sun.com/j2se/1.5.0/docs/api/java/util/Properties.html
 *   http://java.sun.com/j2se/1.4.2/docs/api/java/util/Properties.html
 *
 * This implementation supports non-Ascii characters but assumes:
 * ... XXX ...
 *
 * This class derives publicly from std::map.  Generally, this would
 * be problematic, since std::map does not seem to provide a virtual
 * destructor (at least, GNU's Standard C++ Library v3 does not).
 * In other words, without a virtual destructor, std::map does not
 * support derived types whose instances can also pass as std::maps.
 *
 * Under this limitation, the best approach would be for Properties
 * to derive privately (or protectedly) from std::map only, so that
 * Properties instances are not exposed to being deleted through a
 * std::map base pointer.  As a consequence, this requires to
 * publicly redeclare most of the std::map members (e.g.
 * "using std::map<std::wstring,std::wstring>::operator[];").
 *
 * However, class Properties is lucky, though, in that it does not
 * declare any data members and destructor, for it conceptually
 * posesses no state apart from the inherited std::map for the key,
 * value mapping.  It is therefore unproblematic if a Properties
 * instance is deleted through its base pointer.
 *
 * Nevertheless, any classes publicly derived from Properties would
 * have to reconsider the lack of a virtual destuctor in std::map.
 */
class Properties : public map<wstring, wstring>
{
public:
    /**
     * Reads properties from the character file and adds them to this
     * property table.
     */
    void load(const char* filename)
        throw (ios_base::failure);

    /**
     * Reads properties from the character input stream and adds them
     * to this property table.
     */
    void load(istream& is)
        throw (ios_base::failure);

    /**
     * Reads properties from the character buffer and adds them
     * to this property table.
     *
     * The line-oriented format is the same as used by Java Properties.
     * The byte stream is read under the ISO 8859-1 character encoding,
     * so, all non-ISO 8859-1 characters of the key and value strings
     * need to be expressed as an escape sequence.
     */
    void load(streambuf& ib)
        throw (ios_base::failure);

    /**
     * Reads properties from the wide character input stream and adds them
     * to this property table.
     */
    // not implemented yet
    //void load(wistream& is)
    //    throw (ios_base::failure);

    /**
     * Reads properties from the wide character buffer and adds them
     * to this property table.
     *
     * The byte stream is read under the UTF-32/UCS-4 character encoding,
     * and characters of the key and value strings are not parsed as an
     * escape sequence.
     */
    // not implemented yet
    //void load(wstreambuf& ib)
    //    throw (ios_base::failure);

    /**
     * Writes this property table to the character file.
     */
    void store(const char* filename, const wstring* header = NULL) const
        throw (ios_base::failure);

    /**
     * Writes this property table to the character output stream.
     */
    void store(ostream& os, const wstring* header = NULL) const
        throw (ios_base::failure);

    /**
     * Writes this property table to the character buffer.
     *
     * The format is suitable for reading the properties using the load
     * function.  The stream is written using the ISO 8859-1 character
     * encoding and characters of the key and value strings are examined
     * to see whether they should be rendered as an escape sequence.
     *
     * If the header argument is not null, then an ASCII # character, the
     * header string, and a line separator are first written to the output
     * stream. Thus, the header can serve as an identifying comment.
     */
    void store(streambuf& ob, const wstring* header = NULL) const
        throw (ios_base::failure);

    /**
     * Writes this property table to the wide character output stream.
     */
    // not implemented yet
    //void store(wostream& os, const wstring* header = NULL) const
    //    throw (ios_base::failure);

    /**
     * Writes this property table to the wide character buffer.
     *
     * The format is suitable for reading the properties using the load
     * function.  The stream is written using the UTF-32/UCS-4 character
     * encoding, and characters of the key and value strings are not
     * rendered as an escape sequence.
     *
     * If the header argument is not null, then an ASCII # character, the
     * header string, and a line separator are first written to the output
     * stream. Thus, the header can serve as an identifying comment.
     */
    // not implemented yet
    //void store(wstreambuf& ob, const wstring* header = NULL) const
    //    throw (ios_base::failure);

protected:
    static bool isWS(int c);
    static bool isNL(int c);
    static bool isComment(int c);
    static bool isAssign(int c);
    static bool isKeyTerminator(int c);
    static bool isEsc(int c);
    static void skipWS(streambuf& ib);
    static void skipComment(streambuf& ib);
    static void readIgnored(streambuf& ib);
    static void readEsc(wstring& s, streambuf& ib);
    static void readKey(wstring& s, streambuf& ib);
    static void readValue(wstring& s, streambuf& ib);
    static bool isPrintableAscii(wchar_t c);
    static void writeAsciiEsc(streambuf& os, wchar_t c);
    static void writeUnicodeEsc(streambuf& os, wchar_t c);
    static void writeKey(streambuf& os, const wstring& s);
    static void writeValue(streambuf& os, const wstring& s);
    static void writeChar(streambuf& os, char c);
};

inline istream&
operator>>(istream& s, Properties& p)
{
    p.load(s);
    return s;
}

/*
// not implemented yet
inline wistream&
operator>>(wistream& s, Properties& p)
{
    p.load(s);
    return s;
}
*/

inline ostream&
operator<<(ostream& s, const Properties& p)
{
    p.store(s);
    return s;
}

/*
// not implemented yet
inline wostream&
operator<<(wostream& s, const Properties& p)
{
    p.store(s);
    return s;
}
*/


// ---------------------------------------------------------------------------
// Properties Implementation
// ---------------------------------------------------------------------------

using std::cout;
using std::wcout;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::streambuf;
using std::stringbuf;

// ---------------------------------------------------------------------------

inline bool
Properties::isWS(int c)
{
    switch (c) {
    case 0x09: // '\t' HT
    case 0x0c: // '\f' FF
    case 0x20: // ' '  SPACE
        return true;
    }
    return false;
}

inline bool
Properties::isNL(int c)
{
    switch (c) {
    case 0x0a: // '\n' LF
    case 0x0d: // '\r' CR
        return true;
    }
    return false;
}

inline bool
Properties::isComment(int c)
{
    switch (c) {
    case 0x21: // '!'
    case 0x23: // '#'
        return true;
    }
    return false;
}

inline bool
Properties::isAssign(int c)
{
    switch (c) {
    case 0x3a: // ':'
    case 0x3d: // '='
        return true;
    }
    return false;
}

inline bool
Properties::isKeyTerminator(int c)
{
    return isWS(c) || isAssign(c) || isNL(c);
}

inline bool
Properties::isEsc(int c)
{
    switch (c) {
    case 0x5c: // '\\'
        return true;
    }
    return false;
}

inline void
Properties::skipWS(streambuf& ib)
{
    int c;
    while ((c = ib.snextc()) != EOF && isWS(c));
}

inline void
Properties::skipComment(streambuf& ib)
{
    int c;
    // comments cannot have escaped line terminators
    while ((c = ib.snextc()) != EOF && !isNL(c));
    ib.sbumpc();
}

inline void
Properties::readIgnored(streambuf& ib)
{
    int c;
    while ((c = ib.sgetc()) != EOF) {
        if (isWS(c)) {
            skipWS(ib);
            c = ib.sgetc();
        }
        if (isNL(c)) {
            ib.sbumpc();
            continue;
        }
        if (isComment(c)) {
            ib.sbumpc();
            skipComment(ib);
            continue;
        }
        return;
    }
}

inline void
Properties::readEsc(wstring& s, streambuf& ib)
{
    int c = ib.sgetc();
    switch (c) {
    case EOF:
        return;
    case 0x0a: // '\n' LF
    case 0x0d: // '\r' CR
        // escaped EOL (CR, LF, CRLF)
        if ((c = ib.snextc()) != 0x0a) // '\n' LF
            ib.sungetc();
        skipWS(ib);
        return;
    case 0x6e: // 'n'
        // LF ("newline") char escape
        c = 0x0a; // '\n'
        break;
    case 0x72: // 'r'
        // CR ("return") char escape
        c = 0x0d; // '\r'
        break;
    case 0x75: { // 'u'
        // unicode escape
        c = ib.sbumpc();
        // store input characters in case not an escape sequence
        wstring raw;
        raw += c; // silently drop backslash by general rule
        unsigned int d = 0;
        for (int i = 0; i < 4; i++) {
            d <<= 4;
            c = ib.sbumpc();
            raw += static_cast<wchar_t>(c); // exlicit cast preferred
            switch (c) {
            case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
            case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
                // '0'..'9'
                d += c - 0x30; // - '0'
                break;
            case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46:
                // 'A'..'F'
                d += 10 + c - 0x41; // - 'A'
                break;
            case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66:
                // 'a'..'f'
                d += 10 + c - 0x61; // - 'a'
                break;
            case EOF:
            default:
                // not a unicode escape sequence, write the raw char sequence
                s += static_cast<wchar_t>(c); // exlicit cast preferred
                return;
            }
        }
        s += static_cast<wchar_t>(d); // exlicit cast preferred
        return;
    }
    default:
        // unrecognized escape no error, silently drop preceding backslash
        break;
    }
    s += static_cast<wchar_t>(c); // exlicit cast preferred
    ib.sbumpc();
}

inline void
Properties::readKey(wstring& s, streambuf& ib)
{
    int c;
    while ((c = ib.sgetc()) != EOF) {
        if (isKeyTerminator(c)) {
            if (isNL(c)) {
                return;
            }
            if (isWS(c)) {
                skipWS(ib);
                c = ib.sgetc();
            }
            if (isAssign(c)) {
                skipWS(ib);
            }
            return;
        }

        ib.sbumpc();
        if (isEsc(c)) {
            readEsc(s, ib);
        } else {
            s += static_cast<wchar_t>(c); // exlicit cast preferred
        }
    }
}

inline void
Properties::readValue(wstring& s, streambuf& ib)
{
    int c;
    while ((c = ib.sgetc()) != EOF) {
        ib.sbumpc();
        if (isNL(c)) {
            return;
        }

        if (isEsc(c)) {
            readEsc(s, ib);
        } else {
            s += static_cast<wchar_t>(c); // exlicit cast preferred
        }
    }
}

inline bool
Properties::isPrintableAscii(wchar_t c)
{
    return (L'\x20' <= c && c <= L'\x7e');
}

inline void
Properties::writeChar(streambuf& os, char c)
{
    int n = os.sputc(c);
    if (n == EOF)
        throw ios_base::failure("Error writing to streambuf");
}

inline void
Properties::writeAsciiEsc(streambuf& os, wchar_t c)
{
    assert(L'\x20' <= c && c <= L'\x7e');
    char d;
    switch (c) {
    case L'\t':     // HT
        d = '\x74'; // 't'
        break;
    case L'\n':    // LF
        d = '\x6e'; // 'n'
        break;
    case L'\f':    // FF
        d = '\x66'; // 'f'
        break;
    case L'\r':    // CR
        d = '\x72'; // 'r'
        break;
    case L' ':     // SPACE
    case L'!':
    case L'#':
    case L':':
    case L'=':
    case L'\\':
        d = static_cast<char>(c); // exlicit cast preferred
        break;
    default:
        // write the raw character
        writeChar(os, static_cast<char>(c)); // explicit cast preferred
        return;
    }
    writeChar(os, '\x5c'); // '\\'
    writeChar(os, d);
}

inline void
Properties::writeUnicodeEsc(streambuf& os, wchar_t c)
{
    assert(c < L'\x20' || L'\x7e' < c);

    // subsequent code depends upon a UCS-2 (UTF-16) or UTF-32
    // encoding of wide characters
    const int w = sizeof(wchar_t);
    assert(w == 2 || w == 4);
    assert(CHAR_BIT == 8);

    // write unicode escape sequence as "\\unnnn" or "\Unnnnnnnn"
    writeChar(os, '\x5c'); // '\\'
    writeChar(os, w == 2 ? '\x75' : '\x55'); // 'u' : 'U'
    static const char ascii[] = { '\x30', '\x31', '\x32', '\x33',
                                  '\x34', '\x35', '\x36', '\x37',
                                  '\x38', '\x39', '\x41', '\x42',
                                  '\x43', '\x44', '\x45', '\x46' }; // '0'..'F'
    for (unsigned int i = w * 8 - 4; i >= 0; i -= 4) {
        writeChar(os, ascii[(c>>i) & 0xF]);
    }
}

inline void
Properties::writeKey(streambuf& os, const wstring& s)
{
    for (wstring::const_iterator i = s.begin(); i != s.end(); ++i) {
        const wchar_t c = *i;
        if (isPrintableAscii(c))
            writeAsciiEsc(os, c);
        else
            writeUnicodeEsc(os, c);
    }
}

inline void
Properties::writeValue(streambuf& os, const wstring& s)
{
    wstring::const_iterator i = s.begin();
    for (; i != s.end() && *i == L'\x20'; ++i) { // L' '  SPACE
        // write leading spaces escaped
        writeAsciiEsc(os, *i);
    }
    for (; i != s.end(); ++i) {
        const wchar_t c = *i;
        if (c == L'\x20') { // L' '  SPACE
            // write embedded or tailing spaces unescaped
            writeChar(os, '\x20'); // ' '  SPACE
        } else if (isPrintableAscii(c)) {
            writeAsciiEsc(os, c);
        } else {
            writeUnicodeEsc(os, c);
        }
    }
}

// ---------------------------------------------------------------------------

inline void
Properties::load(const char* filename)
    throw (ios_base::failure)
{
    assert(filename);
    ifstream ifs;
    ifs.exceptions(ifstream::failbit | ifstream::badbit);
    ifs.open(filename);
    assert(!ifs.bad()); // thrown ios_base::failure
    load(ifs);
    ifs.close();
}

inline void
Properties::load(istream& is)
    throw (ios_base::failure)
{
    istream::iostate exceptions = is.exceptions(); // backup
    is.exceptions(istream::failbit | istream::badbit);
    streambuf* ib = is.rdbuf();
    assert(ib != NULL); // thrown ios_base::failure
    load(*ib);
    is.exceptions(exceptions); // restore
}

inline void
Properties::load(streambuf& ib)
    throw (ios_base::failure)
{
    while (ib.sgetc() != EOF) {
        readIgnored(ib);
        if (ib.sgetc() == EOF)
            return;

        // parse property
        wstring k;
        readKey(k, ib);
        wstring v;
        readValue(v, ib);
        //wcout << "('" << k << "', '" << v << "')" << endl;
        (*this)[k] = v; // YYY
        //this->operator[](k) = v;
    }
}

inline void
Properties::store(const char* filename, const wstring* header) const
    throw (ios_base::failure)
{
    assert(filename);
    ofstream ofs;
    ofs.exceptions(ifstream::failbit | ifstream::badbit);
    ofs.open(filename);
    assert(!ofs.bad());
    store(ofs, header);
    ofs.close();
}

inline void
Properties::store(ostream& os, const wstring* header) const
    throw (ios_base::failure)
{
    ostream::iostate exceptions = os.exceptions(); // backup
    os.exceptions(istream::failbit | istream::badbit);
    streambuf* ob = os.rdbuf();
    assert(ob != NULL); // thrown ios_base::failure
    store(*ob, header);
    os.exceptions(exceptions); // restore
}

inline void
Properties::store(streambuf& os, const wstring* header) const
    throw (ios_base::failure)
{
    // subsequent code for writing the header, keys and values
    // depends upon UCS-2 (UTF-16) or UTF-32 character encoding
    const int w = sizeof(wchar_t);
    assert(w == 2 || w == 4);
    assert(CHAR_BIT == 8);
    assert(L'!' == '\x21');
    assert(L'A' == '\x41');
    assert(L'a' == '\x61');
    assert(L'~' == '\x7e');
    (void)w;
    
    if (header != NULL) {
        writeChar(os, '\x23'); // '#'
        writeKey(os, *header);
        writeChar(os, '\x0a'); // '\n'
    }

    for (const_iterator i = begin(); i != end(); ++i) {
        const wstring& key = i->first;
        const wstring& value = i->second;
        writeKey(os, key);
        writeChar(os, '\x3d'); // '='
        writeValue(os, value);
        writeChar(os, '\x0a'); // '\n'
    }
}

// ---------------------------------------------------------------------------

} // utils

#endif // Properties_hpp
