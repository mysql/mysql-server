// $Id$
// Copyright (c) 2000-2016 the Regents of the University of California
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
//
// implementation of the utility functions defined in util.h (namespace
// ibis::util)
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#include <direct.h>     // _rmdir
#endif
#if defined(__unix__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE) || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__)
#include <unistd.h>     // getuid, rmdir, sysconf, popen, pclose
#include <sys/stat.h>   // stat
#include <dirent.h>     // DIR, opendir, readdir
#endif

#include "util.h"
#include "horometer.h"
#include "resource.h"
#include <stdarg.h>     // vsprintf

#include <set>          // std::set
#include <limits>       // std::numeric_limits
#include <locale>       // std::numpunct<char>
#include <iostream>     // std::cout
#if  (defined(HAVE_GETPWUID) || defined(HAVE_GETPWUID_R)) && !(defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__))
#include <pwd.h>        // getpwuid
#endif

#define FASTBIT_SYNC_WRITE 1

// global variables
#if defined(DEBUG)
int FASTBIT_CXX_DLLSPEC ibis::gVerbose = 10;
#else
int FASTBIT_CXX_DLLSPEC ibis::gVerbose = 0;
#endif
// define the default log file to use: stderr or stdout
#ifndef FASTBIT_DEFAULT_LOG
#ifdef FASTBIT_LOG_TO_STDERR
#define FASTBIT_DEFAULT_LOG stderr
#elif FASTBIT_LOG_TO_STDOUT
#define FASTBIT_DEFAULT_LOG stdout
#else
#define FASTBIT_DEFAULT_LOG stderr
#endif
#endif

// initialize the global variables of ibis::util
pthread_mutex_t ibis::util::envLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ibis::util::ioLock::mutex = PTHREAD_MUTEX_INITIALIZER;
/// A list of 65 printable ASCII characters that are not special to most of
/// the command interpreters.  The first 64 of them are basically the same
/// as specified in RFC 3548 for base-64 numbers, but appear in different
/// order.  A set of numbers represented in this base-64 representation
/// will be sorted in the same order as their decimal representations.
const char* ibis::util::charTable =
"-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~";
/// Maps back from ASCII to positions of the characters in
/// ibis::util::charTable.
const short unsigned ibis::util::charIndex[] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64,  0, 64, 64,
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 64, 64, 64, 63, 64, 64,
    64, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 64, 64, 64, 64, 37,
    64, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 64, 64, 64, 64,
};
// delimiters that can be used to separate names in a name list
const char* ibis::util::delimiters = ";, \v\b\f\r\t\n'\"";

// log base 2 of an integer, the lookup table
const int ibis::util::log2table[256] = {
#define X(i) i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    X(4), X(5), X(5), X(6), X(6), X(6), X(6),
    X(7), X(7), X(7), X(7), X(7), X(7), X(7), X(7)
#undef X
};

/// The global variable holding all the data partitions.
ibis::partList ibis::datasets;

// file scope variables
static std::string ibis_util_logfilename("");
static FILE* ibis_util_logfilepointer = 0;

/// Return a null-terminated string from the beginning of input string str.
/// The first apparence of any character from characters in tok_chars is
/// turned into null.  The incoming argument is modified to point to the
/// first character that is not in tok_chrs.  If no character in tok_chrs
/// is found, str is returned and the first argument is changed to null.
const char* ibis::util::getToken(char*& str, const char* tok_chrs) {
    const char* token = static_cast<const char*>(0);
    if (!str || !*str) return token;
    token = str;
    char* pc = strpbrk(str, tok_chrs);
    if (pc > str) {
        str = pc + strspn(pc, tok_chrs);
        *pc = static_cast<char>(0);
    }
    else {
        str = static_cast<char*>(0);
    }
    return token;
}

/// Recursivly create directory "dir".  Returns zero (0) to indicate
/// success, a negative number to indicate error.  If the directory already
/// exists, it immediately returns 0.
int ibis::util::makeDir(const char* dir) {
    Stat_T st;
    if (dir == 0 || *dir == 0) return -1;
    if (UnixStat(dir, &st) == 0) return 0; // directory exists already

    // make a copy of the directory name so we can turn directory
    // separators into null.
    char *buf = ibis::util::strnewdup(dir);
    const char *cdir = buf;
    if (*buf == FASTBIT_DIRSEP)
        cdir = buf + 1;
    else if (buf[1] == ':') // not the same as a typical local directory
        cdir = buf + 2;
    while (*cdir == FASTBIT_DIRSEP) ++cdir; // look beyond the leading DIRSEP

    while (cdir != 0 && *cdir != 0) {
        char* tmp = const_cast<char*>(strchr(cdir, FASTBIT_DIRSEP));
        if (tmp > cdir)
            *tmp = 0; // change FASTBIT_DIRSEP to null
        if (UnixStat(buf, &st) != 0) {
            int pmode = OPEN_FILEMODE;
#if defined(S_IXUSR)
            pmode = pmode | S_IXUSR;
#endif
#if defined(S_IRWXG) && defined(S_IRWXO)
            pmode = pmode | S_IRWXG | S_IRWXO;
#elif defined(S_IXGRP) && defined(S_IXOTH)
            pmode = pmode | S_IXGRP | S_IXOTH;
#endif
            if (mkdir(buf, pmode) == -1) {
                if (errno != EEXIST) { // check errno produced by mkdir
                    ibis::util::logMessage("Warning", "makeDir failed to "
                                           "create directory \"%s\"", buf);
                    delete [] buf;
                    return -2;
                }
            }
        }
        if (tmp > cdir) {
            *tmp = FASTBIT_DIRSEP; // change null back to DIRSEP
            cdir = tmp + 1;
            while (*cdir == FASTBIT_DIRSEP) ++cdir; // skip consecutive DIRSEP
        }
        else {
            cdir = static_cast<const char*>(0);
        }
    }
    delete [] buf;
    return 0;
} // ibis::util::makeDir

/// Extract a string from the given buf.  Remove leading and trailing spaces
/// and surrounding quotes.  Returns a copy of the string allocated with
/// the @c new operator.
char* ibis::util::getString(const char* buf) {
    char* s2 = 0;
    if (buf == 0)
        return s2;
    if (buf[0] == 0)
        return s2;

    // skip leading space
    const char* s1 = buf;
    while (*s1 && isspace(*s1))
        ++ s1;
    if (*s1 == 0)
        return s2;

    if (*s1 == '\'') { // quoted by a single quote
        const char* tmp = 0;
        ++ s1;
        do { // skip escaped quotation
            tmp = strchr(s1, '\'');
        } while (tmp > s1 && tmp[-1] == '\\');
        if (tmp > s1) { // copy till the matching quote
            const uint32_t len = tmp - s1;
            s2 = new char[len+1];
            strncpy(s2, s1, len);
            s2[len] = 0;
        }
        else if (*s1) { // no matching quote, copy all characters
            s2 = ibis::util::strnewdup(s1);
        }
    }
    else if (*s1 == '"') { // quoted by a double quote
        const char* tmp = 0;
        ++ s1;
        do { // skip escaped quotation
            tmp = strchr(s1, '"');
        } while (tmp > s1 && tmp[-1] == '\\');
        if (tmp > s1) { // copy till the matching quote
            const uint32_t len = tmp - s1;
            s2 = new char[len+1];
            strncpy(s2, s1, len);
            s2[len] = 0;
        }
        else if (*s1) { // no matching quote, copy all characters
            s2 = ibis::util::strnewdup(s1);
        }
    }
    else { // not quoted, copy all characters
        const char *tmp = s1 + std::strlen(s1) - 1;
        while (tmp>s1 && isspace(*tmp))
            -- tmp;
        s2 = ibis::util::strnewdup(s1, tmp-s1+1);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 0)
        << "DEBUG -- util::getString(" << buf << ") retrieved \"" << s2 << "\"";
#endif
    return s2;
} // ibis::util::getString

/// Copy the next string to the output variable str.  Leading blank spaces
/// are skipped.  The content of str will be empty if buf is nil or an
/// empty string.  If the string is quoted, only spaces before the quote
/// are skipped, and the content of the string will be everything after the
/// first quote to the last character before the matching quote or end of
/// buffer.  If delim is not provided (i.e., is 0) and the 1st nonblank
/// character is not a quote, then string will terminate at the 1st space
/// (or nonprintable) character following the nonblank character.
///
/// @note A unquoted empty string is considered a null value which is
/// indicated by a negative return value.  A quoted empty string is a valid
/// string, which is indicated by a return value of 0.
///
/// @note This function uses backslash as the escape character for allowing
/// quotes to be inside the quoted strings.  Unfortunately, this also means
/// to have a single backslash in a string, one has to input two of them
/// right next to each other.
///
/// @note Input strings starting with an apostrophe, such as "'twas", must
/// be quoted as "\"'twas\"" or the apostrophe must be escaped as "\'twas".
/// Otherwise, the leading apostrophe would be interpreted as a unmatched
/// quote which will cause the next string value to extend beyond the
/// intended word.
int ibis::util::readString(std::string& str, const char *&buf,
                           const char *delim) {
    str.erase(); // erase the existing content
    while (*buf && isspace(*buf) != 0) ++ buf; // skip leading space
    if (buf == 0 || *buf == 0) return -3;

    if (*buf == '\'') { // single quoted string
        ++ buf; // skip the openning quote
        while (*buf) {
            if (*buf != '\'')
                str += *buf;
            else if (str.size() > 0 && str[str.size()-1] == '\\')
                str[str.size()-1] = '\'';
            else {
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf)
                            ++ buf;
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf))
                            ++ buf;
                    }
                }
                return 0;
            }
            ++ buf;
        } // while (*buf)
    }
    else if (*buf == '"') { // double quoted string
        ++ buf; // skip the openning quote
        while (*buf) {
            if (*buf != '"')
                str += *buf;
            else if (str.size() > 0 && str[str.size()-1] == '\\')
                str[str.size()-1] = '"';
            else {
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf)
                            ++ buf;
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf))
                            ++ buf;
                    }
                }
                return 0;
            }
            ++ buf;
        } // while (*buf)
    }
    else if (*buf == '`') { // left quote
        ++ buf; // skip the openning quote
        while (*buf) {
            if (*buf != '`' && *buf != '\'')
                str += *buf;
            else if (str.size() > 0 && str[str.size()-1] == '\\')
                str[str.size()-1] = '`';
            else {
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf)
                            ++ buf;
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf))
                            ++ buf;
                    }
                }
                return 0;
            }
            ++ buf;
        } // while (*buf)
    }
    else { // delimiter separated string
        const char *start = buf;
        if (delim == 0 || *delim == 0) {
            // assume space as delimiter, all non-printable characters are
            // treated as spaces (i.e., as delimiters)
            while (*buf) {
                if (isgraph(*buf)) { // printable and nonspace
                    str += *buf;
                }
                else if (str.size() > 0 && str[str.size()-1] == '\\')
                    str[str.size()-1] = *buf;
                else {
                    ++ buf;
                    return 0;
                }
                ++ buf;
            } // while (*buf)
        }
        else if (delim[1] == 0) { // single character delimiter
            while (*buf) {
                if (*delim != *buf)
                    str += *buf;
                else if (str.size() > 0 && str[str.size()-1] == '\\')
                    str[str.size()-1] = *buf;
                else {
                    int ierr = - (buf <= start);
                    ++ buf;
                    return ierr;
                }
                ++ buf;
            } // while (*buf)
        }
        else if (delim[2] == 0) { // two delimiters
            while (*buf) {
                if (*delim != *buf && delim[1] != *buf)
                    str += *buf;
                else if (str.size() > 0 && str[str.size()-1] == '\\')
                    str[str.size()-1] = *buf;
                else {
                    int ierr = - (buf <= start);
                    ++ buf;
                    return ierr;
                }
                ++ buf;
            } // while (*buf)
        }
        else { // long list of delimiters
            while (*buf) {
                if (0 == strchr(delim, *buf))
                    str += *buf;
                else if (str.size() > 0 && str[str.size()-1] == '\\')
                    str[str.size()-1] = *buf;
                else {
                    int ierr = - (buf <= start);
                    ++ buf;
                    return ierr;
                }
                ++ buf;
            } // while (*buf)
        }

        if (str.size() > 1 && isspace(str[str.size()-1]) != 0) {
            // remove the trailing spaces
            size_t end = str.size()-2;
            while (end > 0 && isspace(str[end]) != 0)
                -- end;
            str.erase(end);
        }
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 0)
        << "DEBUG -- util::getString(" << buf << ") retrieved \""
        << str << "\"";
