/*
   Copyright (C) 2000-2007 MySQL AB
   Use is subject to license terms

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


/* mySTL memory implements auto_ptr
 *
 */

#ifndef mySTL_MEMORY_HPP
#define mySTL_MEMORY_HPP

#include "memory_array.hpp"   // for auto_array

#ifdef _MSC_VER
    // disable operator-> warning for builtins
    #pragma warning(disable:4284)
#endif


namespace mySTL {


template<typename T>
struct auto_ptr_ref {
    T* ptr_;
    explicit auto_ptr_ref(T* p) : ptr_(p) {}
};


template<typename T>
class auto_ptr {
    T*       ptr_;

    void Destroy()
    {
        #ifdef YASSL_LIB
            yaSSL::ysDelete(ptr_);
        #else
            TaoCrypt::tcDelete(ptr_);
        #endif
    }
public:
    explicit auto_ptr(T* p = 0) : ptr_(p) {}

    ~auto_ptr() 
    {
        Destroy();
    }


    auto_ptr(auto_ptr& other) : ptr_(other.release()) {}

    auto_ptr& operator=(auto_ptr& that)
    {
        if (this != &that) {
            Destroy();
            ptr_ = that.release();
        }
        return *this;
    }


    T* operator->() const
    {
        return ptr_;
    }

    T& operator*() const
    {
        return *ptr_;
    }

    T* get() const 
    { 
        return ptr_; 
    }

    T* release()
    {
        T* tmp = ptr_;
        ptr_ = 0;
        return tmp;
    }

    void reset(T* p = 0)
    {
        if (ptr_ != p) {
            Destroy();
            ptr_ = p;
        }
    }

    // auto_ptr_ref conversions
    auto_ptr(auto_ptr_ref<T> ref) : ptr_(ref.ptr_) {}

    auto_ptr& operator=(auto_ptr_ref<T> ref)
    {
        if (this->ptr_ != ref.ptr_) {
            Destroy();
            ptr_ = ref.ptr_;
        }
        return *this;
    }

    template<typename T2>
    operator auto_ptr<T2>()
    {
        return auto_ptr<T2>(this->release());
    }

    template<typename T2>
    operator auto_ptr_ref<T2>()
    {
        return auto_ptr_ref<T2>(this->release());
    }
};


} // namespace mySTL

#endif // mySTL_MEMORY_HPP
