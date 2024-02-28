/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_CNO_BUFFER_SEQUENCE_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_CNO_BUFFER_SEQUENCE_H_

#include <iterator>
#include <vector>

#include "cno/core.h"
#include "mysql/harness/net_ts/buffer.h"

namespace http {
namespace cno {

template <typename ResultType = net::const_buffer>
class Sequence {
 public:
  Sequence(const ResultType *begin, size_t size) : begin_{begin}, size_{size} {}

  const ResultType *begin() const { return begin_; }
  const ResultType *end() const { return begin_ + size_; }

 private:
  const ResultType *begin_;
  size_t size_;
};

template <typename SourceType = cno_buffer_t,
          typename ResultType = net::const_buffer>
class BufferSequence {
 public:
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = ResultType;
    using difference_type = void;
    using pointer = ResultType *;
    using reference = ResultType &;

   public:
    Iterator(const BufferSequence *parent, size_t index)
        : parent_{parent}, index_{index} {
      make_result();
    }

    void operator++() {
      ++index_;
      make_result();
    }

    bool operator!=(const Iterator &it) const { return result_ != it.result_; }
    bool operator==(const Iterator &it) const { return result_ == it.result_; }
    reference operator*() { return *result_; }
    Iterator operator+(const int value) const {
      return {parent_, index_ + value};
    }
    pointer operator->() { return result_; }

    operator value_type() { return result_; }

    size_t left() { return parent_->source_count_ - index_; }
    const SourceType *get_buffer() { return parent_->source_ + index_; }

   private:
    void make_result() {
      while (parent_->destination_initialized_count_ <= index_) {
        if (index_ >= parent_->source_count_) break;

        auto &src = parent_->source_[index_];
        parent_->destination_[index_] = {src.data, src.size};

        ++parent_->destination_initialized_count_;
      }

      result_ = &parent_->destination_[0] + index_;
    }

    const BufferSequence *parent_;
    size_t index_;
    pointer result_{nullptr};
  };

  BufferSequence(const SourceType *source, size_t source_count)
      : source_{source}, source_count_{source_count} {
    destination_.resize(source_count_);
  }

  const Iterator begin() const { return Iterator(this, 0); }
  const Iterator end() const { return Iterator(this, source_count_); }

  const SourceType *source_;
  size_t source_count_{0};
  mutable std::vector<ResultType> destination_;
  mutable size_t destination_initialized_count_{0};
};

}  // namespace cno
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_CNO_BUFFER_SEQUENCE_H_
