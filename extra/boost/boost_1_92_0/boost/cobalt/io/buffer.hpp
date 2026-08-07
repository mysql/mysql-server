//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_BUFFER_HPP
#define BOOST_COBALT_IO_BUFFER_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/cobalt/config.hpp>
#include <span>

namespace boost::cobalt::io
{

using asio::buffer;
using asio::mutable_buffer;


struct BOOST_COBALT_IO_DECL mutable_buffer_sequence
{
  std::size_t buffer_count() const {return tail_.size() + 1u;}

#if defined(BOOST_ASIO_HAS_IO_URING)
  mutable_buffer_sequence(asio::mutable_registered_buffer buffer = {}) : registered_(buffer) { }
  mutable_buffer_sequence(asio::mutable_buffer head)  : registered_{}
  {
    this->head_ = head;
  }
  mutable_buffer_sequence(const mutable_buffer_sequence & rhs) noexcept : registered_(rhs.registered_), tail_(rhs.tail_) {}
#else
  mutable_buffer_sequence(const asio::mutable_registered_buffer & buffer = {}) : head_(buffer.buffer()) { }
  mutable_buffer_sequence(const asio::mutable_buffer & head)  : head_{head} { }
  mutable_buffer_sequence(const mutable_buffer_sequence & rhs) noexcept : head_(rhs.head_), tail_(rhs.tail_) {}
#endif

  mutable_buffer_sequence& operator=(const mutable_buffer_sequence & rhs)
  {
#if defined(BOOST_ASIO_HAS_IO_URING)
    registered_ = rhs.registered_;
#else
    head_ = rhs.head_;
#endif
    tail_ = rhs.tail_;
    return *this;
  }
  ~mutable_buffer_sequence() {}

  template<typename T>
    requires (std::constructible_from<std::span<const asio::mutable_buffer>, const T&>)
  mutable_buffer_sequence(const T & value)
#if defined(BOOST_ASIO_HAS_IO_URING)
        : registered_{}
#endif
  {
    std::span<const asio::mutable_buffer> spn(value);
    if (!spn.empty())
    {
      head_ = spn.front();
      tail_ = spn.subspan(1u);
    }
  }

  mutable_buffer_sequence(std::span<const asio::mutable_buffer> spn)
#if defined(BOOST_ASIO_HAS_IO_URING)
        : registered_{}
#endif
  {
    if (!spn.empty())
    {
      head_ = spn.front();
      tail_ = spn.subspan(1u);
    }
  }

  mutable_buffer_sequence & operator+=(std::size_t n)
  {
    if (n < head_.size())
      head_ += n;
    else
    {
      n -= head_.size();
      std::size_t idx = 0u;
      while (idx < tail_.size() && n > tail_[idx].size() )
        n -= tail_[idx++].size();

      if (idx == tail_.size()) // we're exceeding the size, so just drop everything
      {
        tail_ = {};
        head_ = {};
      }
      if (tail_.empty())
        head_ = {};
      else
      {
        head_ = tail_[idx];
        head_ += n;
        tail_ = tail_.subspan(idx + 1);
      }
    }
    return *this;
  }

  struct const_iterator
  {
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = asio::mutable_buffer;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const asio::mutable_buffer*;
    using reference         = const asio::mutable_buffer&;

    BOOST_COBALT_MSVC_NOINLINE
    const_iterator(asio::mutable_buffer head,
                   std::span<const asio::mutable_buffer> tail,
                   std::size_t offset = std::numeric_limits<std::size_t>::max()) : head_(head), tail_(tail), offset_(offset) {}

    BOOST_COBALT_MSVC_NOINLINE
    reference & operator*() const
    {
      return offset_ == std::numeric_limits<std::size_t>::max() ? head_ : tail_[offset_];
    }

    BOOST_COBALT_MSVC_NOINLINE
    pointer operator->() const
    {
      return offset_ == std::numeric_limits<std::size_t>::max() ? &head_ : &tail_[offset_];
    }

    const_iterator operator++()
    {
      offset_++;
      return *this;
    }

    const_iterator operator++(int)
    {
      auto o = *this;
      offset_ ++ ;
      return o;
    }

    const_iterator operator--()
    {
      offset_ --;
      return *this;
    }

    const_iterator operator--(int)
    {
      auto o = *this;
      offset_ -- ;
      return o;
    }

    const_iterator operator+(difference_type diff) const
    {
      auto res = *this;
      res.offset_ += diff;
      return res;
    }

    const_iterator operator-(difference_type diff) const
    {
      auto res = *this;
      res.offset_ -= diff;
      return res;
    }

