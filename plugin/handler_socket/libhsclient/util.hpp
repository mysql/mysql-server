
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_UTIL_HPP
#define DENA_UTIL_HPP

namespace dena {

/* boost::noncopyable */
struct noncopyable {
  noncopyable() { }
 private:
  noncopyable(const noncopyable&);
  noncopyable& operator =(const noncopyable&);
};

};

#endif

