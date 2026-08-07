//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_OPS_HPP
#define BOOST_COBALT_OPS_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/detail/handler.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/result.hpp>


#include <optional>

namespace boost::cobalt::io
{

struct BOOST_COBALT_IO_DECL write_op final : op<system::error_code, std::size_t>
{
  const_buffer_sequence buffer;

  using     implementation_t = void(void*, const_buffer_sequence, completion_handler<system::error_code, std::size_t>);
  using try_implementation_t = void(void*, const_buffer_sequence,            handler<system::error_code, std::size_t>);

  BOOST_COBALT_MSVC_NOINLINE
  write_op(const_buffer_sequence buffer,
           void * this_,
           implementation_t *implementation,
           try_implementation_t * try_implementation = nullptr)
            :  buffer(buffer), this_(this_),
               implementation_(implementation),
               try_implementation_(try_implementation)
  {}


  void initiate(completion_handler<system::error_code, std::size_t> handler) final
  {
    implementation_(this_, buffer, std::move(handler));
  }

  void ready(handler<system::error_code, std::size_t> handler) final
  {
    if (try_implementation_)
        try_implementation_(this_, buffer, std::move(handler));
  }
  ~write_op() = default;

 private:
  void *this_;
  implementation_t *implementation_;
  try_implementation_t * try_implementation_;
};


struct BOOST_COBALT_IO_DECL read_op final : op<system::error_code, std::size_t>
{
  mutable_buffer_sequence buffer;

  using     implementation_t = void(void*, mutable_buffer_sequence, completion_handler<system::error_code, std::size_t>);
  using try_implementation_t = void(void*, mutable_buffer_sequence,            handler<system::error_code, std::size_t>);

  BOOST_COBALT_MSVC_NOINLINE
  read_op(mutable_buffer_sequence buffer,
           void * this_,
           implementation_t *implementation,
           try_implementation_t * try_implementation = nullptr)
      :  buffer(buffer), this_(this_),
         implementation_(implementation),
         try_implementation_(try_implementation)
  {}


  void initiate(completion_handler<system::error_code, std::size_t> handler) final
  {
    implementation_(this_, buffer, std::move(handler));
  }

  void ready(handler<system::error_code, std::size_t> handler) final
  {
    if (try_implementation_)
        try_implementation_(this_, buffer, std::move(handler));
  }
  ~read_op() = default;

 private:
  void *this_;
  implementation_t *implementation_;
  try_implementation_t * try_implementation_;
};


struct BOOST_COBALT_IO_DECL write_at_op final : op<system::error_code, std::size_t>
{
  std::uint64_t offset;
  const_buffer_sequence buffer;

  using     implementation_t = void(void*, std::uint64_t, const_buffer_sequence, completion_handler<system::error_code, std::size_t>);
  using try_implementation_t = void(void*, std::uint64_t, const_buffer_sequence,            handler<system::error_code, std::size_t>);

  BOOST_COBALT_MSVC_NOINLINE
  write_at_op(std::uint64_t offset,
           const_buffer_sequence buffer,
           void * this_,
           implementation_t *implementation,
           try_implementation_t * try_implementation = nullptr)
      :  offset(offset), buffer(buffer), this_(this_),
         implementation_(implementation),
         try_implementation_(try_implementation)
  {}


  void initiate(completion_handler<system::error_code, std::size_t> handler) final
  {
    implementation_(this_, offset, buffer, std::move(handler));
  }

  void ready(handler<system::error_code, std::size_t> handler) final
  {
    if (try_implementation_)
        try_implementation_(this_, offset, buffer, std::move(handler));
  }
  ~write_at_op() = default;
 private:
  void *this_;
  implementation_t *implementation_;
  try_implementation_t * try_implementation_;
};


struct BOOST_COBALT_IO_DECL read_at_op final : op<system::error_code, std::size_t>
{
  std::uint64_t offset;
  mutable_buffer_sequence buffer;

  using     implementation_t = void(void*, std::uint64_t, mutable_buffer_sequence, completion_handler<system::error_code, std::size_t>);
  using try_implementation_t = void(void*, std::uint64_t, mutable_buffer_sequence,            handler<system::error_code, std::size_t>);

  BOOST_COBALT_MSVC_NOINLINE
  read_at_op(std::uint64_t offset,
          mutable_buffer_sequence buffer,
          void * this_,
          implementation_t *implementation,
          try_implementation_t * try_implementation = nullptr)
      :  offset(offset), buffer(buffer), this_(this_),
         implementation_(implementation),
         try_implementation_(try_implementation)
  {}


  void initiate(completion_handler<system::error_code, std::size_t> handler) final
  {
    implementation_(this_, offset, buffer, std::move(handler));
  }

  void ready(handler<system::error_code, std::size_t> handler) final
  {
    if (try_implementation_)
        try_implementation_(this_, offset, buffer, std::move(handler));
  }
  ~read_at_op() = default;
 private:
  void *this_;
  implementation_t *implementation_;
  try_implementation_t * try_implementation_;
};


struct BOOST_COBALT_IO_DECL wait_op final : op<system::error_code>
{
  using     implementation_t = void(void*, completion_handler<system::error_code>);
  using try_implementation_t = void(void*,            handler<system::error_code>);

  BOOST_COBALT_MSVC_NOINLINE
  wait_op(void * this_,
          implementation_t *implementation,
          try_implementation_t * try_implementation = nullptr)
      :  this_(this_),
         implementation_(implementation),
         try_implementation_(try_implementation)
  {}


  void initiate(completion_handler<system::error_code> handler) final
  {
    implementation_(this_, std::move(handler));
  }

  void ready(handler<system::error_code> handler) final
  {
    if (try_implementation_)
      try_implementation_(this_, std::move(handler));
  }
  ~wait_op() = default;

 private:
  void *this_;
  implementation_t *implementation_;
  try_implementation_t * try_implementation_;
};


}

#endif //BOOST_COBALT_OPS_HPP