    const_iterator& operator+=(difference_type diff)
    {
      offset_ += diff;
      return *this;
    }

    const_iterator operator-=(difference_type diff)
    {
      offset_ -= diff;
      return *this;
    }

    reference operator[](difference_type n) const
    {
      auto idx = offset_ + n;
      return idx == std::numeric_limits<std::size_t>::max() ? head_ : tail_[idx];
    }

    difference_type operator-(const_iterator itr) const
    {
      return static_cast<difference_type>(offset_) - static_cast<difference_type>(itr.offset_);
    }

    friend auto operator<=>(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
         <=> std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    };

    friend bool operator==(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
          == std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    }

    friend bool operator!=(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
          != std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    }
   private:

    asio::mutable_buffer head_;
    std::span<const asio::mutable_buffer> tail_;

    std::size_t offset_{std::numeric_limits<std::size_t>::max()};
  };

  BOOST_COBALT_MSVC_NOINLINE
  const_iterator begin() const {return const_iterator{head_, tail_};}
  BOOST_COBALT_MSVC_NOINLINE
  const_iterator   end() const {return const_iterator{head_, tail_, tail_.size()};}

  bool is_registered() const
  {
#if defined(BOOST_ASIO_HAS_IO_URING)
    return registered_.id() != asio::registered_buffer_id();
#else
    return false;
#endif
  }

  template<typename Func>
  friend auto visit(const mutable_buffer_sequence & seq, Func && func)
  {
    if (seq.buffer_count() > 1u)
      return std::forward<Func>(func)(seq);
#if defined(BOOST_ASIO_HAS_IO_URING)
    else if (seq.is_registered())
      return std::forward<Func>(func)(seq.registered_);
#endif
    else
      return std::forward<Func>(func)(seq.head_);
  }
 private:
#if defined(BOOST_ASIO_HAS_IO_URING)
  union {
    asio::mutable_registered_buffer registered_{};
    asio::mutable_buffer head_;
  };
#else

  asio::mutable_buffer head_;
#endif
  std::span<const asio::mutable_buffer> tail_;
};


using asio::const_buffer;

struct BOOST_COBALT_IO_DECL const_buffer_sequence
{
  std::size_t buffer_count() const {return tail_.size() + 1u;}

#if defined(BOOST_ASIO_HAS_IO_URING)
  const_buffer_sequence(asio::const_registered_buffer buffer = {}) : registered_(buffer) {}
  const_buffer_sequence(asio::mutable_registered_buffer buffer)    : registered_(buffer) {}
  const_buffer_sequence(asio::const_buffer head)   : registered_{} { this->head_ = head; }
  const_buffer_sequence(asio::mutable_buffer head) : registered_{} { this->head_ = head; }
  const_buffer_sequence(const const_buffer_sequence & rhs) : registered_(rhs.registered_), tail_(rhs.tail_) {}
#else

  const_buffer_sequence(const asio::const_registered_buffer &buffer = {}) : head_(buffer.buffer()) {}
  const_buffer_sequence(const asio::mutable_registered_buffer &buffer)    : head_(buffer.buffer()) {}
  const_buffer_sequence(const asio::const_buffer &head)   : head_{ head} { }
  const_buffer_sequence(const asio::mutable_buffer &head) : head_{ head} { }
  const_buffer_sequence(const const_buffer_sequence & rhs) : head_(rhs.head_), tail_(rhs.tail_) {}
#endif
  template<typename T>
    requires (std::constructible_from<std::span<const asio::const_buffer>, const T&>)
  const_buffer_sequence(const T & value)
#if defined(BOOST_ASIO_HAS_IO_URING)
      : registered_{}
#endif
  {
    std::span<const asio::const_buffer> spn(value);
    if (!spn.empty())
    {
      head_ = spn.front();
      tail_ = spn.subspan(1u);
    }
  }

  const_buffer_sequence& operator=(const const_buffer_sequence & rhs)
  {
#if defined(BOOST_ASIO_HAS_IO_URING)
    registered_ = rhs.registered_;
#else
    head_ = rhs.head_;
#endif
    tail_ = rhs.tail_;
    return *this;
  }
  ~const_buffer_sequence() {}

  const_buffer_sequence(std::span<const asio::const_buffer> spn)
#if defined(BOOST_ASIO_HAS_IO_URING)
    : registered_{}
#endif
  {
    if (!spn.empty())
    {
      head_ = spn.front();
      tail_ = spn.subspan(1u);
    }
  }

