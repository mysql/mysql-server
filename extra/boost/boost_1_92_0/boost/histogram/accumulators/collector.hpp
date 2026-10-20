// Copyright 2024 Ruggero Turra, Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_COLLECTOR_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_COLLECTOR_HPP

#include <algorithm> // for std::equal
#include <boost/core/nvp.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/fwd.hpp> // for collector<>
#include <initializer_list>
#include <type_traits>

namespace boost {
namespace histogram {
namespace accumulators {

/** Collects samples.

  Input samples are stored in an internal container for later retrival, which stores the
  values consecutively in memory. The interface is designed to work with std::vector and
  other containers which implement the same API.

  Warning: The memory of the accumulator is unbounded.
*/
template <class ContainerType>
class collector {
public:
  using container_type = ContainerType;
  using value_type = typename container_type::value_type;
  using allocator_type = typename container_type::allocator_type;
  using const_reference = typename container_type::const_reference;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  using size_type = typename container_type::size_type;
  using const_pointer = typename container_type::const_pointer;

  // make template only match if forwarding args to container is valid
  template <typename... Args, class = decltype(container_type(std::declval<Args>()...))>
  explicit collector(Args&&... args) : container_(std::forward<Args>(args)...) {}

  // make template only match if forwarding args to container is valid
  template <class T, typename... Args, class = decltype(container_type(std::initializer_list<T>(),std::declval<Args>()...))>
  explicit collector(std::initializer_list<T> list, Args&&... args)
      : container_(list, std::forward<Args>(args)...) {}

  /// Append sample x.
  void operator()(const_reference x) { container_.push_back(x); }

  /// Append samples from another collector.
  template <class C>
  collector& operator+=(const collector<C>& rhs) {
    container_.reserve(size() + rhs.size());
    container_.insert(end(), rhs.begin(), rhs.end());
    return *this;
  }

  /// Return true if collections are equal.
  ///
  /// Two collections are equal if they have the same number of elements
  /// which all compare equal.
  template <class Iterable, class = detail::is_iterable<Iterable>>
  bool operator==(const Iterable& rhs) const noexcept {
    return std::equal(begin(), end(), rhs.begin(), rhs.end());
  }

  /// Return true if collections are not equal.
  template <class Iterable, class = detail::is_iterable<Iterable>>
  bool operator!=(const Iterable& rhs) const noexcept {
    return !operator==(rhs);
  }

  /// Return number of samples.
  size_type size() const noexcept { return container_.size(); }

  /// Return number of samples (alias for size()).
  size_type count() const noexcept { return container_.size(); }

  /// Return readonly iterator to start of collection.
  const const_iterator begin() const noexcept { return container_.begin(); }

  /// Return readonly iterator to end of collection.
  const const_iterator end() const noexcept { return container_.end(); }

  /// Return const reference to value at index.
  const_reference operator[](size_type idx) const noexcept { return container_[idx]; }

  /// Return pointer to internal memory.
  const_pointer data() const noexcept { return container_.data(); }

  allocator_type get_allocator() const { return container_.get_allocator(); }

  template <class Archive>
  void serialize(Archive& ar, unsigned version) {
    (void)version;
    ar& make_nvp("container", container_);
  }

private:
  container_type container_;
};

} // namespace accumulators
} // namespace histogram
} // namespace boost

#endif