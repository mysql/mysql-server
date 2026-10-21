// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_CHUNK_VECTOR_HPP
#define BOOST_HISTOGRAM_DETAIL_CHUNK_VECTOR_HPP

#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <vector>

namespace boost {
namespace histogram {
namespace detail {

// Warning: this is not a proper container and is only used to
// test the feasibility of using accumulators::collector with a
// custom container type. If time permits, this will be expanded
// into a proper container type.
template <class ValueType>
class chunk_vector {
public:
  using base = std::vector<ValueType>;
  using allocator_type = typename base::allocator_type;
  using pointer = typename base::pointer;
  using const_pointer = typename base::const_pointer;
  using size_type = typename base::size_type;
  using const_reference = boost::span<const ValueType>;
  using reference = boost::span<ValueType>;
  // this is wrong and should make a copy; it is not a problem for
  // the current use-case, but a general purpose implementation cannot
  // violate concepts like this
  using value_type = const_reference;

  template <class Pointer>
  struct iterator_t {
    iterator_t& operator++() {
      ptr_ += chunk_;
      return *this;
    }

    iterator_t operator++(int) {
      iterator_t copy(*this);
      ptr_ += chunk_;
      return copy;
    }

    value_type operator*() const { return value_type(ptr_, ptr_ + chunk_); }

    Pointer ptr_;
    size_type chunk_;
  };

  using iterator = iterator_t<pointer>;
  using const_iterator = iterator_t<const_pointer>;

  // this creates an empty chunk_vector
  explicit chunk_vector(size_type chunk, const allocator_type& alloc = {})
      : chunk_(chunk), vec_(alloc) {}

  chunk_vector(std::initializer_list<value_type> list, size_type chunk,
               const allocator_type& alloc = {})
      : chunk_(chunk), vec_(list, alloc) {}

  allocator_type get_allocator() noexcept(noexcept(allocator_type())) {
    return vec_.get_allocator();
  }

  void push_back(const_reference x) {
    if (x.size() != chunk_)
      BOOST_THROW_EXCEPTION(std::runtime_error("argument has wrong size"));
    // we don't use std::vector::insert here to have amortized constant complexity
    for (auto&& elem : x) vec_.push_back(elem);
  }

  auto insert(const_iterator pos, const_iterator o_begin, const_iterator o_end) {
    if (std::distance(o_begin, o_end) % chunk_ == 0)
      BOOST_THROW_EXCEPTION(std::runtime_error("argument has wrong size"));
    return vec_.insert(pos, o_begin, o_end);
  }

  const_iterator begin() const noexcept { return {vec_.data(), chunk_}; }
  const_iterator end() const noexcept { return {vec_.data() + vec_.size(), chunk_}; }

  value_type operator[](size_type idx) const noexcept {
    return {vec_.data() + idx * chunk_, vec_.data() + (idx + 1) * chunk_};
  }

  size_type size() const noexcept { return vec_.size() / chunk_; }

private:
  size_type chunk_;
  base vec_;
};

} // namespace detail
} // namespace histogram
} // namespace boost

#endif