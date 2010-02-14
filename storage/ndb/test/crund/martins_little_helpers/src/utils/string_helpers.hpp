/*
 * string_helpers.hpp
 *
 */

#ifndef string_helpers_hpp
#define string_helpers_hpp

//#include <cstdio>
#include <iostream>
#include <string>
#include <set>

namespace utils {
    
//using namespace std;
//using std::cout;
//using std::endl;
//using std::flush;
using std::string;
using std::wstring;
using std::ostream;
using std::set;

/************************************************************
 * String Helper Functions
 ************************************************************/

/**
 * Returns the boolean value of a wide character string.
 *
 * Returns true if the argument is equal, ignoring case, to the string
 * "true"; otherwise, false.
 */
inline bool
toBool(const wstring& ws)
{
    // can't get manipulators to compile
    //bool r;
    //wistringstream(ws) >> ios_base::boolalpha >> r;
    //wistringstream(ws) >> setiosflags(ios_base::boolalpha) >> r;
    // but even if compiling, this seems to return true for empty strings:
    //wistringstream wss(ws);
    //wss.flags(ios_base::boolalpha);
    //wss >> r;

    // this works but isn't worth the overhead:
    //#include <cctype>
    //wstring t;
    //std::transform(ws.begin(), ws.end(), t.begin(),
    //               static_cast< int (*)(int) >(std::tolower));

    // short & simple
    return  ((ws.length() == 4)
             && (ws[0] == L'T' || ws[0] == L't')
             && (ws[1] == L'R' || ws[1] == L'r')
             && (ws[2] == L'U' || ws[2] == L'u')
             && (ws[3] == L'E' || ws[3] == L'e'));
}

/**
 * Returns the character representation of an int.
 */
inline string
toString(int i)
{
    std::ostringstream o;
    o << i;
    return o.str();
}

/**
 * Returns a multi-byte representation of a wide-character string.
 * 
 * XXX document semantics since this conversion ignores any
 * character set encodings...
 * This function is not very efficient in that it involves multiple
 * copying operations.
 */
inline string
toString(const wstring& ws) 
{
    //sprintf(charptr,"%ls",wsdtring.c_str()); 
    string s(ws.begin(), ws.end());
    return s;
}

/**
 * Inserts all elements of a set s into the output stream os.
 */
/*
// neither matches operator<< in: set<string> s ; cout << s;

inline ostream &
operator<< (ostream & os, const set< string >& s)
{
    os << "{";
    set< string >::iterator i = s.begin();
    if (i != s.end()) {
        os << *i;
        while (++i != s.end())
            os << "," << *i;
    }
    os << "}";
    return os;
}    

template< typename T >
inline ostream &
operator<< (ostream& os, const set< T >& s)
{
    typename set< T >::iterator i = s.begin();
...
}    

*/

/**
 * Returns a string representation of all elements in the set.
 * 
 * This function is not very efficient in that it involves multiple
 * copying operations.
 */
inline string
toString(const set< string >& s)
{
    string r;
    r += "{";
    set< string >::iterator i = s.begin();
    if (i != s.end()) {
        r += "\"";
        r += *i;
        r += "\"";
        while (++i != s.end()) {
            r += ",\"";
            r += *i;
            r += "\"";
        }
    }
    r += "}";
    return r;
}    

} // utils

#endif // string_helpers_hpp
