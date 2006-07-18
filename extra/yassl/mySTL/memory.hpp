/* mySTL memory.hpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * There are special exceptions to the terms and conditions of the GPL as it
 * is applied to yaSSL. View the full text of the exception in the file
 * FLOSS-EXCEPTIONS in the directory of this software distribution.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


/* mySTL memory implements auto_ptr
 *
 */

#ifndef mySTL_MEMORY_HPP
#define mySTL_MEMORY_HPP


#ifdef _MSC_VER
    // disable operator-> warning for builtins
    #pragma warning(disable:4284)
#endif


namespace mySTL {


template<typename T>
struct auto_ptr_ref {
    typedef void (*Deletor)(T*);
    T*      ptr_;
    Deletor del_;
    auto_ptr_ref(T* p, Deletor d) : ptr_(p), del_(d) {}
};


template<typename T>
class auto_ptr {
    typedef void (*Deletor)(T*);
    T*       ptr_;
    Deletor  del_;

    void Destroy()
    {
        del_(ptr_);
    }
public:
    auto_ptr(T* p, Deletor d) : ptr_(p), del_(d) {}

    explicit auto_ptr(Deletor d) : ptr_(0), del_(d) {}

    ~auto_ptr() 
    {
        Destroy();
    }


    auto_ptr(auto_ptr& other) : ptr_(other.release()), del_(other.del_) {}

    auto_ptr& operator=(auto_ptr& that)
    {
        if (this != &that) {
            Destroy();
            ptr_ = that.release();
            del_ = that.del_;
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
    auto_ptr(auto_ptr_ref<T> ref) : ptr_(ref.ptr_), del_(ref.del_) {}

    auto_ptr& operator=(auto_ptr_ref<T> ref)
    {
        if (this->ptr_ != ref.ptr_) {
            Destroy();
            ptr_ = ref.ptr_;
            del_ = ref.del_;
        }
        return *this;
    }

    template<typename T2>
    operator auto_ptr<T2>()
    {
        return auto_ptr<T2>(this->release(), this->del_);
    }

    template<typename T2>
    operator auto_ptr_ref<T2>()
    {
        return auto_ptr_ref<T2>(this->release(), this->del_);
    }
};


} // namespace mySTL

#endif // mySTL_MEMORY_HPP
