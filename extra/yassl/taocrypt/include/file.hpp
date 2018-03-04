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

/* file.hpp provies File Sources and Sinks
*/


#ifndef TAO_CRYPT_FILE_HPP
#define TAO_CRYPT_FILE_HPP

#include "misc.hpp"
#include "block.hpp"
#include "error.hpp"
#include <stdio.h>

namespace TaoCrypt {


class Source {
    ByteBlock buffer_;
    word32    current_;
    Error     error_;
public:
    explicit Source(word32 sz = 0) : buffer_(sz), current_(0) {}
    Source(const byte* b, word32 sz) : buffer_(b, sz), current_(0) {}

    word32 remaining()         { if (GetError().What()) return 0;
                                 else return buffer_.size() - current_; } 
    word32 size() const        { return buffer_.size(); }
    void   grow(word32 sz)     { buffer_.CleanGrow(sz); }

    bool IsLeft(word32 sz) { if (remaining() >= sz) return true;
                             else { SetError(CONTENT_E); return false; } }
   
    const byte*  get_buffer()  const { return buffer_.get_buffer(); }
    const byte*  get_current() const { return &buffer_[current_]; }
    word32       get_index()   const { return current_; }
    void         set_index(word32 i) { if (i < size()) current_ = i; }

    byte operator[] (word32 i) { current_ = i; return next(); }
    byte next() { if (IsLeft(1)) return buffer_[current_++]; else return 0; }
    byte prev() { if (current_)  return buffer_[--current_]; else return 0; }

    void add(const byte* data, word32 len)
    {
        if (IsLeft(len)) {
            memcpy(buffer_.get_buffer() + current_, data, len);
            current_ += len;
        }
    }

    void advance(word32 i) { if (IsLeft(i)) current_ += i; }
    void reset(ByteBlock&);

    Error  GetError()              { return error_; }
    void   SetError(ErrorNumber w) { error_.SetError(w); }

    friend class FileSource;  // for get()

    Source(const Source& that)
        : buffer_(that.buffer_), current_(that.current_) {}

    Source& operator=(const Source& that)
    {
        Source tmp(that);
        Swap(tmp);
        return *this;
    }

    void Swap(Source& other) 
    {
        buffer_.Swap(other.buffer_);
        STL::swap(current_, other.current_);
    }

};


// File Source
class FileSource {
    FILE* file_;
public:
    FileSource(const char* fname, Source& source);
    ~FileSource();
   
    word32   size(bool use_current = false);
private:
    word32   get(Source&);
    word32   size_left();                     

    FileSource(const FileSource&);            // hide
    FileSource& operator=(const FileSource&); // hide
};


// File Sink
class FileSink {
    FILE* file_;
public:
    FileSink(const char* fname, Source& source);
    ~FileSink();

    word32 size(bool use_current = false);
private:
    void put(Source&);

    FileSink(const FileSink&);            // hide
    FileSink& operator=(const FileSink&); // hide
};



} // namespace

#endif // TAO_CRYPT_FILE_HPP