#endif
    // end of string
    return -2 * (str.empty());
} // ibis::util::readString

/// Attempt to extract a string token from the incoming buffer.  It uses
/// the existing buffer without allocating additional space.  It creates
/// nil terminated strings by converting terminators or space characters
/// into nil characters.  If it finds a valid string, it will return a
/// pointer to the string, otherwise it returns a nil pointer or an empty
/// string.
///
/// - If no delimiter is specified, it turns all non-printable characters
/// into the null character and returns the starting positions of groups of
/// alphanumeric characters as tokens.
///
/// - If a list of delimiters are provided, any of the delimiters will
/// terminate a token.  Blank spaces surrounding the delimiters will be
/// turned into null characters along with the delimiters.
///
/// @note If a token starts with a quote (any of signle quote, double
/// quote, or back quote), the token will end with the matching quote.
/// Note that the back quote can be ended with either another back quote or
/// a single quote.
///
/// @note All non-printable characters are treated as blank spaces.
const char* ibis::util::readString(char *&buf, const char *delim) {
    const char* str = 0;
    while (*buf && isspace(*buf) != 0) ++ buf; // skip leading space
    if (buf == 0 || *buf == 0) return str;

    if (*buf == '\'') { // single quoted string
        ++ buf; // skip the openning quote
        str = buf;
        while (*buf) {
            if (*buf != '\'')
                ++ buf;
            else if (buf > str && buf[-1] == '\\')
                ++ buf;
            else {
                *buf = 0;
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf) {
                            *buf = 0;
                            ++ buf;
                        }
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf)) {
                            *buf = 0;
                            ++ buf;
                        }
                    }
                }
                return str;
            }
        } // while (*buf)
    }
    else if (*buf == '"') { // double quoted string
        ++ buf; // skip the openning quote
        str = buf;
        while (*buf) {
            if (*buf != '"')
                ++ buf;
            else if (buf > str && buf[-1] == '\\')
                ++ buf;
            else {
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf) {
                            * buf = 0;
                            ++ buf;
                        }
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf)) {
                            * buf = 0;
                            ++ buf;
                        }
                    }
                }
                break;
            }
        } // while (*buf)
    }
    else if (*buf == '`') { // left quote
        ++ buf; // skip the openning quote
        str = buf;
        while (*buf) {
            if (*buf != '`' && *buf != '\'')
                ++ buf;
            else if (buf > str && buf[-1] == '\\')
                ++ buf;
            else {
                ++ buf;
                if (*buf != 0) {
                    if (delim == 0 || *delim == 0) {
                        // nothing to do
                    }
                    else if (delim[1] == 0) { // skip delimiter
                        if (delim[0] == *buf) {
                            *buf = 0;
                            ++ buf;
                        }
                    }
                    else { // skip delimiter
                        if (0 != strchr(delim, *buf)) {
                            *buf = 0;
                            ++ buf;
                        }
                    }
                }
                return str;
            }
        } // while (*buf)
    }
    else { // delimiter separated string
        str = buf;
        if (delim == 0 || *delim == 0) {
            // assume space as delimiter, all non-printable characters are
            // treated as spaces (i.e., as delimiters)
            while (*buf) {
                if (isgraph(*buf)) { // printable and nonspace
                    ++ buf;
                }
                else {
                    * buf = 0;
                    ++ buf;
                    break;
                }
            } // while (*buf)
        }
        else if (delim[1] == 0) { // single character delimiter
            while (*buf) {
                if (*delim != *buf)
                    ++ buf;
                else if (buf > str && buf[-1] == '\\')
                    ++ buf;
                else {
                    *buf = 0;
                    ++ buf;
                    break;
                }
            } // while (*buf)
        }
        else if (delim[2] == 0) { // two delimiters
            while (*buf) {
                if (*delim != *buf && delim[1] != *buf)
                    ++ buf;
                else if (buf > str && buf[-1] == '\\')
                    ++ buf;
                else {
                    *buf = 0;
                    ++ buf;
                    break;
                }
            } // while (*buf)
        }
        else { // long list of delimiters
            while (*buf) {
                if (0 == strchr(delim, *buf))
                    ++ buf;
                else if (buf > str && buf[-1] == '\\')
                    ++ buf;
                else {
                    *buf = 0;
                    ++ buf;
                    break;
                }
            } // while (*buf)
        }

        // to remove the trailing spaces
        char *back = (*buf ? buf - 2 : buf - 1);
        if (back > str && isspace(*back) != 0) {
            do {
                *back = 0;
                -- back;
            } while (back > str && isspace(*back) != 0);
        }
    }

#if DEBUG+0 > 0 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 0)
        << "DEBUG -- util::getString retrieved \""
        << str << "\", remaining .. " << buf;
#endif
    // end of string
    return str;
} // ibis::util::readString

/// Attempt to convert the incoming string into an integer.  It skips
/// leading space and converts an optional +/- sign followed by a list of
/// decimal digits to an integer.
///
/// On successful completion of this function, the argument val contains
/// the converted value and str points to the next unused character in the
/// input string.  In this case, it returns 0.  It returns -1 if the input
/// string is a null string, is an empty string, has only blank spaces, or
/// has a delimiter immediately following the leading blank spaces.  It
/// returns -2 to indicate overflow, in which case, it resets val to 0 and
/// moves str to the next character that is not a decimal digit.
int ibis::util::readInt(int64_t& val, const char *&str, const char* del) {
    int64_t tmp = 0;
    val = 0;
    if (str == 0 || *str == 0) return -1;
    for (; isspace(*str); ++ str); // skip leading space
    if (*str == 0)
        return -1;
    if (del != 0 && *del != 0 && strchr(del, *str) != 0) {
        ++ str;
        return -2;
    }

    if (*str == '0' && (str[1] == 'x' || str[1] == 'X')) { // hexadecimal
        return readUInt(reinterpret_cast<uint64_t&>(val), str, del);
    }

    const bool neg = (*str == '-');
    if (*str == '-' || *str == '+') ++ str;
    while (*str != 0 && isdigit(*str) != 0) {
        tmp = 10 * val + (*str - '0');
        if (tmp > val) {
            val = tmp;
        }
        else if (val > 0) { // overflow
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- util::readInt encounters an overflow: adding "
                << *str << " to " << val << " causes it to become " << tmp
                << ", reset val to 0";
            val = 0;
            while (*str != 0 && isdigit(*str) != 0) ++ str;
            return -3;
        }
        ++ str;
    }
    // skip trail modifier U
    if (*str == 'u' || *str == 'U') ++ str;
    // skip up to two 'L'
    if (*str == 'l' || *str == 'L') {
        ++ str;
        if (*str == 'l' || *str == 'L') ++ str;
    }
    if (neg) val = -val;
    if (*str != 0) {
        if (del == 0 || *del == 0) {
            // nothing to do
        }
        else if (del[1] == 0) { // skip delimiter
            if (del[0] == *str)
                ++ str;
        }
        else { // skip delimiter
            if (0 != strchr(del, *str))
                ++ str;
        }
    }
    return 0;
} // ibis::util::readInt

/// Attempt to convert the incoming string into a unsigned integer.  It
/// skips leading space and converts a list of decimal digits to an
/// integer.  On successful completion of this function, the argument val
/// contains the converted value and str points to the next unused
/// character in the input string.  In this case, it returns 0.  It returns
/// -1 if the input string is a null string, is an empty string, has only
/// blank spaces or have one of the delimiters immediately following the
/// leading blank spaces.  It returns -2 to indicate overflow, in which
/// case, it resets val to 0 and moves str to the next character that is
/// not a decimal digit.
int ibis::util::readUInt(uint64_t& val, const char *&str, const char* del) {
    uint64_t tmp = 0;
    val = 0;
    if (str == 0 || *str == 0) return -1;
    for (; isspace(*str); ++ str); // skip leading space
    if (*str == 0)
        return -1;
    if (del != 0 && *del != 0 && strchr(del, *str) != 0) {
        ++ str;
        return -2;
    }

    if (*str == '0' && (str[1] == 'x' || str[1] == 'X')) { // hexadecimal
        while ((*str >= '0' && *str <= '9') ||
               (*str >= 'A' && *str <= 'F') ||
               (*str >= 'a' && *str < 'f')) {
            tmp <<= 4; // shift 4 bits
            if (*str >= '0' && *str <= '9') {
                tmp += (*str - '0');
            }
            else if (*str >= 'A' && *str <= 'F') {
                tmp += (*str - 'A' + 10);
            }
            else {
                tmp += (*str - 'a' + 10);
            }
            if (tmp > val) { // accept the new value
                val = tmp;
            }
            else if (val > 0) { // overflow
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- readUInt encounters an overflow: adding "
                    << *str << " to " << val << " causes it to become " << tmp
                    << ", reset val to 0";
                val = 0;
                while (*str != 0 && isdigit(*str) != 0) ++ str;
                return -3;
            }
            ++ str;
        }
    }
    else {
        while (*str != 0 && isdigit(*str) != 0) {
            tmp = 10 * val + (*str - '0');
            if (tmp > val) {
                val = tmp;
            }
            else if (val > 0) { // overflow
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- readUInt encounters an overflow: adding "
                    << *str << " to " << val << " causes it to become " << tmp
                    << ", reset val to 0";
                val = 0;
                while (*str != 0 && isdigit(*str) != 0) ++ str;
                return -4;
            }
            ++ str;
        }
    }
    // skip trail modifier U
    if (*str == 'u' || *str == 'U') ++ str;
    // skip up to two 'L'
    if (*str == 'l' || *str == 'L') {
        ++ str;
        if (*str == 'l' || *str == 'L') ++ str;
    }
    if (*str != 0) {
        if (del == 0 || *del == 0) {
            // nothing to do
        }
        else if (del[1] == 0) { // skip delimiter
            if (del[0] == *str)
                ++ str;
        }
        else { // skip delimiter
            if (0 != strchr(del, *str))
                ++ str;
        }
    }
    return 0;
} // ibis::util::readUInt

