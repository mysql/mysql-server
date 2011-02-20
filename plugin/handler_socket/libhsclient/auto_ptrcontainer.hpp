
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_AUTO_PTRCONTAINER_HPP
#define DENA_AUTO_PTRCONTAINER_HPP

namespace dena {

template <typename Tcnt>
struct auto_ptrcontainer {
  typedef Tcnt container_type;
  typedef typename container_type::value_type value_type;
  typedef typename container_type::pointer pointer;
  typedef typename container_type::reference reference;
  typedef typename container_type::const_reference const_reference;
  typedef typename container_type::size_type size_type;
  typedef typename container_type::difference_type difference_type;
  typedef typename container_type::iterator iterator;
  typedef typename container_type::const_iterator const_iterator;
  typedef typename container_type::reverse_iterator reverse_iterator;
  typedef typename container_type::const_reverse_iterator
    const_reverse_iterator;
  iterator begin() { return cnt.begin(); }
  const_iterator begin() const { return cnt.begin(); }
  iterator end() { return cnt.end(); }
  const_iterator end() const { return cnt.end(); }
  reverse_iterator rbegin() { return cnt.rbegin(); }
  reverse_iterator rend() { return cnt.rend(); }
  const_reverse_iterator rbegin() const { return cnt.rbegin(); }
  const_reverse_iterator rend() const { return cnt.rend(); }
  size_type size() const { return cnt.size(); }
  size_type max_size() const { return cnt.max_size(); }
  bool empty() const { return cnt.empty(); }
  reference front() { return cnt.front(); }
  const_reference front() const { cnt.front(); }
  reference back() { return cnt.back(); }
  const_reference back() const { cnt.back(); }
  void swap(auto_ptrcontainer& x) { cnt.swap(x.cnt); }
  ~auto_ptrcontainer() {
    for (iterator i = begin(); i != end(); ++i) {
      delete *i;
    }
  }
  template <typename Tap> void push_back_ptr(Tap& ap) {
    cnt.push_back(ap.get());
    ap.release();
  }
  void erase_ptr(iterator i) {
    delete *i;
    cnt.erase(i);
  }
  reference operator [](size_type n) { return cnt[n]; }
  const_reference operator [](size_type n) const { return cnt[n]; }
  void clear() { cnt.clear(); }
 private:
  Tcnt cnt;
};

};

#endif

