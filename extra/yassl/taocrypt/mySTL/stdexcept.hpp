/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
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
