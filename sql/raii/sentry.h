/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONTAINER_SENTRY_INCLUDED
#define CONTAINER_SENTRY_INCLUDED

#include <functional>

namespace raii {
/**
  Generic sentry class that invokes some callable object of type F upon disposal
  of an instance of this class.

  Check C++ documentation for the definition of `Callable` named requirement for
  more information.
 */
template <typename F = std::function<void()>>
class Sentry {
 public:
  /**
    Constructor for the class that stores the callable object passed as
    argument, to be invoked upon disposal of the object.

    @param dispose The callable object to be invoked upon disposal of the
                   object.
   */
  Sentry(F dispose);
  /**
    Destructor for the class. It will call the stored callable.
   */
  virtual ~Sentry();

 private:
  /** The callable to be invoked upon disposal of the object. */
  F m_dispose;
};
}  // namespace raii

template <typename F>
raii::Sentry<F>::Sentry(F dispose) : m_dispose{dispose} {}

template <typename F>
raii::Sentry<F>::~Sentry() {
  this->m_dispose();
}

#endif  // CONTAINER_SENTRY_INCLUDED
