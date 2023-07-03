/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef ITERATOR_INCLUDED
#define ITERATOR_INCLUDED

#include "cache.h" /* Datacache */

namespace keyring_common {

namespace iterator {
template <typename Data_extension>
class Iterator {
 public:
  /** Constructor */
  Iterator() : it_(), end_(), version_(0), valid_(false), cached_(false) {}
  Iterator(const cache::Datacache<Data_extension> &datacache,
           const meta::Metadata &metadata)
      : it_(datacache.at(metadata)),
        end_(datacache.end()),
        version_(datacache.version()),
        valid_(it_ != end_),
        cached_(false) {}

  Iterator(const cache::Datacache<Data_extension> &datacache, bool cached)
      : it_(datacache.begin()),
        end_(datacache.end()),
        version_(datacache.version()),
        valid_(true),
        cached_(cached) {
    if (cached_) {
      for (std::pair<meta::Metadata, Data_extension> element : datacache)
        metadata_.store(element.first, element.second);

      it_ = metadata_.begin();
      end_ = metadata_.end();
    }
  }

  /** Destructor */
  ~Iterator() { metadata_.clear(); }

  /** Get iterator */
  typename cache::Cache<Data_extension>::const_iterator get_iterator() const {
    return it_;
  }

  /** Move iterator forward */
  bool next(size_t version) {
    if (iterator_valid(version) == false) {
      it_ = end_;
      valid_ = false;
      return false;
    }
    it_++;
    return true;
  }

  bool metadata(size_t version, meta::Metadata &metadata) {
    if (iterator_valid(version) == false) {
      it_ = end_;
      valid_ = false;
      return false;
    }
    metadata = it_->first;
    return true;
  }

  bool data(size_t version, Data_extension &data) {
    if (iterator_valid(version) == false) {
      it_ = end_;
      valid_ = false;
      return false;
    }
    data = it_->second;
    return true;
  }

  bool valid(size_t version) {
    valid_ = iterator_valid(version);
    return valid_;
  }

 private:
  /** Internal validity checker */
  inline bool iterator_valid(size_t version) {
    if (cached_) {
      return valid_ && (it_ != end_);
    }
    return valid_ && (version == version_) && (it_ != end_);
  }

 private:
  /** Const Iterator */
  typename cache::Cache<Data_extension>::const_iterator it_;
  /** End */
  typename cache::Cache<Data_extension>::const_iterator end_;
  /** Iterator version */
  size_t version_;
  /** validity of the iterator */
  bool valid_;
  /** Iterator type */
  bool cached_;
  /** Local copy */
  typename cache::Datacache<Data_extension> metadata_;
};

}  // namespace iterator

}  // namespace keyring_common

#endif  // !ITERATOR_INCLUDED