/// Attempt to convert the incoming string into a double.  The format
/// recodnized is the following
///@code
/// [+-]?\d*\.\d*[[eE][+-]?\d+]
///@endcode
///
/// @note the decimal point is the period (.)!  This function does not
/// understand any other notation.
///
/// @note This function leaves str at the first character that does not
/// follow the above pattern.
int ibis::util::readDouble(double& val, const char *&str, const char* del) {
    val = 0;
    if (str == 0 || *str == 0) return -1;
    for (; isspace(*str); ++ str); // skip leading space
    if (*str == 0)
        return -1;
    if (del != 0 && *del != 0 && strchr(del, *str) != 0) {
        ++ str;
        return -2;
    }
    const size_t slen = strlen(str);
    if (slen > 3 && str[3] == 0 &&
        ((str[0]=='N' || str[0]=='n') &&
         (str[1]=='A' || str[1]=='a') &&
         (str[2]=='N' || str[2]=='n'))) {
        val = std::numeric_limits<double>::quiet_NaN();
        str += 4;
        return 0;
    }
    else if (slen > 7 && str[7] == 0 &&
             ((str[0]=='I' || str[0]=='i') &&
              (str[1]=='N' || str[1]=='n') &&
              (str[2]=='F' || str[2]=='f') &&
              (str[3]=='I' || str[3]=='i') &&
              (str[4]=='N' || str[4]=='n') &&
              (str[5]=='I' || str[5]=='i') &&
              (str[6]=='T' || str[6]=='t'))) {
        val = std::numeric_limits<double>::infinity();
        str += 8;
        return 0;
    }
    else if (slen > 8 && str[8] == 0 &&
             ((str[0]=='+' || str[0]=='-') &&
              (str[1]=='I' || str[0]=='i') &&
              (str[2]=='N' || str[1]=='n') &&
              (str[3]=='F' || str[2]=='f') &&
              (str[4]=='I' || str[3]=='i') &&
              (str[5]=='N' || str[4]=='n') &&
              (str[6]=='I' || str[5]=='i') &&
              (str[7]=='T' || str[6]=='t'))) {
        val = (str[0]=='+' ? std::numeric_limits<double>::infinity() :
               -std::numeric_limits<double>::infinity());
        str += 9;
        return 0;
    }

    double tmp;
    const bool neg = (*str == '-');
    if (*str == '-' || *str == '+') ++ str;
    // extract the values before the decimal point
    while (*str != 0 && isdigit(*str) != 0) {
        val = 10 * val + static_cast<double>(*str - '0');
        ++ str;
    }

    if (*str == '.') { // values after the decimal point
        tmp = 0.1;
        for (++ str; isdigit(*str); ++ str) {
            val += tmp * static_cast<double>(*str - '0');
            tmp *= 0.1;
        }
    }

    if (*str == 'e' || *str == 'E') {
        ++ str;
        int ierr;
        int64_t ex;
        ierr = ibis::util::readInt(ex, str, del);
        if (ierr == 0) {
            val *= pow(1e1, static_cast<double>(ex));
        }
        else {
            return ierr;
        }
    }

    if (neg) val = - val;
    if (*str != 0) {
        if (del == 0 || *del == 0) {
            // nothing to do
        }
        else if (del[1] == 0) { // skip delimiter
            if (del[0] == *str)
                ++ str;
        }
        else { // skip delimiter
            if (0 != strchr(del, *str))
                ++ str;
        }
    }
    return 0;
} // ibis::util::readDouble

/// Return size of the file in bytes.  The value 0 is returned if
/// file does not exist.
off_t ibis::util::getFileSize(const char* name) {
    if (name != 0 && *name != 0) {
        Stat_T buf;
        if (UnixStat(name, &buf) == 0) {
            if ((buf.st_mode & S_IFREG) == S_IFREG)
                return buf.st_size;
        }
        else {
            LOGGER(ibis::gVerbose > 11 || errno != ENOENT)
                << "Warning -- getFileSize(" << name << ") failed ... "
                << strerror(errno);
        }
    }
    return 0;
}

/// Copy file named "from" to a file named "to".  It overwrites the content
/// of "to".  It returns a negative number if the source file does not
/// existing can not be copied.
int ibis::util::copy(const char* to, const char* from) {
    Stat_T tmp;
    if (UnixStat(from, &tmp) != 0) return -1; // file does not exist
    if ((tmp.st_mode&S_IFDIR) == S_IFDIR // directory
#ifdef S_IFSOCK
        || (tmp.st_mode&S_IFSOCK) == S_IFSOCK // socket
#endif
        ) return -4;

    int fdes = UnixOpen(from, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(errno != ENOENT || ibis::gVerbose > 10)
            << "Warning -- util::copy(" << to << ", " << from
            << ") failed to open " << from << " ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    int tdes = UnixOpen(to, OPEN_WRITENEW, OPEN_FILEMODE);
    if (tdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- util::copy(" << to << ", " << from
            << ") failed to open " << to << " ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -2;
    }
    IBIS_BLOCK_GUARD(UnixClose, tdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(tdes, _O_BINARY);
#endif

    uint32_t nbuf = 16777216; // 16 MB
    char* buf = 0;
    try {buf = new char[nbuf];} // try hard to allocate some space for buf
    catch (const std::bad_alloc&) {
        nbuf >>= 1; // reduce the size by half and try again
        try {buf = new char[nbuf];}
        catch (const std::bad_alloc&) {
            nbuf >>= 2; // reduce the size to a quarter and try again
            buf = new char[nbuf];
        }
    }

    uint32_t i, j;
    if (buf) { // got a sizeable buffer to use
        while ((i = UnixRead(fdes, buf, nbuf))) {
            j = UnixWrite(tdes, buf, i);
            LOGGER(i != j)
                << "Warning -- util::copy(" << to << ", " << from
                << ") failed to write " << i << " bytes, only " << j
                << " bytes are written";
        }
        delete [] buf;
    }
    else {
        // use a static buffer of 256 bytes
        char sbuf[256];
        nbuf = 256;
        while ((i = UnixRead(fdes, sbuf, nbuf))) {
            j = UnixWrite(tdes, sbuf, i);
            LOGGER(i != j)
                << "Warning -- util::copy(" << to << ", " << from
                << ") failed to write " << i << " bytes, actually wrote "
                << j;
        }
    }   

    return 0;
} // ibis::util::copy

/// A wrapper over POSIX read function.  When a large chunk is requested by
/// the user, the read function may return one piece at a time, typically a
/// piece is no larger than 2^31 bytes, and the size of this piece is
/// implementation dependent.  This function attempts use the return value
/// from read when it is a posive value.
int64_t ibis::util::read(int fdes, void *buf, int64_t nbytes) {
    int64_t ierr = nbytes;
    int64_t offset = 0;
    while (ierr > 0) {
        ierr = UnixRead(fdes,
                        static_cast<char*>(buf)+offset,
                        nbytes);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- util::read received error code "
                << ierr << " on file descriptor " << fdes;
            return ierr;
        }
        else {
            nbytes -= ierr;
            offset += ierr;
        }
    }
    return offset;
} // ibis::util::read

/// A wrapper over POSIX write function.  When a large chunk is requested
/// by the user, the write function may return one piece at a time,
/// typically a piece is no larger than 2^31 bytes depending implementation
/// details.  This function attempts use the return value from write when
/// it is a posive value.
int64_t ibis::util::write(int fdes, const void *buf, int64_t nbytes) {
    long ierr = 0;
    int64_t offset = 0;
    while (nbytes > 0) {
        ierr = UnixWrite(fdes,
                         static_cast<const char*>(buf)+offset,
                         nbytes);
        if (ierr <= 0) {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- util::write received error code "
                << ierr << " on file descriptor " << fdes;
            return ierr;
        }
        else {
            nbytes -= ierr;
            offset += ierr;
        }
    }
    return offset;
} // ibis::util::write

/// Remove the content of named directory.
/// If this function is run on a unix-type system and the second argument
/// is true, it will leave all the subdirectories intact as well.
void ibis::util::removeDir(const char* name, bool leaveDir) {
    if (name == 0 || *name == 0) return; // can not do anything
    char* cmd = new char[std::strlen(name)+32];
    char buf[PATH_MAX];
    std::string event = "util::removeDir(";
    event += name;
    event += ")";
#if defined(FASTBIT_RMDIR_USE_RM)
    if (leaveDir)
        sprintf(cmd, "/bin/rm -rf \"%s\"/*", name);
    else
        sprintf(cmd, "/bin/rm -rf \"%s\"", name);
    LOGGER(ibis::gVerbose > 4)
        << event << " issuing command \"" << cmd << "\"";

    FILE* fptr = popen(cmd, "r");
    if (fptr) {
        while (fgets(buf, PATH_MAX, fptr)) {
            LOGGER(ibis::gVerbose > 4)
                << event << " got message -- " << buf;
        }

        int ierr = pclose(fptr);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- command \"" << cmd << "\" returned with error "
                << ierr << " ... " << strerror(errno);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << event << " -- command \"" << cmd << "\" succeeded";
        }
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << event << " failed to popen(" << cmd << ") ... "
            << strerror(errno);
    }
#elif defined(_WIN32) && defined(_MSC_VER)
    sprintf(cmd, "rmdir /s /q \"%s\"", name); // "/s /q" are available on NT
    LOGGER(ibis::gVerbose > 4)
        << event << " issuing command \"" << cmd << "\"...";

    FILE* fptr = _popen(cmd, "rt");
    if (fptr) {
        while (fgets(buf, PATH_MAX, fptr)) {
            LOGGER(ibis::gVerbose > 4)
                << event << " get message -- " << buf;
        }

        int ierr = _pclose(fptr);
        if (ierr && ibis::gVerbose >= 0) { 
            ibis::util::logMessage("Warning ", "command \"%s\" returned with "
                                   "error %d ... %s", cmd, ierr,
                                   strerror(errno));
        }
        else if (ibis::gVerbose > 0) {
            ibis::util::logMessage(event.c_str(), "command \"%s\" succeeded",
                                   cmd);
        }
     }
     else if (ibis::gVerbose >= 0) {
        ibis::util::logMessage("Warning", "%s failed to popen(%s) ... %s",
                               event.c_str(), cmd, strerror(errno));
     }
#elif defined(__unix__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
    char* olddir = getcwd(buf, PATH_MAX);
    if (olddir) {
        olddir = ibis::util::strnewdup(buf);
    }
    else {
        ibis::util::logMessage("util::removeDir", "can not getcwd ... "
                               "%s ", strerror(errno));
    }

    int ierr = chdir(name);
    if (ierr != 0) {
        if (errno != ENOENT) // ok if directory does not exist
            ibis::util::logMessage
                ("util::removeDir", "can not chdir to %s ... %s",
                 name, strerror(errno));
        if (errno == ENOTDIR) { // assume it to be a file
            if (0 != remove(name))
                ibis::util::logMessage
                    ("util::removeDir", "can not remove %s ... %s",
                     name, strerror(errno));
        }
        delete [] cmd;
        return;
    }

    if (buf != getcwd(buf, PATH_MAX)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- util::removeDir failed to determine the current "
            "working directory";
        return;
    }
    uint32_t len = std::strlen(buf);
    if (strncmp(buf, name, len)) { // names differ
        ibis::util::logMessage("util::removeDir", "specified dir name "
                               "is %s, but CWD is actually %s", name, buf);
        strcpy(buf, name);
        len = std::strlen(buf);
    }
    if (buf[len-1] != FASTBIT_DIRSEP) {
        buf[len] = FASTBIT_DIRSEP;
        ++len;
    }

    struct dirent* ent = 0;
    bool isEmpty = true;
    DIR* dirp = opendir(".");
    while ((ent = readdir(dirp)) != 0) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == static_cast<char>(0) ||
             ent->d_name[1] == '.')) {
            continue;       // skip '.' and '..'
        }
        Stat_T fst;

        // construct the full name
        if (len+std::strlen(ent->d_name) >= PATH_MAX) {
            ibis::util::logMessage("util::removeDir", "file name "
                                   "\"%s%s\" too long", buf, ent->d_name);
            isEmpty = false;
            continue;
        }
        strcpy(buf+len, ent->d_name);

        if (UnixStat(buf, &fst) != 0) {
            ibis::util::logMessage("util::removeDir",
                                   "stat(%s) failed ... %s",
                                   buf, strerror(errno));
            if (0 != remove(buf)) {
                ibis::util::logMessage("util::removeDir",
                                       "can not remove %s ... %s",
                                       buf, strerror(errno));
                if (errno != ENOENT) isEmpty = false;
            }
            continue;
        }

        if ((fst.st_mode & S_IFDIR) == S_IFDIR) {
            if (leaveDir)
                isEmpty = false;
            else
                removeDir(buf);
        }
        else { // assume it is a regular file
            if (0 != remove(buf)) {
                ibis::util::logMessage("util::removeDir",
                                       "can not remove %s ... %s",
                                       buf, strerror(errno));
                if (errno != ENOENT) isEmpty = false;
            }
        }
    }
    ierr = closedir(dirp);
    if (olddir) {
        ierr = chdir(olddir);
        if (0 != ierr) {
            ibis::util::logMessage("Warning", "util::removeDir cannot "
                                   "return to %s ... %s", olddir,
                                   (errno ? strerror(errno) : "???"));
        }
        delete [] olddir;
    }

    if (isEmpty && !leaveDir) {
        ierr = rmdir(name);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- util::removeDir can not remove directory "
                << name << " ... " << (errno ? strerror(errno) : "???");
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "util::removeDir removed directory " << name;
        }
    }
    else if (!isEmpty) {
        LOGGER(ibis::gVerbose >= 0)
            << "util::removeDir failed to remove directory "
            << name << " because it is not empty";
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "util::removeDir removed directory " << name;
    }
