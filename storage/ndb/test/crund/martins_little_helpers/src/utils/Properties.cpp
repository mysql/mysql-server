/*
 * Properties.cpp
 *
 */

#include "Properties.hpp"

#include <fstream>
#include <cassert>
#include <climits>

using std::istream;
using std::ostream;
using std::ifstream;
using std::ofstream;
using std::cout;
using std::wcout;
using std::endl;
using std::wstring;
using std::streambuf;
using std::stringbuf;

using utils::Properties;

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
Properties::writeAsciiEsc(streambuf& os, wchar_t c)
{
    assert (L'\x20' <= c && c <= L'\x7e');
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
        int n = os.sputc(static_cast<char>(c)); // explicit cast preferred
        assert (n != EOF); // XXX handle error
        (void)n;
        return;
    }
    int n = os.sputc('\x5c'); // '\\'
    assert (n != EOF); // XXX handle error
    n = os.sputc(d);
    assert (n != EOF); // XXX handle error
}

inline void
Properties::writeUnicodeEsc(streambuf& os, wchar_t c)
{
    assert (c < L'\x20' || L'\x7e' < c);

    // subsequent code depends upon a UCS-2 (UTF-16) or UTF-32
    // encoding of wide characters
    const int w = sizeof(wchar_t);
    assert (w == 2 || w == 4);
    assert (CHAR_BIT == 8);

    // write unicode escape sequence as "\\unnnn" or "\Unnnnnnnn"
    int n = os.sputc('\x5c'); // '\\'
    assert (n != EOF); // XXX handle error
    n = os.sputc(w == 2 ? '\x75' : '\x55'); // 'u' : 'U'
    assert (n != EOF); // XXX handle error
    static const char ascii[] = { '\x30', '\x31', '\x32', '\x33',
                                  '\x34', '\x35', '\x36', '\x37',
                                  '\x38', '\x39', '\x41', '\x42',
                                  '\x43', '\x44', '\x45', '\x46' }; // '0'..'F'
    for (unsigned int i = w * 8 - 4; i >= 0; i -= 4) {
        n = os.sputc(ascii[(c>>i) & 0xF]);
        assert (n != EOF); // XXX handle error
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
            int n = os.sputc('\x20'); // ' '  SPACE
            assert (n != EOF); // XXX handle error
            (void)n;
        } else if (isPrintableAscii(c)) {
            writeAsciiEsc(os, c);
        } else {
            writeUnicodeEsc(os, c);
        }
    }
}

//---------------------------------------------------------------------------

void
Properties::load(const char* filename)
{
    assert (filename);
    ifstream ifs;
    ifs.open(filename);
    assert (ifs.good()); // XXX handle error
    assert (!ifs.bad()); // XXX handle error
    load(ifs);
    ifs.close();
}

void
Properties::load(istream& is)
{
    streambuf* ib = is.rdbuf();
    assert (ib != NULL); // XXX handle error
    load(*ib);
}

void
Properties::load(streambuf& ib)
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

void
Properties::store(const char* filename, const wstring* header) const
{
    assert (filename);
    ofstream ofs;
    ofs.open(filename);
    assert (ofs.good()); // XXX handle error
    assert (!ofs.bad()); // XXX handle error
    store(ofs, header);
    ofs.close();
}

void
Properties::store(ostream& os, const wstring* header) const
{
    streambuf* ob = os.rdbuf();
    assert (ob != NULL); // XXX handle error
    store(*ob, header);
}

void
Properties::store(streambuf& os, const wstring* header) const
{
    // subsequent code for writing the header, keys and values
    // depends upon UCS-2 (UTF-16) or UTF-32 character encoding
    const int w = sizeof(wchar_t);
    assert (w == 2 || w == 4);
    assert (CHAR_BIT == 8);
    assert (L'!' == '\x21');
    assert (L'A' == '\x41');
    assert (L'a' == '\x61');
    assert (L'~' == '\x7e');
    (void)w;
    
    if (header != NULL) {
        int n = os.sputc('\x23'); // '#'
        assert (n != EOF); // XXX handle error
        writeKey(os, *header);
        n = os.sputc('\x0a'); // '\n'
        assert (n != EOF); // XXX handle error
    }

    for (const_iterator i = begin(); i != end(); ++i) {
        const wstring& key = i->first;
        const wstring& value = i->second;
        writeKey(os, key);
        int n = os.sputc('\x3d'); // '='
        assert (n != EOF); // XXX handle error
        writeValue(os, value);
        n = os.sputc('\x0a'); // '\n'
        assert (n != EOF); // XXX handle error
    }
}

// ---------------------------------------------------------------------------
