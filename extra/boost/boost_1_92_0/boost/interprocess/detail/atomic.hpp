//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2012
// (C) Copyright Markus Schoepflin 2007
// (C) Copyright Bryce Lelbach 2010
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_DETAIL_ATOMIC_HPP
#define BOOST_INTERPROCESS_DETAIL_ATOMIC_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/detail/workaround.hpp>
#include <boost/cstdint.hpp>

#if !defined(_AIX)
#define BOOST_INTERPROCESS_DETAIL_PPC_ASM_LABEL(label) label ":\n\t"
#define BOOST_INTERPROCESS_DETAIL_PPC_ASM_JUMP(insn, label, offset) insn " " label "\n\t"
#else
#define BOOST_INTERPROCESS_DETAIL_PPC_ASM_LABEL(label)
#define BOOST_INTERPROCESS_DETAIL_PPC_ASM_JUMP(insn, label, offset) insn " $" offset "\n\t"
#endif

namespace boost{
namespace interprocess{
namespace ipcdetail{

//! Atomically increment an boost::uint32_t by 1
//! "mem": pointer to the object
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_inc32(volatile boost::uint32_t *mem);

//! Atomically read an boost::uint32_t from memory
inline boost::uint32_t atomic_read32(volatile boost::uint32_t *mem);

//! Atomically set an boost::uint32_t in memory
//! "mem": pointer to the object
//! "param": val value that the object will assume
inline void atomic_write32(volatile boost::uint32_t *mem, boost::uint32_t val);

//! Compare an boost::uint32_t's value with "cmp".
//! If they are the same swap the value with "with"
//! "mem": pointer to the value
//! "with": what to swap it with
//! "cmp": the value to compare it to
//! Returns the old value of *mem
inline boost::uint32_t atomic_cas32
   (volatile boost::uint32_t *mem, boost::uint32_t with, boost::uint32_t cmp);

}  //namespace ipcdetail{
}  //namespace interprocess{
}  //namespace boost{

#if defined (BOOST_INTERPROCESS_WINDOWS)

#include <boost/interprocess/detail/win32_api.hpp>

#if defined( _MSC_VER )
   extern "C" void _ReadWriteBarrier(void);
   #pragma intrinsic(_ReadWriteBarrier)

#define BOOST_INTERPROCESS_READ_WRITE_BARRIER \
            BOOST_INTERPROCESS_DISABLE_DEPRECATED_WARNING \
            _ReadWriteBarrier() \
            BOOST_INTERPROCESS_RESTORE_WARNING

#elif defined(__GNUC__)
#  define BOOST_INTERPROCESS_READ_WRITE_BARRIER __sync_synchronize()
#else
#  error "Unsupported Compiler for Window"
#endif

namespace boost{
namespace interprocess{
namespace ipcdetail{

//! Atomically decrement an boost::uint32_t by 1
//! "mem": pointer to the atomic value
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_dec32(volatile boost::uint32_t *mem)
{  return (boost::uint32_t)winapi::interlocked_decrement(reinterpret_cast<volatile long*>(mem)) + 1;  }

//! Atomically increment an apr_uint32_t by 1
//! "mem": pointer to the object
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_inc32(volatile boost::uint32_t *mem)
{  return (boost::uint32_t)winapi::interlocked_increment(reinterpret_cast<volatile long*>(mem))-1;  }

//! Atomically read an boost::uint32_t from memory
inline boost::uint32_t atomic_read32(volatile boost::uint32_t *mem)
{
    const boost::uint32_t val = *mem;
    BOOST_INTERPROCESS_READ_WRITE_BARRIER;
    return val;
}

//! Atomically set an boost::uint32_t in memory
//! "mem": pointer to the object
//! "param": val value that the object will assume
inline void atomic_write32(volatile boost::uint32_t *mem, boost::uint32_t val)
{  winapi::interlocked_exchange(reinterpret_cast<volatile long*>(mem), (long)val);  }

//! Compare an boost::uint32_t's value with "cmp".
//! If they are the same swap the value with "with"
//! "mem": pointer to the value
//! "with": what to swap it with
//! "cmp": the value to compare it to
//! Returns the old value of *mem
inline boost::uint32_t atomic_cas32
   (volatile boost::uint32_t *mem, boost::uint32_t with, boost::uint32_t cmp)
{  return (boost::uint32_t)winapi::interlocked_compare_exchange(reinterpret_cast<volatile long*>(mem), (long)with, (long)cmp);  }

}  //namespace ipcdetail{
}  //namespace interprocess{
}  //namespace boost{

#elif defined(__GNUC__) && ( __GNUC__ * 100 + __GNUC_MINOR__ >= 401 )

namespace boost {
namespace interprocess {
namespace ipcdetail{

//! Atomically add 'val' to an boost::uint32_t
//! "mem": pointer to the object
//! "val": amount to add
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_add32
   (volatile boost::uint32_t *mem, boost::uint32_t val)
{  return __sync_fetch_and_add(const_cast<boost::uint32_t *>(mem), val);   }

//! Atomically increment an apr_uint32_t by 1
//! "mem": pointer to the object
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_inc32(volatile boost::uint32_t *mem)
{  return atomic_add32(mem, 1);  }

//! Atomically decrement an boost::uint32_t by 1
//! "mem": pointer to the atomic value
//! Returns the old value pointed to by mem
inline boost::uint32_t atomic_dec32(volatile boost::uint32_t *mem)
{  return atomic_add32(mem, (boost::uint32_t)-1);   }

//! Atomically read an boost::uint32_t from memory
inline boost::uint32_t atomic_read32(volatile boost::uint32_t *mem)
{  boost::uint32_t old_val = *mem; __sync_synchronize(); return old_val;  }

//! Compare an boost::uint32_t's value with "cmp".
//! If they are the same swap the value with "with"
//! "mem": pointer to the value
//! "with" what to swap it with
//! "cmp": the value to compare it to
//! Returns the old value of *mem
inline boost::uint32_t atomic_cas32
   (volatile boost::uint32_t *mem, boost::uint32_t with, boost::uint32_t cmp)
{  return __sync_val_compare_and_swap(const_cast<boost::uint32_t *>(mem), cmp, with);   }

//! Atomically set an boost::uint32_t in memory
//! "mem": pointer to the object
//! "param": val value that the object will assume
inline void atomic_write32(volatile boost::uint32_t *mem, boost::uint32_t val)
{  __sync_synchronize(); *mem = val;  }

}  //namespace ipcdetail{
}  //namespace interprocess{
}  //namespace boost{

#else

#error No atomic operations implemented for this platform, sorry!

#endif

namespace boost{
namespace interprocess{
namespace ipcdetail{

inline bool atomic_add_unless32
   (volatile boost::uint32_t *mem, boost::uint32_t value, boost::uint32_t unless_this)
{
   boost::uint32_t old, c(atomic_read32(mem));
   while(c != unless_this && (old = atomic_cas32(mem, c + value, c)) != c){
      c = old;
   }
   return c != unless_this;
}

}  //namespace ipcdetail
}  //namespace interprocess
}  //namespace boost


#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_DETAIL_ATOMIC_HPP
