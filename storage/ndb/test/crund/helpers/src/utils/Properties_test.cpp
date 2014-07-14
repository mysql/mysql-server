/*
   Copyright (c) 2010, 2014 Oracle and/or its affiliates. All rights reserved.

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
 * Properties.cpp
 *
 */

#include <sstream>
#include <cassert>
#include <cstdlib>

#include "Properties.hpp"

using std::cout;
using std::wcout;
using std::endl;
using std::string;
using std::wstring;
using std::stringbuf;

using utils::Properties;

//template <class C, size_t N> size_t dim_array (C (&) [N]) { return N; }
// usage: int n = dim_array(array); // == sizeof(array)/sizeof(*array)

void test()
{
    cout << "--> test()" << endl;

    // test objects
    Properties p;
    const wstring k(L"key");
    const wstring v(L"value");
    const wstring w(L"");

    // test comments, empty lines
    const char* kv0[] = {
        "", "\n", "\r", "\n\n", "\n\r", "\r\n", "\r\r", "\r\n\r", "\n\r\n",
        "#", "#k", "#\n", "#\\", "#\\\\", "#\\\n", "#\\\n\n",
        "!", "!k", "!\n", "!\\", "!\\\\", "!\\\n", "!\\\n\n",
        " #k", "\f#k", "\t#k",
        "  #k", "\f #k", "\t #k",
        "#kkk", "#kkk vvv", "#kkk= vvv", "#kkk: vvv",
        "# kkk", "# kkk vvv", "# kkk= vvv", "# kkk: vvv",
    };
    const int n0 = sizeof(kv0)/sizeof(*kv0);
    for (int i = 0; i < n0; ++i ) {
        //cout << "[" << i << "] " << "'" << kv0[i] << "'" << endl;
        stringbuf sb(kv0[i]);
        p.load(sb);
        assert (p.size() == 0);
    }

    // test non-empty key, value pairs
    const char* kv1[] = {
        "key=value", "key=value\n", "key=value\r", "key=value\r\n",
        "key:value", "key=value\n", "key=value\r", "key=value\r\n",
        "key value", "key\fvalue", "key\tvalue", "key value\n",
        " key=value", "\fkey=value", "\tkey=value",
        "key =value", "key\f=value", "key\t=value",
        " key =value", "\fkey\f=value", "\tkey\t=value",
        "key= value", "key=\fvalue", "key=\tvalue",
        "key=  value", "key  value", "key   value", "key \t \fvalue",
    };
    const int n1 = sizeof(kv1)/sizeof(*kv1);
    for (int i = 0; i < n1; ++i ) {
        //cout << "[" << i << "] " << "'" << kv1[i] << "'" << endl;
        stringbuf sb(kv1[i]);
        p.load(sb);
        assert (p.size() == 1);
        Properties::const_iterator e = p.begin();
        //wcout << e->first << " => " << e->second << endl;
        (void)e;
        assert (e->first == k);
        assert (e->second == v);
        p.clear();
    }

    // test single keys (empty values)
    const char* kv2[] = {
        "key", "key ", " key ", "key\n", " \fkey \t\n",
        "key=", "key =", "key= ", "key = ", "key=\n", " \fkey \t= \f\r\n",
    };
    const int n2 = sizeof(kv2)/sizeof(*kv2);
    for (int i = 0; i < n2; ++i ) {
        //cout << "[" << i << "] " << "'" << kv2[i] << "'" << endl;
        stringbuf sb(kv2[i]);
        p.load(sb);
        assert (p.size() == 1);
        Properties::const_iterator e = p.begin();
        //wcout << e->first << " => " << e->second << endl;
        (void)e;
        assert (e->first == k);
        assert (e->second == w);
        p.clear();
    }

    // test escape sequences
    const char* kv3[] = {
        "key=value", "key=value\\\n", "key=value\\\r", "key=value\\\r\n",
        "k\\\ney=va\\\nlue", "k\\\rey=va\\\rlue", "k\\\r\ney=va\\\r\nlue",
        "k\\\n ey=va\\\n lue", "k\\\r ey=va\\\r lue", "k\\\r\n ey=va\\\r\n lue",
        "k\\\n\\\ney=va\\\n\\\nlue", "k\\\r\\\ney=va\\\r\\\nlue",
        "k\\\n \\\n  ey=va\\\n \\\n  lue", "k\\\r \\\n  ey=va\\\r \\\n  lue",
        "k\\ey=va\\lue",
        "\\u006b\\u0065\\u0079=\\u0076\\u0061\\u006C\\u0075\\u0065",
    };
    const int n3 = sizeof(kv3)/sizeof(*kv3);
    for (int i = 0; i < n3; ++i ) {
        //cout << "[" << i << "] " << "'" << kv3[i] << "'" << endl;
        stringbuf sb(kv3[i]);
        p.load(sb);
        assert (p.size() == 1);
        Properties::const_iterator e = p.begin();
        //wcout << e->first << " => " << e->second << endl;
        (void)e;
        assert (e->first == k);
        assert (e->second == v);
        p.clear();
    }

    // test store
    const char* kv4 = ("\\ a\\ key\\ =\\ a value \n"
                       "key\\!=value\\!\n"
                       "key\\#=value\\#\n"
                       "key0=value0\n" "key1=value1\n"
                       "key2=\n" "key3=\n"
                       "key\\:=value\\:\n"
                       "key\\==value\\=\n");
    {
        //cout << "kv4='" << endl << kv4 << "'" << endl;
        stringbuf ib(kv4);
        p.load(ib);
        //cout << "p={" << endl << p << "}" << endl;

        stringbuf ob;
        p.store(ob);
        //cout << "ib='" << ib.str() << "'" << endl;
        //cout << "ob='" << ob.str() << "'" << endl;
        assert (ib.str() == ob.str());

        Properties q;
        q.load(ob);
        //cout << "q={" << endl << q << "}" << endl;
        assert (p == q);

        p.clear();
    }

    cout << "all tests passed." << endl;
    cout << "<-- test()" << endl;
}

void
exitUsage()
{
    cout << "usage: [options]" << endl
         << "    -p <file name>         properties file name" << endl
         << "    -h|--help              print usage message and exit" << endl
         << endl;
    exit(EXIT_FAILURE);
}

int
main(int argc, const char* argv[])
{
    cout << "--> main()" << endl;

    test();

    // parse and print a property file
    if (argc > 1) {
        const char* fn = NULL;
        const string arg = argv[1];
        if (arg.compare("-p") == 0) {
            if (argc < 3) {
                exitUsage();
            }
            fn = argv[2];
        } else if (arg.compare("-h") == 0 || arg.compare("--help") == 0) {
            exitUsage();
        } else {
            cout << "unknown option: " << arg << endl;
            exitUsage();
        }

        if (fn != NULL) {
            Properties p;
            cout << "read: " << fn << endl;
            p.load(fn);
            
            cout << "print:" << endl;
            wstring h(L"this header string passed to store() should be first");
            p.store(cout, &h);
        }
    }
    
    cout << "<-- main()" << endl;
    return 0;
}