#else
    ibis::util::logMessage("Warning", "donot know how to delete directory");
#endif
    delete [] cmd;
} // ibis::util::removeDir

/// Compute a compact 64-bit floating-point value in the range (left,
/// right].  The righ-end is inclusive because the computed value is used
/// to define bins based 'x<compactValue' and those bins are interpreted as
/// [low, upper) as in the typical c notion.
///
/// @note The shortest number is considered to be zero (0.0).
/// @note If the given start is not in the valid range, the middle point of
/// the range is used.
/// @note It returns 0.0 if either left or right is not-a-number.
double ibis::util::compactValue(double left, double right,
                                double start) {
    if (left == right) return left;
    if (! (left != right)) return 0.0;
    if (left > right) {
        double tmp;
        tmp = left;
        left = right;
        right = tmp;
    }

    // if zero is in the range, return zero
    if (left < 0 && right >= 0)
        return 0.0;
    // if the range include 1, return 1
    if (left < 1.0 && right >= 1.0)
        return 1.0;
    // if the range include -1, return -1
    if (left < -1.0 && right >= -1.0)
        return -1.0;

    double diff, sep;
    if (left == 0.0) { // left == 0, right > 0
        diff = floor(log10(right));
        sep  = pow(10.0, diff);
        if (sep > right) {
            if (diff >= -3.0 && diff < 3.0)
                sep *= 0.5;
            else
                sep *= 0.1;
        }
    }
    else if (right < 0.0 && right * 10 > left) {
        // negative numbers with a large difference
        diff = ceil(log10(-right));
        sep  = -pow(10.0, diff);
        if (sep > right) {
            if (diff >= -3.0 && diff <= 3.0)
                sep += sep;
            else
                sep *= 10;
        }
    }
    else if (left > 0.0 && right > 10 * left) {
        // postitive numbers with a large difference
        diff = ceil(log10(left));
        sep  = pow(10.0, diff);
        if (sep <= left) {
            if (sep >= -3.0 && diff <= 3.0)
                sep += sep;
            else
                sep *= 10.0;
        }
    }
    else { // two values are within one order of magnitude
        diff = pow(10.0, ceil(FLT_EPSILON + log10(right - left)));
        if (!(start > left && start <= right))
            start = 0.5 * (right + left);
        sep = floor(0.5 + start / diff) * diff;
        if (!(sep > left && sep <= right)) {
            diff /= 2;
            sep = floor(0.5 + start / diff) * diff;
            if (!(sep > left && sep <= right)) {
                diff /= 5;
                sep = floor(0.5 + start / diff) * diff;
                if (! (sep > left && sep <= right)) {
                    diff /= 2;
                    sep = floor(0.5 + start / diff) * diff;
                    if (! (sep > left && sep <= right)) {
                        diff /= 2;
                        sep = floor(0.5 + start / diff) * diff;
                    }
                }
            }
        }
    }
    if (! (sep > left && sep <= right)) {
#if _DEBUG+0 > 1 || DEBUG+0 > 1
        ibis::util::logMessage("Warning", "util::compactValue produced "
                               "a value, %g (%g) out of range (%g, %g]",
                               sep, diff, left, right);
#endif
        sep = right;
    }
    return sep;
} // ibis::util::compactValue

/// This version round the number in binary and tries to use a few binary
/// digits in the mantissa as possible.  The resulting number should have a
/// better chance of producing exact results in simple arithmetic
/// operations.
///
/// @sa ibis::util::compactValue
double ibis::util::compactValue2(double left, double right,
                                 double start) {
    if (left == right) return left;
    if (! (left != right)) return 0.0;
    if (left > right) {
        double tmp;
        tmp = left;
        left = right;
        right = tmp;
    }

    // if zero is in the range, return zero
    if (left < 0 && right >= 0)
        return 0.0;
    // if the range include 1, return 1
    if (left < 1.0 && right >= 1.0)
        return 1.0;
    // if the range include -1, return -1
    if (left < -1.0 && right >= -1.0)
        return -1.0;

    double diff, sep;
    if (left == 0.0) { // left == 0, right > 0
        diff = floor(1.4426950408889633870 * log(right));
        sep  = pow(2.0, diff);
        if (sep > right) {
            sep *= 0.5;
        }
    }
    else if (right < 0.0 && right + right > left) {
        // negative numbers with a large difference
        diff = ceil(1.4426950408889633870 * log(-right));
        sep  = -pow(2.0, diff);
        if (sep > right) {
            sep += sep;
        }
    }
    else if (left > 0.0 && right > left + left) {
        // postitive numbers with a large difference
        diff = ceil(1.4426950408889633870 * log(left));
        sep  = pow(2.0, diff);
        if (sep <= left) {
            sep += sep;
        }
    }
    else { // two values are within a factor 2
        diff = pow(2.0, ceil(FLT_EPSILON + 1.4426950408889633870 *
                             log(right - left)));
        if (!(start > left && start <= right))
            start = 0.5 * (right + left);
        sep = floor(0.5 + start / diff) * diff;
        if (!(sep > left && sep <= right)) {
            diff *= 0.5;
            sep = floor(0.5 + start / diff) * diff;
            if (!(sep > left && sep <= right)) {
                diff *= 0.5;
                sep = floor(0.5 + start / diff) * diff;
                if (! (sep > left && sep <= right)) {
                    diff *= 0.5;
                    sep = floor(0.5 + start / diff) * diff;
                    if (! (sep > left && sep <= right)) {
                        diff *= 0.5;
                        sep = floor(0.5 + start / diff) * diff;
                    }
                }
            }
        }
    }
    if (! (sep > left && sep <= right)) {
#if _DEBUG+0 > 1 || DEBUG+0 > 1
        ibis::util::logMessage("Warning", "util::compactValue2 produced "
                               "a value, %g (%g) out of range (%g, %g]",
                               sep, diff, left, right);
#endif
        sep = right;
    }
    return sep;
} // ibis::util::compactValue2

void ibis::util::setNaN(float& val) {
    //static char tmp[5] = "\x7F\xBF\xFF\xFF";
    val = std::numeric_limits<float>::quiet_NaN();
} // ibis::util::setNaN

void ibis::util::setNaN(double& val) {
    //static char tmp[9] = "\x7F\xF7\xFF\xFF\xFF\xFF\xFF\xFF";
    val = std::numeric_limits<double>::quiet_NaN();
} // ibis::util::setNaN

/// Compute a serial number.  It is a unique number that is always
/// increasing.  Even time FastBit runs, it starts this number with the
/// value 1.  This implementation uses a 32-bit shared integer and
/// therefore could wrap-around when exceeding 2^32.
uint32_t ibis::util::serialNumber() {
    static sharedInt32 cnt;
    return ++cnt;
} // ibis::util::serialNumber

/// Same as strdup() but uses 'new' instead of 'malloc'.  If s == 0, then
/// it returns 0.
char* ibis::util::strnewdup(const char* s) {
    char* str = 0;
    if (s != 0
#ifdef FASTBIT_EMPTY_STRING_AS_NULL
        && *s != static_cast<char>(0)
#endif
        ) { //
        str = new char[std::strlen(s)+1];
        std::strcpy(str, s);
    }
    return str;
} // ibis::util::strnewdup

char* ibis::util::strnewdup(const char* s, const uint32_t n) {
    char* str = 0;
    if (n > 0 && s != 0 && *s != static_cast<char>(0)) {
        uint32_t len = std::strlen(s);
        if (n < len)
            len = n;
        str = new char[len+1];
        strncpy(str, s, len);
        str[len] = 0;
    }
    return str;
} // ibis::util::strnewdup

/// Compute a denominator and numerator pair. 
/// The fraction (denominator / numerator) is uniformly distributed in [0, 1).
void ibis::util::uniformFraction(const long unsigned idx,
                                 long unsigned &denominator,
                                 long unsigned &numerator) {
    switch (idx) {
    case 0:
        denominator = 0;
        numerator = 1;
        break;
    case 1:
        denominator = 1;
        numerator = 2;
        break;
    case 2:
        denominator = 1;
        numerator = 4;
        break;
    case 3:
        denominator = 3;
        numerator = 4;
        break;
    default: // the general case
        if (idx <= INT_MAX) {
            denominator = 4;
            numerator = 8;
            while (idx >= numerator) {
                denominator = numerator;
                numerator += numerator;
            }
            denominator = 2 * (idx - denominator) + 1;
        }
        else { // could not deal with very large numbers
            denominator = 0;
            numerator = 1;
        }
    }
} // unformMultiplier

/// Fletcher's arithmetic checksum with 32-bit result.
uint32_t ibis::util::checksum(const char* str, uint32_t sz) {
    uint32_t c0 = 0;
    uint32_t c1 = 0;
    const unsigned ssz = sizeof(short);
    const short unsigned* ptr = reinterpret_cast<const unsigned short*>(str);
    while (sz >= ssz) {
        c1 += c0;
        c0 += *ptr;
        ++ ptr;
        sz -= ssz;
    }
    if (sz > 0) {
        c1 += c0;
        c0 += *ptr;
    }
    uint32_t ret = (((c1 & 0xFFFF) << 16) ^ c0);
    return ret;
} // checksum for string

/// Convert the incoming numbers into a base-64 alpha-numeric
/// representation.  Three 32-bit integers can be packed int 16 base-64
/// alphabets.
void ibis::util::int2string(std::string& str,
                            const std::vector<unsigned>& val) {
    uint32_t i;
    str.erase();
    std::string tmp;
    for (i = 0; i + 2 < val.size(); i += 3) {
        ibis::util::int2string(tmp, val[i], val[i+1], val[i+2]);
        str += tmp;
    }
    i -= 3;
    const uint32_t rem = val.size() - i;
    if (rem == 2) {
        ibis::util::int2string(tmp, val[i], val[i+1]);
        str += tmp;
    }
    else if (rem == 1) {
        ibis::util::int2string(tmp, val[i]);
        str += tmp;
    }
} // ibis::util::int2string

