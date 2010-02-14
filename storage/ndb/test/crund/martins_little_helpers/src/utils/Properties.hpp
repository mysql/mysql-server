/*
 * Properties.hpp
 *
 */

#ifndef Properties_hpp
#define Properties_hpp

#include <map>
#include <string>
#include <iostream>

namespace utils {
    
using std::map;
using std::wstring;
using std::istream;
using std::streambuf;
using std::ostream;

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
     * Reads properties from the file and adds them to this
     * property table.
     */
    void load(const char* filename);

    /**
     * Reads properties from the input stream and adds them to this
     * property table.
     */
    void load(istream& is);

    /**
     * Reads properties from the input byte buffer and adds them to this
     * property table.
     *
     * The line-oriented format is the same as used by Java Properties.
     * The byte stream is read under the ISO 8859-1 character encoding,
     * so, all non-ISO 8859-1 characters of the key and value strings
     * need to be expressed as an escape sequence.
     */
    void load(streambuf& ib);

    /**
     * Writes this property table to the file.
     */
    void store(const char* filename, const wstring* header = NULL) const;

    /**
     * Writes this property table to the output stream.
     */
    void store(ostream& os, const wstring* header = NULL) const;

    /**
     * Writes this property table to the output byte buffer.
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
    void store(streambuf& ob, const wstring* header = NULL) const;

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
};

inline istream&
operator>>(istream& s, Properties& p)
{
    p.load(s);
    return s;
}

inline ostream&
operator<<(ostream& s, const Properties& p)
{
    p.store(s);
    return s;
}

} // utils

#endif // Properties_hpp
