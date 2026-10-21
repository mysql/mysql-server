//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_FILE_HPP
#define BOOST_COBALT_IO_FILE_HPP

#include <boost/asio/basic_file.hpp>

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/system/result.hpp>

#if !defined(BOOST_ASIO_HAS_FILE)

#include <fcntl.h>
#include <boost/asio/posix/basic_stream_descriptor.hpp>

#endif

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE file
#if defined(BOOST_ASIO_HAS_FILE)
    : asio::file_base
#endif
{

#if !defined(BOOST_ASIO_HAS_FILE)
  enum flags
  {
    read_only = O_RDONLY,
    write_only = O_WRONLY,
    read_write = O_RDWR,
    append = O_APPEND,
    create = O_CREAT,
    exclusive = O_EXCL,
    truncate = O_TRUNC,
    sync_all_on_write = O_SYNC
  };

  // Implement bitmask operations as shown in C++ Std [lib.bitmask.types].

  friend flags operator&(flags x, flags y)
  {
    return static_cast<flags>(
        static_cast<unsigned int>(x) & static_cast<unsigned int>(y));
  }

  friend flags operator|(flags x, flags y)
  {
    return static_cast<flags>(
        static_cast<unsigned int>(x) | static_cast<unsigned int>(y));
  }

  friend flags operator^(flags x, flags y)
  {
    return static_cast<flags>(
        static_cast<unsigned int>(x) ^ static_cast<unsigned int>(y));
  }

  friend flags operator~(flags x)
  {
    return static_cast<flags>(~static_cast<unsigned int>(x));
  }

  friend flags& operator&=(flags& x, flags y)
  {
    x = x & y;
    return x;
  }

  friend flags& operator|=(flags& x, flags y)
  {
    x = x | y;
    return x;
  }

  friend flags& operator^=(flags& x, flags y)
  {
    x = x ^ y;
    return x;
  }

  /// Basis for seeking in a file.
  enum seek_basis
  {
    seek_set = SEEK_SET,
    seek_cur = SEEK_CUR,
    seek_end = SEEK_END
  };
#endif

#if !defined(BOOST_ASIO_HAS_FILE)
  using native_handle_type = int;
#else
  using native_handle_type = asio::basic_file<executor>::native_handle_type;
#endif
  BOOST_COBALT_IO_DECL system::result<void> assign(const native_handle_type & native_file);
  BOOST_COBALT_IO_DECL system::result<void> cancel();

  BOOST_COBALT_IO_DECL executor get_executor();
  BOOST_COBALT_IO_DECL bool is_open() const;

  BOOST_COBALT_IO_DECL system::result<void> close();
  BOOST_COBALT_IO_DECL native_handle_type native_handle();

  BOOST_COBALT_IO_DECL system::result<void> open(const char * path,        flags open_flags);
  BOOST_COBALT_IO_DECL system::result<void> open(const std::string & path, flags open_flags);

  BOOST_COBALT_IO_DECL system::result<native_handle_type> release();
  BOOST_COBALT_IO_DECL system::result<void> resize(std::uint64_t n);

  BOOST_COBALT_IO_DECL system::result<std::uint64_t> size() const;
  BOOST_COBALT_IO_DECL system::result<void> sync_all();
  BOOST_COBALT_IO_DECL system::result<void> sync_data();

#if defined(BOOST_ASIO_HAS_FILE)
  file(asio::basic_file<executor> & file) : file_(file) {}
 private:
  asio::basic_file<executor> & file_;
#else
  explicit file(executor exec) : file_(exec) {}
  file(executor exec, int fd) : file_(exec, fd) {}
 protected:
  boost::asio::posix::basic_stream_descriptor<executor> file_;
#endif



};

}


#endif //BOOST_COBALT_IO_FILE_HPP