/// Pack three 32-bit integers into 16 base-64 alphabets.
void ibis::util::int2string(std::string& str, unsigned v1, unsigned v2,
                            unsigned v3) {
    char name[17];
    name[16] = (char)0;
    name[15] = ibis::util::charTable[63 & v3]; v3 >>= 6;
    name[14] = ibis::util::charTable[63 & v3]; v3 >>= 6;
    name[13] = ibis::util::charTable[63 & v3]; v3 >>= 6;
    name[12] = ibis::util::charTable[63 & v3]; v3 >>= 6;
    name[11] = ibis::util::charTable[63 & v3]; v3 >>= 6;
    name[10] = ibis::util::charTable[63 & (v3 | (v2<<2))]; v2 >>= 4;
    name[9] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[8] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[7] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[6] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[5] = ibis::util::charTable[63 & (v2 | (v1<<4))]; v1 >>= 2;
    name[4] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[3] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[2] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[1] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[0] = ibis::util::charTable[63 & v1];
    str = name;
} // ibis::util::int2string

/// Pack two 32-bit integers into 11 base-64 alphabets.
void ibis::util::int2string(std::string& str, unsigned v1, unsigned v2) {
    char name[12];
    name[11] = (char)0;
    name[10] = ibis::util::charTable[15 & v2]; v2 >>= 4;
    name[9] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[8] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[7] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[6] = ibis::util::charTable[63 & v2]; v2 >>= 6;
    name[5] = ibis::util::charTable[63 & (v2 | (v1<<4))]; v1 >>= 2;
    name[4] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[3] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[2] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[1] = ibis::util::charTable[63 & v1]; v1 >>= 6;
    name[0] = ibis::util::charTable[63 & v1];
    str = name;
} // ibis::util::int2string

/// Pack a 32-bit integer into six base-64 alphabets.
void ibis::util::int2string(std::string& str, unsigned val) {
    char name[7];
    name[6] = (char)0;
    name[5] = ibis::util::charTable[ 3 & val]; val >>= 2;
    name[4] = ibis::util::charTable[63 & val]; val >>= 6;
    name[3] = ibis::util::charTable[63 & val]; val >>= 6;
    name[2] = ibis::util::charTable[63 & val]; val >>= 6;
    name[1] = ibis::util::charTable[63 & val]; val >>= 6;
    name[0] = ibis::util::charTable[63 & val];
    str = name;
} // ibis::util::int2string

/// Use the Fletcher's checksum to produce a short string.  The result is
/// often used as name of temporary table objects.  The name conforms to
/// the SQL standard.
std::string ibis::util::shortName(const std::string& de) {
    std::string tn;
    ibis::util::int2string(tn, ibis::util::checksum(de.c_str(), de.size()));
    if (0 == isalpha(tn[0]))
        tn[0] = '_';
    size_t i1 = 1, i2 = 2;
    while (i1 < tn.size() && i2 < tn.size()) {
        if (isalnum(tn[i1]) == 0) {
            if (i2 <= i1) i2 = i1 + 1;
            while (i2 < tn.size() && isalnum(tn[i2]) == 0) ++ i2;
            if (i2 < tn.size()) {
                tn[i1] = tn[i2];
                tn[i2] = ' ';
                ++ i1;
                ++ i2;
            }
        }
        else {
            ++ i1;
        }
    }
    tn.erase(i1);
    return tn;
} // ibis::util::shortName

/// Generate a short string to be used as a table/partition name.
std::string ibis::util::randName(const std::string& de) {
    std::string tn;
    ibis::util::int2string(tn, ibis::util::checksum(de.c_str(), de.size()) ^
                           ibis::util::serialNumber());
    if (0 == isalpha(tn[0]))
        tn[0] = '_';
    size_t i1 = 1, i2 = tn.size()-1;
    while (i1 <= i2) {
        if (isalnum(tn[i1]) != 0) {
            ++ i1;
        }
        else {
            while (i2 > 1 && isalnum(tn[i2]) == 0)
                -- i2;
            if (i2 > i1) {
                tn[i1] = tn[i2];
                ++ i1;
                -- i2;
            }
        }
    }
    tn.erase(i1);
    return tn;
} // ibis::util::randName

/// Produce a string version of the unsigned integer value with the decimal
/// digits grouped into 1000s.
std::string ibis::util::groupby1000(uint64_t val) {
    if (val >= 1000) {
        const char separator = std::use_facet< std::numpunct<char> >
            (std::cout.getloc()).thousands_sep();
        std::string res;
        // loop to extract the decimal digits
        unsigned char ott = 0;
        while (res.empty() || val > 0) {
            uint64_t quo = val / 10U;
            res += (unsigned char)(val - quo * 10) + '0';
            val = quo;
            ++ ott;
            if (ott > 2 && val > 0) {
                res += separator;
                ott = 0;
            }
        }

        // reverse the string
        const size_t jmax = res.size() / 2;
        const size_t nres = res.size() - 1;
        for (size_t j = 0; j < jmax; ++ j) {
            ott = res[j];
            res[j] = res[nres-j];
            res[nres-j] = ott;
        }
        return res;
    }
    else {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }
} // ibis::util::groupby1000

/// Turn the incoming integer into a 64-bit representation.  The resulting
/// string is stored in the output variable buf.  The return value of this
/// function is buf.c_str() if this function completes successfully,
/// otherwise, a nil pointer is returned.
void ibis::util::encode64(uint64_t input, std::string &buf) {
    buf.clear();
    do {
        buf += ibis::util::charTable[input & 64];
        input >>= 6;
    } while (input > 0);

    const size_t end = buf.size()-1;
    for (size_t j = 0; j < end-j; ++ j) {
        char tmp = buf[j];
        buf[j] = buf[end-j];
        buf[end-j] = tmp;
    }
} // ibis::util::encode64

/// Decode a number encoded using ibis::util::encode64.
int ibis::util::decode64(uint64_t &output, const std::string &buf) {
    output = 0;
    if (buf.empty() || buf.size() > 11 ||
        (buf.size()==11 && ibis::util::charIndex[(unsigned)buf[10]] >= 16)) return -1;

    output = ibis::util::charIndex[(unsigned)buf[0]];
    if (output >= 64) return -2;
    if (buf.size() == 1) return 0;

    for (size_t j = 1; j < buf.size(); ++ j) {
        output <<= 6;
        unsigned short tmp = ibis::util::charIndex[(unsigned)buf[j]];
        if (tmp < 64) {
            output = output | tmp;
        }
        else {
            output = 0;
            return -3;
        }
    }
    return 0;
} // ibis::util::decode64

/// Convert a string of hexadecimal digits back to an integer.
/// The return values are:
/// - 0: successful completion of the this function.
/// - -1: incoming string is empty or a nil pointer.
/// - -2: incoming string contaings something not hexadecimal.  The string
///       may contain '0x' or '0X' as prefix and 'h' or 'H' as suffix.
/// - -3: incoming string has more than 16 hexadecimal digits.
int ibis::util::decode16(uint64_t &output, const char* buf) {
    output = 0;
    if (buf == 0 || *buf == 0) return -1;
    while (isspace(*buf)) ++ buf; // skip leading space
    if (*buf == '0' && (buf[1] == 'x' || buf[1] == 'X')) buf += 2;

    uint16_t sz;
    for (sz = 0; *buf != 0 && sz < 16; ++ buf, ++ sz) {
        output <<= 4;
        switch (*buf) {
        default:
            return -2;
        case '0':
            break;
        case '1':
            ++ output;
            break;
        case '2':
            output |= 2;
            break;
        case '3':
            output |= 3;
            break;
        case '4':
            output |= 4;
            break;
        case '5':
            output |= 5;
            break;
        case '6':
            output |= 6;
            break;
        case '7':
            output |= 7;
            break;
        case '8':
            output |= 8;
            break;
        case '9':
            output |= 9;
            break;
        case 'a':
        case 'A':
            output |= 10;
            break;
        case 'b':
        case 'B':
            output |= 11;
            break;
        case 'c':
        case 'C':
            output |= 12;
            break;
        case 'd':
        case 'D':
            output |= 13;
            break;
        case 'e':
        case 'E':
            output |= 14;
            break;
        case 'f':
        case 'F':
            output |= 15;
            break;
        case 'h':
        case 'H':
            output >>= 4;
            for (++ buf; isspace(*buf); ++ buf);
            if (*buf == 0) {
                return 0;
            }
            else {
                return -2;
            }
        }
    }

    while (isspace(*buf)) ++ buf;
    if (sz <= 16 && *buf == 0) // ok
        return 0;
    else // string too long
        return -3;
} // ibis::util::decode16

/// It attempts to retrieve the user name from the system and store it
/// locally.  A global mutex lock is used to ensure that only one thread
/// can access the locally stored user name.  If it fails to determine the
/// user name, it will return '<(-_-)>', which is a long form of the robot
/// emoticon.
const char* ibis::util::userName() {
    static std::string uid;
    if (! uid.empty()) return uid.c_str();
    ibis::util::mutexLock lock(&ibis::util::envLock, "<(-_-)>");

    if (uid.empty()) {
#if defined(_WIN32) && defined(_MSC_VER)
        long unsigned int len = 63;
        char buf[64];
        if (GetUserName(buf, &len))
            uid = buf;
#elif defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
        // MinGW does not have support for user names?!
#elif defined(HAVE_GETPWUID) && !(defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__))
#if (defined(HAVE_GETPWUID_R) || defined(_REENTRANT) || \
     defined(_POSIX_THREAD_SAFE_FUNCTIONS) || defined(__sun) || \
     defined(_THREAD_SAFE) || defined(__APPLE__) || defined(__FreeBSD__)) && \
    defined(_SC_GETPW_R_SIZE_MAX)
        // use the thread-safe version of getpwuid_r
        struct passwd  pass;
        struct passwd *ptr1 = &pass;
        int nbuf = sysconf(_SC_GETPW_R_SIZE_MAX);
        int ierr = (nbuf > 0 ? 0 : -1);
        if (ierr == 0) {
            char *buf = new char[nbuf];
            if (buf != 0) {
                struct passwd *ptr2 = 0;
                ierr = getpwuid_r(getuid(), ptr1, buf, nbuf, &ptr2);
                if (ierr == 0) {
                    uid = pass.pw_name;
                }
                // if (ptr2 != ptr1)
                //     delete ptr2;
                delete [] buf;
            }
        }
        if (ierr != 0) {
            // the trusted getpwuid(getuid()) combination
            struct passwd *ptr3 = getpwuid(getuid());
            if (ptr3 != 0)
                uid = ptr3->pw_name;
            // delete ptr3;
        }
#else
        // the trusted getpwuid(getuid()) combination
        struct passwd *pass = getpwuid(getuid());
        if (pass != 0)
            uid = pass->pw_name;
        // delete pass;
#endif
#elif defined(L_cuserid) && defined(__USE_XOPEN)
        // in the unlikely case that we are not on a POSIX-compliant system
        // https://buildsecurityin.us-cert.gov/daisy/bsi-rules/home/g1/731.html
        char buf[L_cuserid+1];
        (void) cuserid(buf);
        if (*buf)
            uid = buf;
        // Warning: do not attempt to use getlogin because getlogin and
        // variants need a TTY or utmp entry to function correctly.  They
        // may cause a core dump if this function is called without a TTY
        // (or utmp)
#endif
        if (uid.empty()) {
            const char* nm = getenv("LOGNAME");
            if (nm != 0 && *nm != 0) {
                uid = nm;
            }
            else {
                nm = getenv("USER");
                if (nm != 0 && *nm != 0)
                    uid = nm;
            }
        }

        // fall back option, assign a fixed string that is commonly
        // interpreted as a robot
        if (uid.empty())
            uid = "<(-_-)>";
    }
    return uid.c_str();
} // ibis::util::userName

