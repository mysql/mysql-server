/*
   Copyright (C) 2000-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/


/* mySTL memory implements exception, runtime_error
 *
 */

#ifndef mySTL_STDEXCEPT_HPP
#define mySTL_STDEXCEPT_HPP


#include <string.h>  // strncpy
#include <stdlib.h>  // size_t


namespace mySTL {


class exception {
public:
    exception() {}
    virtual ~exception() {}   // to shut up compiler warnings

    virtual const char* what() const { return ""; }

    // for compiler generated call, never used
    static void operator delete(void*) { }
private:
    // don't allow dynamic creation of exceptions
    static void* operator new(size_t);
};


class named_exception : public exception {
public:
    enum { NAME_SIZE = 80 };

    explicit named_exception(const char* str) 
    {
        strncpy(name_, str, NAME_SIZE);
        name_[NAME_SIZE - 1] = 0;
    }

    virtual const char* what() const { return name_; }
private:
    char name_[NAME_SIZE];
};


class runtime_error : public named_exception {
public:
    explicit runtime_error(const char* str) : named_exception(str) {}
};




} // namespace mySTL

#endif // mySTL_STDEXCEPT_HPP
