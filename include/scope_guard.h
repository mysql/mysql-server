/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef SCOPE_GUARD_H
#define SCOPE_GUARD_H

/**
  @brief A Lambda to be called at scope exit.

  Used as std::scope_exit of sorts.
  Useful if you can't use unique_ptr to install a
  specific deleter but still want to do automatic
  cleanup at scope exit.

  @note Always use @ref create_scope_guard() instead of this template directly!

  Typical use is:
  @code
     ...
     foo_init();
     auto cleanup_foo = create_scope_guard([&] {
        foo_deinit();
     }
     ...
     if (some_error)
       return; // cleanup_foo calls foo_deinit()
     ...
     // foo_deinit is not going to be called past this point
     cleanup_foo.release();
     return; // return with foo initialized.
     ...
   @endcode
*/
template <typename TLambda>
class Scope_guard {
 public:
  Scope_guard(const TLambda &cleanup_lambda)
      : m_is_released(false), m_cleanup_lambda(cleanup_lambda) {}
  Scope_guard(const Scope_guard<TLambda> &) = delete;
  Scope_guard(Scope_guard<TLambda> &&moved)
      : m_is_released(moved.m_is_released),
        m_cleanup_lambda(moved.m_cleanup_lambda) {
    /* Set moved guard to "invalid" state, the one in which the cleanup lambda
      will not be executed. */
    moved.m_is_released = true;
  }
  ~Scope_guard() {
    if (!m_is_released) {
      m_cleanup_lambda();
    }
  }

  /**
    @brief Releases the scope guard

    Makes sure that when scope guard goes out of scope the
    cleanup lambda is not going to be called.
  */
  inline void release() { m_is_released = true; }

  /**
    @brief Calls the cleanup lambda and releases the scope guard

    Useful if you want to explicitly provoke the cleanup earlier
    than when going out of scope.
  */
  inline void reset() {
    if (!m_is_released) {
      m_cleanup_lambda();
      m_is_released = true;
    }
  }

 private:
  /** If true the cleanup is not going to be called */
  bool m_is_released;
  /** The cleanup to be called */
  const TLambda m_cleanup_lambda;
};

/**
  @brief Create a scope guard object

  Always use this instead of the @ref Scope_guard template itself!
  @sa @ref Scope_guard

  @tparam TLambda The type of the lambda. Inferred, never specify it.
  @param rollback_lambda The lambda to execute.
  @return Scope_guard<TLambda> An specialization of the @ref Scope_guard
  template.
*/
template <typename TLambda>
Scope_guard<TLambda> create_scope_guard(const TLambda rollback_lambda) {
  return Scope_guard<TLambda>(rollback_lambda);
}

/**
  Template class to scope guard variables.
*/
template <typename T>
class Variable_scope_guard {
 public:
  Variable_scope_guard() = delete;
  Variable_scope_guard(T &var) : m_var_ref(var), m_var_val(var) {}

  Variable_scope_guard(const Variable_scope_guard &) = delete;
  Variable_scope_guard(Variable_scope_guard &&) = delete;

  Variable_scope_guard &operator=(const Variable_scope_guard &) = delete;
  Variable_scope_guard &operator=(Variable_scope_guard &&) = delete;

  ~Variable_scope_guard() { m_var_ref = m_var_val; }

 private:
  T &m_var_ref;
  T m_var_val;
};
#endif /* SCOPE_GUARD_H */