/// Print a message to standard output.  The format string is same as
/// printf.  A global mutex lock is used to ensure printed messages are
/// ordered.  The message is preceeded with the current local time (as from
/// function ctime) if the macro FASTBIT_TIMED_LOG is defined at the
/// compile time.
void ibis::util::logMessage(const char* event, const char* fmt, ...) {
    if (ibis::gVerbose < 0) return;

    FILE* fptr = ibis::util::getLogFile();
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
#if defined(FASTBIT_TIMED_LOG)
    char tstr[28];
    ibis::util::getLocalTime(tstr);

    ibis::util::ioLock lock;
    fprintf(fptr, "%s   %s -- ", tstr, event);
#else
    ibis::util::ioLock lock;
    fprintf(fptr, "%s -- ", event);
#endif
    va_list args;
    va_start(args, fmt);
    vfprintf(fptr, fmt, args);
    va_end(args);
    fprintf(fptr, "\n");
#if defined(_DEBUG) || defined(DEBUG) || defined(FASTBIT_SYNC_WRITE)
    fflush(fptr);
#endif
#else
    ibis::util::logger lg;
    lg() << event << " -- " << fmt << " ...";
#endif
} // ibis::util::logMessage

/// Change the current log file to the named file.  Log files are
/// opened in append mode, so the existing content will be
/// preserved.  The log file will be changed only if the named file
/// can be opened correctly.
///
/// Error code
/// -  0: success;
/// - -1: unable to open the specified file,
/// - -2: unable to write to the specified file.
///
/// @note The incoming argument is generally interpreted as a file name and
/// passed to fopen, however, there are three special values: a blank
/// string (or a nil pointer), "stdout" and "stderr."  The blank string (or
/// a null pointer) resets the log file to the default value
/// FASTBIT_DEFAULT_LOG.  The default value of FASTBIT_DEFAULT_LOG is
/// explained in function ibis::util::getLogFile.  The values of "stderr"
/// and "stdout" refer to the stderr and stdout defined on most UNIX
/// systems.  Note that both "stderr" and "stdout" must be in lower case if
/// the caller intends to use stderr or stdout.
///
/// @sa ibis::util::getLogFile
int ibis::util::setLogFileName(const char* filename) {
    if (filename == 0 || *filename == 0) {
        if (ibis_util_logfilename.empty() &&
            ibis_util_logfilepointer == FASTBIT_DEFAULT_LOG)
            return 0;
        else
            return ibis::util::writeLogFileHeader(FASTBIT_DEFAULT_LOG, 0);
    }
    else if (std::strcmp(filename, "stderr") == 0) {
        if (ibis_util_logfilename.empty() &&
            ibis_util_logfilepointer == stderr)
            return 0;
        else
            return ibis::util::writeLogFileHeader(stderr, 0);
    }
    else if (std::strcmp(filename, "stdout") == 0) {
        if (ibis_util_logfilename.empty() &&
            ibis_util_logfilepointer == stdout)
            return 0;
        else
            return ibis::util::writeLogFileHeader(stdout, 0);
    }
    if (ibis_util_logfilename.compare(filename) == 0)
        return 0;

    FILE* fptr = fopen(filename, "a");
    if (fptr != 0) {
        return writeLogFileHeader(fptr, filename);
    }
    else {
        return -1;
    }
} // ibis::util::setLogFileName

/// Write a header to the log file.  The caller has opened the file named
/// fname, this function attempts to write a simple message to verify that
/// the file is writable.  It returns zero (0) upon successful completion
/// of the write operation, it returns a non-zero number to indicate error.
///
/// If fptr is 0, it passes fname to ibis::util::setLogFileName to open the
/// appropriate file.  Otherwise, this function attempt to write to fptr.
int ibis::util::writeLogFileHeader(FILE *fptr, const char *fname) {
    if (fptr == 0)
        return ibis::util::setLogFileName(fname);

    char tstr[28];
    ibis::util::getLocalTime(tstr);
    const char *str = ibis::util::getVersionString();
    std::string tmp;
    if (str == 0 || *str == 0) {
        tmp = "FastBit ibis";
        std::ostringstream oss;
        oss << ibis::util::getVersionNumber();
        tmp += oss.str();
        tmp += ", Copyright (c) (c)-2015";
        str = tmp.c_str();
    }
    int ierr = 0;
    if (fname != 0 && *fname != 0)
        ierr = fprintf(fptr, "\n%s\nLog file %s opened on %s\n",
                       str, fname, tstr);
    else if (ibis::gVerbose > 1)
        ierr = fprintf(fptr, "\n%s\nLog started on %s\n",
                       str, tstr);
    else
        ierr = fprintf(fptr, "\n");
    if (ierr > 0) {
        if (fname != 0 && *fname != 0 && ibis::gVerbose > 1) {
#ifdef FASTBIT_BUGREPORT
        str = FASTBIT_BUGREPORT;
        if (str != 0 && *str != 0)
            (void) fprintf(fptr, "\tsend comments and bug reports to %s\n",
                           str);
#endif
#ifdef FASTBIT_URL
        str = FASTBIT_URL;
        if (str != 0 && *str != 0)
            (void) fprintf(fptr, "\tread more about the package at %s\n",
                           str);
#endif
        }

        ibis::util::ioLock lock;
        if (ibis_util_logfilepointer != 0 &&
            ibis_util_logfilepointer != fptr &&
            ibis_util_logfilepointer != stdout &&
            ibis_util_logfilepointer != stderr) // close existing file
            (void) fclose(ibis_util_logfilepointer);

        // set the global variables ibis_util_logfilepointer
        // and ibis_util_logfilename
        ibis_util_logfilepointer = fptr;
        if (fname != 0 && *fname != 0)
            ibis_util_logfilename = fname;
        else
            ibis_util_logfilename.erase();
        return 0;
    }
    else {
        return -2;
    }
} // ibis::util::writeLogFileHeader

/// Retrieve the pointer to the log file.  The value of FASTBIT_DEFAULT_LOG
/// will be returned if no log file was specified.  A log file name be
/// specified through the following means (in the specified order),
///
/// -- setLogFile
/// -- environment variable FASTBITLOGFILE
/// -- configuration parameter logfile
///
/// @note The macro FASTBIT_DEFAULT_LOG is default to stderr.  It could be
/// directly set to another FILE* during compile time by changing
/// FASTBIT_DEFAULT_LOG or changed at run time through
/// ibis::util::setLogFileName.  Instead of directly giving a FILE* to
/// FASTBIT_DEFAULT_LOG, one may also set either FASTBIT_LOG_TO_STDERR or
/// FASTBIT_LOG_TO_STDOUT at compile time to change the default log file to
/// stderr or stdout.
FILE* ibis::util::getLogFile() {
    if (ibis_util_logfilepointer != 0) { // file pointer already set
        return ibis_util_logfilepointer;
    }
    else { // wait and see if another thread will set it
        ibis::util::ioLock lock;
        if (ibis_util_logfilepointer != 0)
            return ibis_util_logfilepointer;
    }

    const char* fname = (ibis_util_logfilename.empty() ? 0 :
                         ibis_util_logfilename.c_str());
    if (fname != 0 && *fname != 0) {
        if (setLogFileName(fname) == 0) {
            return ibis_util_logfilepointer;
        }
    }

    fname = getenv("FASTBITLOGFILE");
    if (fname != 0 && *fname != 0) {
        if (setLogFileName(fname) == 0) {
            return ibis_util_logfilepointer;
        }
    }

    fname = ibis::gParameters()["logfile"];
    if (fname == 0 || *fname == 0)
        fname = ibis::gParameters()["mesgfile"];
    if (fname != 0 && *fname == 0) {
        if (setLogFileName(fname) == 0) {
            return ibis_util_logfilepointer;
        }
    }

    // fall back to use FASTBIT_DEFAULT_LOG, writeLogFileHeader should not fail
    (void) writeLogFileHeader(FASTBIT_DEFAULT_LOG, 0);
    return ibis_util_logfilepointer;
} // ibis::util::getLogFile

/// Return name the of the current log file.  A blank string indicates a
/// standard output stream, either stderr or stdout.
const char* ibis::util::getLogFileName() {
    return ibis_util_logfilename.c_str();
} // ibis::util::getLogFileName

/// Close the log file.  This function is registered with atexit through
/// ibis::init.  It will be automatically invoked during exit.  However, if
/// necessary, the log file will be reopened during later operations.  In
/// any case, the operating system will close the log file after the
/// termination of the program.
void ibis::util::closeLogFile() {
    ibis::util::ioLock lock;
    if (ibis_util_logfilepointer != 0 &&
        ibis_util_logfilepointer != stdout &&
        ibis_util_logfilepointer != stderr) {
        (void) fclose(ibis_util_logfilepointer);
        ibis_util_logfilepointer = 0;
    }
} // ibis::util::closeLogFile

/// The argument to this constructor is taken to be the number of spaces
/// before the message.  When the macro FASTBIT_TIMED_LOG is defined at
/// compile time, a time stamp is printed before the spaces.  The number of
/// leading blanks is usually the verboseness level.  Typically, a message
/// is formed only if the global verboseness level is higher than the level
/// assigned to the particular message (through the use of LOGGER macro or
/// explicit if statements).  In addition, the message is only outputed if
/// the global verboseness level is no less than 0.
ibis::util::logger::logger(int lvl) {
#if defined(FASTBIT_TIMED_LOG)
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    mybuffer << tster << " ";
#endif
    if (lvl > 4) {
        if (lvl > 1000) lvl = 10 + (int)sqrt(log((double)lvl));
        else if (lvl > 8) lvl = 6 + (int)log((double)lvl);
        for (int i = 0; i < lvl; ++ i)
            mybuffer << " ";
    }
    else if (lvl == 4) {
        mybuffer << "    ";
    }
    else if (lvl == 3) {
        mybuffer << "   ";
    }
    else if (lvl == 2) {
        mybuffer << "  ";
    }
} // ibis::util::logger::logger

/// Output the message and timing information from this function.
ibis::util::logger::~logger() {
    const std::string& mystr = mybuffer.str();
    if (ibis::gVerbose >= 0 && ! mystr.empty()) {
        FILE* fptr = ibis::util::getLogFile();
        // The lock is still necessary because other logging functions use
        // multiple fprintf statements.
        ibis::util::ioLock lock;
        (void) fwrite(mystr.c_str(), mystr.size(), 1U, fptr);
        (void) fwrite("\n", 1U, 1U, fptr);
#if defined(_DEBUG) || defined(DEBUG) || defined(FASTBIT_SYNC_WRITE)
        (void) fflush(fptr);
#endif
    }
} // ibis::util::logger::~logger

/// Return the current content of the string stream in the form of a
/// std::string.  The content of the string stream is copied into the
/// output string.
std::string ibis::util::logger::str() const {
    return mybuffer.str();
} // ibis::util::logger::str

/// Return a nil-terminated version of the string steam content.  This
/// function relies on the function std::ostringstream::str, which makes
/// copy of the string stream buffer.
const char* ibis::util::logger::c_str() const {
    return mybuffer.str().c_str();
} // ibis::util::logger::c_str

