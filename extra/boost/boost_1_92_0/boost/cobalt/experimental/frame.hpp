//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_EXPERIMENTAL_FRAME_HPP
#define BOOST_COBALT_EXPERIMENTAL_FRAME_HPP

#include <type_traits>
#include <utility>

namespace boost::cobalt::experimental
{

template<typename Impl, typename Promise>
struct frame
{
  void (*resume_) (frame *) = +[](frame * ff) { static_cast<Impl*>(ff)->resume();};
  void (*destroy_)(frame *) = +[](frame * ff) { static_cast<Impl*>(ff)->destroy();};
  typedef Promise promise_type;
  Promise promise;

  template<typename ... Args>
  frame(Args && ... args) : promise(std::forward<Args>(args)...)
  {
  }

};


}

#endif //BOOST_COBALT_EXPERIMENTAL_FRAME_HPP