  const_buffer_sequence & operator+=(std::size_t n)
  {
    if (n < head_.size())
      head_ += n;
    else
    {
      n -= head_.size();
      std::size_t idx = 0u;
      while (idx < tail_.size() && n > tail_[idx].size() )
        n -= tail_[idx++].size();

      if (idx == tail_.size()) // we're exceeding the size, so just drop everything
      {
        tail_ = {};
        head_ = {};
      }
      if (tail_.empty())
        head_ = {};
      else
      {
        head_ = tail_[idx];
        head_ += n;
        tail_ = tail_.subspan(idx + 1);
      }
    }
    return *this;
  }

  struct const_iterator
  {
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = asio::const_buffer;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const asio::const_buffer*;
    using reference         = const asio::const_buffer&;

    BOOST_COBALT_MSVC_NOINLINE
    const_iterator(asio::const_buffer head,
                   std::span<const asio::const_buffer> tail,
                   std::size_t offset = std::numeric_limits<std::size_t>::max()) : head_(head), tail_(tail), offset_(offset)
    {

    }

    BOOST_COBALT_MSVC_NOINLINE
    reference & operator*() const
    {
      return offset_ == std::numeric_limits<std::size_t>::max() ? head_ : tail_[offset_];
    }

    BOOST_COBALT_MSVC_NOINLINE
    pointer operator->() const
    {
      return offset_ == std::numeric_limits<std::size_t>::max() ? &head_ : &tail_[offset_];
    }

    const_iterator operator++()
    {
      offset_++;
      return *this;
    }

    const_iterator operator++(int)
    {
      auto o = *this;
      offset_ ++ ;
      return o;
    }

    const_iterator operator--()
    {
      offset_ --;
      return *this;
    }

    const_iterator operator--(int)
    {
      auto o = *this;
      offset_ -- ;
      return o;
    }

    const_iterator operator+(difference_type diff) const
    {
      auto res = *this;
      res.offset_ += diff;
      return res;
    }

    const_iterator operator-(difference_type diff) const
    {
      auto res = *this;
      res.offset_ -= diff;
      return res;
    }

    const_iterator& operator+=(difference_type diff)
    {
      offset_ += diff;
      return *this;
    }

    const_iterator operator-=(difference_type diff)
    {
      offset_ -= diff;
      return *this;
    }

    BOOST_COBALT_MSVC_NOINLINE
    reference operator[](difference_type n) const
    {
      auto idx = offset_ + n;
      return idx == std::numeric_limits<std::size_t>::max() ? head_ : tail_[idx];
    }

    difference_type operator-(const_iterator itr) const
    {
      return static_cast<difference_type>(offset_) - static_cast<difference_type>(itr.offset_);
    }

    friend auto operator<=>(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
             <=> std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    };
    friend bool operator==(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
          == std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    }
    friend bool operator!=(const const_iterator & lhs, const const_iterator & rhs)
    {
      return std::make_tuple(lhs.head_.data(), lhs.head_.size(), lhs.tail_.data(), lhs.tail_.size(), lhs.offset_)
          != std::make_tuple(rhs.head_.data(), rhs.head_.size(), rhs.tail_.data(), rhs.tail_.size(), rhs.offset_);
    }

   private:

    asio::const_buffer head_;
    std::span<const asio::const_buffer> tail_;

    std::size_t offset_{std::numeric_limits<std::size_t>::max()};

  };

  BOOST_COBALT_MSVC_NOINLINE
  const_iterator begin() const {return const_iterator{head_, tail_};}

  BOOST_COBALT_MSVC_NOINLINE
  const_iterator   end() const {return const_iterator{head_, tail_, tail_.size()};}

  bool is_registered() const
  {
#if defined(BOOST_ASIO_HAS_IO_URING)
    return registered_.id() != asio::registered_buffer_id();
#else
    return false;
#endif
  }

  template<typename Func>
  friend auto visit(const const_buffer_sequence & seq, Func && func)
  {
    if (seq.buffer_count() > 1u)
      return std::forward<Func>(func)(seq);
    // Windows doesn't support registerd buffers anyhow
#if defined(BOOST_ASIO_HAS_IO_URING)
    else if (seq.is_registered())
      return std::forward<Func>(func)(seq.registered_);
#endif
    else
      return std::forward<Func>(func)(seq.head_);
  }

 private:
#if defined(BOOST_ASIO_HAS_IO_URING)
  union {
    asio::const_registered_buffer registered_;
    asio::const_buffer head_;
  };
#else
  asio::const_buffer head_;
#endif
  std::span<const asio::const_buffer> tail_;
};

using asio::buffer_copy;
using asio::buffer_size;

}

#endif //BOOST_COBALT_IO_BUFFER_HPP