/// Constructor.  The caller must provide a message string.  If
/// ibis::gVerbose is no less than lvl, it will create an ibis::horometer
/// object to keep the time and print the duration of the timer in the
/// destructor.  If ibis::gVerbose is no less than lvl+2, it will also
/// print a message from the constructor.
///
/// @note This class holds a private copy of the message to avoid relying
/// on the incoming message being present at the destruction time.  @sa
/// ibis::horometer
ibis::util::timer::timer(const char* msg, int lvl) : chrono_(0) {
    if (ibis::gVerbose >= lvl && msg != 0 && *msg != 0) {
        mesg_ = msg;
        chrono_ = new ibis::horometer;
    }
    if (chrono_ != 0) {
        chrono_->start();
        LOGGER(ibis::gVerbose > lvl+1)
            << mesg_ << " -- start timer ...";
    }
} // ibis::util::timer::timer

/// Destructor.  It reports the time elapsed since the constructor was
/// called.  Use ibis::horometer directly if more control on the timing
/// information is desired.
ibis::util::timer::~timer() {
    if (chrono_ != 0) {
        chrono_->stop();
        ibis::util::logger(0)()
            << mesg_ << " -- duration: " << chrono_->CPUTime()
            << " sec(CPU), " << chrono_->realTime() << " sec(elapsed)";
        delete chrono_;
    }
} // ibis::util::timer::~timer

/// Match the string @c str against a simple pattern @c pat.
///
/// If the whole string matches the pattern, this function returns true,
/// otherwise, it returns false.  The special cases are (1) if the two
/// pointers are the same, it returns true; (2) if both arguments point to
/// an empty string, it returns true; (3) if one of the two argument is a
/// nil pointer, but the other is not, it returns false; (4) if str is
/// an empty string, it matches a pattern containing only '%' and '*'.
///
/// The pattern may contain five special characters, two matches any number
/// of characters, STRMATCH_META_CSH_ANY and STRMATCH_META_SQL_ANY, two
/// matches exactly one character, STRMATCH_META_CSH_ONE and
/// STRMATCH_META_SQL_ONE, and STRMATCH_META_ESCAPE.  This combines the
/// meta characters used in C-shell file name substitution and SQL LIKE
/// clause. 
///
/// @note The strings are matched with the same case, i.e., the
/// match is case sensitive, unless the macro
/// FASTBIT_CASE_SENSITIVE_COMPARE is defined to be 0 at compile time.
bool ibis::util::strMatch(const char *str, const char *pat) {
    static const char metaList[6] = "?*_%\\";
    /* Since the escape character is special to C/C++ too, the following initialization causes problem for some compilers!
    static const char metaList[] = {STRMATCH_META_CSH_ANY,
                                    STRMATCH_META_SQL_ANY,
                                    STRMATCH_META_CSH_ONE,
                                    STRMATCH_META_SQL_ONE,
                                    STRMATCH_META_ESCAPE}; */
    if (str == pat) {
        return true;
    }
    else if (pat == 0) {
        return (str == 0);
    }
    else if (*pat == 0) {
        return (str != 0 && *str == 0);
    }
    else if (str == 0) {
        return false;
    }
    else if (*str == 0) {
        bool onlyany = false;
        for (onlyany = (*pat == STRMATCH_META_CSH_ANY ||
                        *pat == STRMATCH_META_SQL_ANY),
                 ++ pat;
             onlyany && *pat != 0; ++ pat)
            onlyany = (*pat == STRMATCH_META_CSH_ANY ||
                       *pat == STRMATCH_META_SQL_ANY);
        return onlyany;
    }

    const char *s1 = strpbrk(pat, metaList);
    const long int nhead = s1 - pat;
    if (s1 < pat) { // no meta character
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
        return (0 == stricmp(str, pat));
#else
        return (0 == std::strcmp(str, pat));
#endif
    }
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
    else if (s1 > pat && 0 != strnicmp(str, pat, nhead)) {
#else
    else if (s1 > pat && 0 != strncmp(str, pat, nhead)) {
#endif
        // characters before the first meta character do not match
        return false;
    }

    if (*s1 == STRMATCH_META_ESCAPE) { // escape character
        if (str[nhead] == pat[nhead+1])
            return strMatch(str+nhead+1, pat+nhead+2);
        else
            return false;
    }
    else if (*s1 == STRMATCH_META_CSH_ONE || *s1 == STRMATCH_META_SQL_ONE) {
        // match exactly one character
        if (str[nhead] != 0)
            return strMatch(str+nhead+1, pat+nhead+1);
        else
            return false;
    }

    // found STRMATCH_META_*_ANY
    const char* s0 = str + nhead;
    do { // skip consecutive STRMATCH_META_*_ANY
        ++ s1;
    } while (*s1 == STRMATCH_META_CSH_ANY || *s1 == STRMATCH_META_SQL_ANY);
    if (*s1 == 0) // pattern end with STRMATCH_META_ANY
        return true;

    const char* s2 = 0;
    bool  ret;
    if (*s1 == STRMATCH_META_ESCAPE) { // skip STRMATCH_META_ESCAPE
        ++ s1;
        if (*s1 != 0)
            s2 = strpbrk(s1+1, metaList);
        else
            return true;
    }
    else if (*s1 == STRMATCH_META_CSH_ONE || *s1 == STRMATCH_META_SQL_ONE) {
        do {
            if (*s0 != 0) { // STRMATCH_META_*_ONE matched
                ++ s0;
                // STRMATCH_META_*_ANY STRMATCH_META_*_ONE
                // STRMATCH_META_*_ANY
                // ==> STRMATCH_META_*_ANY STRMATCH_META_*_ONE
                do {
                    ++ s1;
                } while (*s1 == STRMATCH_META_CSH_ANY ||
                         *s1 == STRMATCH_META_SQL_ANY);
            }
            else { // end of str, STRMATCH_META_*_ONE not matched
                return false;
            }
        } while (*s1 == STRMATCH_META_CSH_ONE ||
                 *s1 == STRMATCH_META_SQL_ONE);
        if (*s1 == 0) {
            // pattern end with STRMATCH_META_*_ANY plus a number of
            // STRMATCH_META_*_ONE that have been matched.
            return true;
        }
        if (*s1 == STRMATCH_META_ESCAPE) {
            ++ s1;
            if (*s1 != 0)
                s2 = strpbrk(s1+1, metaList);
        }
        else {
            s2 = strpbrk(s1, metaList);
        }
    }
    else {
        s2 = strpbrk(s1, metaList);
    }

    if (s2 < s1) { // no more meta character
        const uint32_t ntail = std::strlen(s1);
        if (ntail <= 0U)
            return true;

        uint32_t nstr = std::strlen(s0);
        if (nstr < ntail)
            return false;
        else
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
            return (0 == stricmp(s1, s0+(nstr-ntail)));
#else
            return (0 == std::strcmp(s1, s0+(nstr-ntail)));
#endif
    }

    const std::string anchor(s1, s2);
    const char* tmp = strstr(s0, anchor.c_str());
    if (tmp < s0) return false;
    ret = strMatch(tmp+anchor.size(), s2);
    while (! ret) { // retry
        tmp = strstr(tmp+1, anchor.c_str());
        if (tmp > s0) {
            ret = strMatch(tmp+anchor.size(), s2);
        }
        else {
            break;
        }
    }
    return ret;
} // ibis::util::strMatch

/// Match the string @c str against a simple pattern @c pat without
/// considering cases.  This is to follow the SQL standard for comparing
/// names.
///
/// @sa ibis::util::strMatch
bool ibis::util::nameMatch(const char *str, const char *pat) {
    static const char metaList[6] = "?*_%\\";
    if (str == pat) {
        return true;
    }
    else if (pat == 0) {
        return (str == 0);
    }
    else if (*pat == 0) {
        return (str != 0 && *str == 0);
    }
    else if (str == 0) {
        return false;
    }
    else if (*str == 0) {
        bool onlyany = false;
        for (onlyany = (*pat == STRMATCH_META_CSH_ANY ||
                        *pat == STRMATCH_META_SQL_ANY),
                 ++ pat;
             onlyany && *pat != 0; ++ pat)
            onlyany = (*pat == STRMATCH_META_CSH_ANY ||
                       *pat == STRMATCH_META_SQL_ANY);
        return onlyany;
    }

    const char *s1 = strpbrk(pat, metaList);
    const long int nhead = s1 - pat;
    if (s1 < pat) { // no meta character
        return (0 == stricmp(str, pat));
    }
    else if (s1 > pat && 0 != strnicmp(str, pat, nhead)) {
        // characters before the first meta character do not match
        return false;
    }

    if (*s1 == STRMATCH_META_ESCAPE) { // escape character
        if (str[nhead] == pat[nhead+1])
            return nameMatch(str+nhead+1, pat+nhead+2);
        else
            return false;
    }
    else if (*s1 == STRMATCH_META_CSH_ONE || *s1 == STRMATCH_META_SQL_ONE) {
        // match exactly one character
        if (str[nhead] != 0)
            return nameMatch(str+nhead+1, pat+nhead+1);
        else
            return false;
    }

    // found STRMATCH_META_*_ANY
    const char* s0 = str + nhead;
    do { // skip consecutive STRMATCH_META_*_ANY
        ++ s1;
    } while (*s1 == STRMATCH_META_CSH_ANY || *s1 == STRMATCH_META_SQL_ANY);
    if (*s1 == 0) // pattern end with STRMATCH_META_ANY
        return true;

    const char* s2 = 0;
    bool  ret;
    if (*s1 == STRMATCH_META_ESCAPE) { // skip STRMATCH_META_ESCAPE
        ++ s1;
        if (*s1 != 0)
            s2 = strpbrk(s1+1, metaList);
        else
            return true;
    }
    else if (*s1 == STRMATCH_META_CSH_ONE || *s1 == STRMATCH_META_SQL_ONE) {
        do {
            if (*s0 != 0) { // STRMATCH_META_*_ONE matched
                ++ s0;
                // STRMATCH_META_*_ANY STRMATCH_META_*_ONE
                // STRMATCH_META_*_ANY
                // ==> STRMATCH_META_*_ANY STRMATCH_META_*_ONE
                do {
                    ++ s1;
                } while (*s1 == STRMATCH_META_CSH_ANY ||
                         *s1 == STRMATCH_META_SQL_ANY);
            }
            else { // end of str, STRMATCH_META_*_ONE not matched
                return false;
            }
        } while (*s1 == STRMATCH_META_CSH_ONE ||
                 *s1 == STRMATCH_META_SQL_ONE);
        if (*s1 == 0) {
            // pattern end with STRMATCH_META_*_ANY plus a number of
            // STRMATCH_META_*_ONE that have been matched.
            return true;
        }
        if (*s1 == STRMATCH_META_ESCAPE) {
            ++ s1;
            if (*s1 != 0)
                s2 = strpbrk(s1+1, metaList);
        }
        else {
            s2 = strpbrk(s1, metaList);
        }
    }
    else {
        s2 = strpbrk(s1, metaList);
    }

    if (s2 < s1) { // no more meta character
        const uint32_t ntail = std::strlen(s1);
        if (ntail <= 0U)
            return true;

        uint32_t nstr = std::strlen(s0);
        if (nstr < ntail)
            return false;
        else
            return (0 == stricmp(s1, s0+(nstr-ntail)));
    }

    const std::string anchor(s1, s2);
    const char* tmp = strstr(s0, anchor.c_str());
    if (tmp < s0) return false;
    ret = nameMatch(tmp+anchor.size(), s2);
    while (! ret) { // retry
        tmp = strstr(tmp+1, anchor.c_str());
        if (tmp > s0) {
            ret = nameMatch(tmp+anchor.size(), s2);
        }
        else {
            break;
        }
    }
    return ret;
} // ibis::util::nameMatch

/// Converts the given time in seconds (as returned by function time) into
/// the string (as from asctime_r).  The argument @c str must contain at
/// least 26 bytes.  The new line character is turned into null.
void ibis::util::secondsToString(const time_t sec, char *str) {
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    strcpy(str, asctime(localtime(&sec)));
    str[24] = 0;
#else
    struct tm stm;
    if (localtime_r(&sec, &stm)) {
        if (asctime_r(&stm, str))
            str[24] = 0;
        else
            *str = 0;
    }
    else
        *str = 0;
#endif
} // ibis::util::secondsToString

/// The argument @c str must have 26 or more bytes and is used to
/// carry the time output.
///
/// The new line character in position 24 is turned into a null character.
/// Therefore the returned string contains only 24 characters.
void ibis::util::getLocalTime(char *str) {
    time_t sec = time(0); // current time in seconds
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    strcpy(str, asctime(localtime(&sec)));
    str[24] = 0;
#else
    struct tm stm;
    if (localtime_r(&sec, &stm)) {
        if (asctime_r(&stm, str))
            str[24] = 0;
        else
            *str = 0;
    }
    else
        *str = 0;
#endif
} // ibis::util::getLocalTime

/// The argument @c str must have 26 or more bytes and is used to
/// carry the time output.
void ibis::util::getGMTime(char *str) {
    time_t sec = time(0); // current time in seconds
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    strcpy(str, asctime(gmtime(&sec)));
    str[24] = 0;
#else
    struct tm stm;
    if (gmtime_r(&sec, &stm)) {
        if (asctime_r(&stm, str))
            str[24] = 0;
        else
            *str = 0;
    }
    else {
        *str = 0;
    }
#endif
} // ibis::util::getGMTime

void ibis::nameList::select(const char* str) {
    if (str == 0) return;
    if (*str == static_cast<char>(0)) return;

    // first put the incoming string into a list of strings
    std::set<std::string> strlist;
    const char* s = str;
    const char* t = 0;
    do {
        s += strspn(s, ibis::util::delimiters); // remove leading space
        if (*s) {
            t = strpbrk(s, ibis::util::delimiters);
            if (t) { // found a delimitor
                std::string tmp;
                while (s < t) {
                    tmp += tolower(*s);
                    ++ s;
                }
                strlist.insert(tmp);
            }
            else { // no more delimitor
                std::string tmp;
                while (*s) {
                    tmp += tolower(*s);
                    ++ s;
                }
                strlist.insert(tmp);
            }
        }
    } while (s != 0 && *s != 0);

    if (! strlist.empty()) {
        clear(); // clear existing content
        uint32_t tot = strlist.size();
        std::set<std::string>::const_iterator it;
        for (it = strlist.begin(); it != strlist.end(); ++ it)
            tot += it->size();

        buff = new char[tot];
        cstr = new char[tot];

        it = strlist.begin();
        strcpy(buff, it->c_str());
        strcpy(cstr, it->c_str());
        cvec.push_back(buff);
        char* s1 = buff + it->size();
        char* t1 = cstr + it->size();
        for (++ it; it != strlist.end(); ++ it) {
            ++ s1;
            *t1 = ',';
            ++ t1;
            strcpy(s1, it->c_str());
            strcpy(t1, it->c_str());
            cvec.push_back(s1);
            s1 += it->size();
            t1 += it->size();
        }
    }
} // ibis::nameList::select

void ibis::nameList::add(const char* str) {
    if (str == 0) return;
    if (*str == static_cast<char>(0)) return;

    // first put the incoming string into a list of strings
    std::set<std::string> strlist;
    // input existing strings
    for (unsigned i = 0; i < cvec.size(); ++ i)
        strlist.insert(cvec[i]);

    const char* s = str;
    const char* t = 0;
    do { // part new strings
        s += strspn(s, ibis::util::delimiters); // remove leading space
        if (*s) {
            t = strpbrk(s, ibis::util::delimiters);
            if (t) { // found a delimitor
                std::string tmp;
                while (s < t) {
                    tmp += tolower(*s);
                    ++ s;
                }
                strlist.insert(tmp);
            }
            else { // no more delimitor
                std::string tmp;
                while (*s) {
                    tmp += tolower(*s);
                    ++ s;
                }
                strlist.insert(tmp);
            }
        }
    } while (s != 0 && *s != 0);

    if (! strlist.empty()) {
        clear(); // clear existing content
        uint32_t tot = strlist.size();
        std::set<std::string>::const_iterator it;
        for (it = strlist.begin(); it != strlist.end(); ++ it)
            tot += it->size();

        buff = new char[tot];
        cstr = new char[tot];

        it = strlist.begin();
        strcpy(buff, it->c_str());
        strcpy(cstr, it->c_str());
        cvec.push_back(buff);
        char* s1 = buff + it->size();
        char* t1 = cstr + it->size();
        for (++ it; it != strlist.end(); ++ it) {
            ++ s1;
            *t1 = ',';
            ++ t1;
            strcpy(s1, it->c_str());
            strcpy(t1, it->c_str());
            cvec.push_back(s1);
            s1 += it->size();
            t1 += it->size();
        }
    }
} // ibis::nameList::select

// the list of names are sorted
uint32_t ibis::nameList::find(const char* key) const {
    const uint32_t sz = cvec.size();
    if (sz < 8) { // linear search
        for (uint32_t i = 0; i < sz; ++ i) {
            int tmp = stricmp(cvec[i], key);
            if (tmp == 0) {
                return i;
            }
            else if (tmp > 0) {
                return sz;
            }
        }
    }
    else { // binary search
        uint32_t i = 0;
        uint32_t j = sz;
        uint32_t k = (i + j) / 2;
        while (i < k) {
            int tmp = stricmp(cvec[k], key);
            if (tmp == 0) { // found a match
                return k;
            }
            else if (tmp < 0) {
                i = k + 1;
                k = (k + 1 + j) / 2;
            }
            else {
                j = k;
                k = (i + k) / 2;
            }
        }
        if (i < j) {
            if (0 == stricmp(cvec[i], key))
                return i;
        }
    }
    return sz;
} // ibis::nameList::find


#ifdef IBIS_REPLACEMENT_RWLOCK
//
// Adapoted from A. Sim's implementation called qthread
//
int pthread_rwlock_init(pthread_rwlock_t *rwp, void*) {
    pthread_mutex_init(&rwp->lock, 0);
    pthread_cond_init(&rwp->readers, 0);
    pthread_cond_init(&rwp->writers, 0);

    rwp->state = 0;
    rwp->waiters = 0;
    return(0);
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwp) {
    pthread_mutex_destroy(&rwp->lock);
    pthread_cond_destroy(&rwp->readers);
    pthread_cond_destroy(&rwp->writers);

    return(0);
}

void pthread_mutex_lock_cleanup(void* arg) {
    pthread_mutex_t* lock = (pthread_mutex_t *) arg;
    pthread_mutex_unlock(lock);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwp) {
    pthread_mutex_t     *lkp    = &rwp->lock;

    pthread_mutex_lock(lkp);
    pthread_cleanup_push(pthread_mutex_lock_cleanup, lkp);
    // active or queued writers 
    while ((rwp->state < 0) || rwp->waiters)
        pthread_cond_wait(&rwp->readers, lkp);

    rwp->state++;
    pthread_cleanup_pop(1);
    return(0);
}

int pthread_rw_tryrdlock(pthread_rwlock_t *rwp) {
    int status = EBUSY;

    pthread_mutex_lock(&rwp->lock);
    // available and no writers queued
    if ((rwp->state >= 0) && !rwp->waiters) {
        rwp->state++;
        status = 0;
    }

    pthread_mutex_unlock(&rwp->lock);
    return(status);
}

void pthread_rwlock_wrlock_cleanup(void *arg) {
    pthread_rwlock_t    *rwp    = (pthread_rwlock_t *) arg;

    //
    // Was the only queued writer and lock is available for readers.
    // Called through cancellation clean-up so lock is held at entry.
    //
    if ((--rwp->waiters == 0) && (rwp->state >= 0))
        pthread_cond_broadcast(&rwp->readers);

    pthread_mutex_unlock(&rwp->lock);

    return;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwp) {
    pthread_mutex_t     *lkp    = &rwp->lock;

    pthread_mutex_lock(lkp);
    rwp->waiters++;             // another writer queued        
    pthread_cleanup_push(pthread_rwlock_wrlock_cleanup, rwp);

    while (rwp->state)
        pthread_cond_wait(&rwp->writers, lkp);

    rwp->state  = -1;
    pthread_cleanup_pop(1);
    return(0);
}

int pthread_rw_trywrlock(pthread_rwlock_t *rwp) {
    int    status  = EBUSY;

    pthread_mutex_lock(&rwp->lock);
    // no readers, no writers, no writers queued
    if (!rwp->state && !rwp->waiters) {
        rwp->state = -1;
        status     = 0;
    }

    pthread_mutex_unlock(&rwp->lock);
    return(status);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwp) {
    pthread_mutex_lock(&rwp->lock);

    if (rwp->state == -1) {     // writer releasing
        rwp->state = 0;         // mark as available

        if (rwp->waiters)               // writers queued
            pthread_cond_signal(&rwp->writers);
        else
            pthread_cond_broadcast(&rwp->readers);
    }
    else {
        if (--rwp->state == 0)  // no more readers
            pthread_cond_signal(&rwp->writers);
    }
    pthread_mutex_unlock(&rwp->lock);

    return(0);
}
#endif // IBIS_REPLACEMENT_RWLOCK

#if defined(WIN32) && ! defined(__CYGWIN__)
#include <conio.h>
char* ibis::util::getpass_r(const char *prompt, char *buff, uint32_t buflen) {
    uint32_t i;
    printf("%s ", prompt);

    for(i = 0; i < buflen; ++ i) {
        buff[i] = getch();
        if ( buff[i] == '\r' ) { /* return key */
            buff[i] = 0;
            break;
        }
        else {
            if ( buff[i] == '\b')       /* backspace */
                i = i - (i>=1?2:1);
        }
    }

    if (i==buflen)    /* terminate buff */
        buff[buflen-1] = 0;

    return buff;
} // getpass_r

char* getpass(const char *prompt) {
    static char buf[256];
    ibis::util::mutexLock lock(&ibis::util::envLock, "util::getpass");
    return ibis::util::getpass_r(prompt, buf, 256);
} // getpass
#endif

#if !defined(__unix__) && defined(_WIN32)
// truncate the named file to specified bytes
int truncate(const char* name, uint32_t bytes) {
    int ierr = 0;
    int fh = _open(name, _O_RDWR | _O_CREAT, _S_IREAD | _S_IWRITE );
    if (fh >= 0) { // open successful
        LOGGER(ibis::gVerbose > 3)
            << "file \"" << name
            << "\" length before truncation is " << _filelength(fh);
        ierr = _chsize(fh, bytes);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "ERROR *** _chsize(" << name << ", " << bytes
                << ") returned " << ierr;
        }
        else {
            LOGGER(ibis::gVerbose > 3)
                << "file \"" << name
                << "\" length after truncation is " << _filelength(fh);
        }
        _close(fh);
    }
    else {
        ierr = INT_MIN;
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- function truncate failed to open file \""
            << name << "\"";
    }
    return ierr;
}
#endif
