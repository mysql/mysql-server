/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once
#include <atomic>     /* std::atomic */
#include <functional> /* std::function */
#include <type_traits>
#include <vector>
#include "sql_class.h" /* THD, SYS_VAR */

extern std::vector<std::function<void()>> innodb_sysvar_initializers;

/** A helper class for numeric sysvars which can be read in thread-safe way. */
template <typename T>
class Atomic_sysvar {
  /** Contains all members of Atomic_sysvar<T> in a standard layout struct -
  which, among other things, means we can't mix private with public, use virtual
  methods, nor std::function<>. We need a struct with standard layout in which
  the non-atomic variable is at offset 0, to make it possible to convert a
  pointer to the non-atomic variable registered via plugin API, to a
  pointer to the whole struct containing the atomic we need to update and
  callback we need to call on each change of registered variable. */
  struct Data {
    /** InnoDB code should not look at this, nor modify this directly (except in
    the update() function defined here).
    This will be registered via plugin API and managed by Server layer.
    It must be the first member.
    Note: Server modifies this directly without calling update() at startup. */
    T m_nonatomic;

    /** This stores the atomic variable which InnoDB threads might access at any
    moment via load(). It has a copy of m_nonatomic value, detected either
    during innodb_init_params() or in update() handler. */
    std::atomic<T> m_atomic;

    /** This is a callback provided by InnoDB to be called after each change of
    the value which happens after innodb_init_params(), i.e. when performing
    SET. Note: this includes recovered values of SET PERSIST. */
    void (*m_after_change)();
  } m_data;

 public:
  /** Constructs the atomic sysvar, initializes its value to zero and makes sure
  innodb_init_params() will remember to copy the nonatomic value to atomic value
  once Server layer finishes modifying the nonatomic value during its
  initialization.
  @param[in]  after_change
                A callback to be called *after* value changed due to SET or
                recovery of value stored with SET PERSIST. */
  explicit Atomic_sysvar(void (*after_change)())
      /* The values we set here do not matter as m_nontatomic will anyway be
      overwritten by the Server layer several times without notifying us via
      update(), before eventually Server calls innodb_init_params() at which
      point the m_nonatomic is equal to the value the Server has settled for,
      which reflects the combination the default, the command line arguments and
      the config files, but not the values persisted via SET PERSIST - these are
      handled even later, as if a user executed SET again, and thus that
      triggers update(). We register our var in innodb_sysvar_initializers here,
      which innodb_init_params() iterates over, to ensure m_atomic matches
      m_nonatomic after the Server settles on the value, and any later updates
      such as those resulting from handling values PERSISTed in mysqld-auto.cnf
      are handled later via update() callback. */
      : m_data{0, 0, after_change} {
    innodb_sysvar_initializers.push_back(
        [this]() { m_data.m_atomic.store(m_data.m_nonatomic); });
  }

  /** Get the current value of the sys-var without UB. */
  [[nodiscard]] T load() const { return m_data.m_atomic.load(); }

  /** Get the variable which we want to register via plugin API. Do not use this
  method in InnoDB code - use load() instead. */
  [[nodiscard]] T &registrable() { return m_data.m_nonatomic; }

  /** This is on-update handler registered via plugin API for all sysvars of
  this type. It is called after new value was validated, but before it was
  assigned. It is the responsibility of this function to assign new_value to
  target.
  @param[in]  target      This points to Data::m_nonatomic
  @param[in]  new_value   This is the new value to be assigned to the sysvar */
  static void update(THD *, SYS_VAR *, void *target, const void *new_value) {
    static_assert(std::is_standard_layout_v<Data>);
    static_assert(offsetof(Data, m_nonatomic) == 0);
    auto var = static_cast<Data *>(target);
    ut_ad(&var->m_nonatomic == target);
    var->m_nonatomic = *static_cast<const T *>(new_value);
    var->m_atomic.store(var->m_nonatomic);
    if (var->m_after_change) {
      var->m_after_change();
    }
  }
};

/* Unfortunately, because there's no nice mapping from C++ type to macro name,
we have to manually provide following declarations for each type. */

#define ATOMIC_SYSVAR_ULONG(sysvar_name_suffix, flags, description, validate,  \
                            after_change, default_value, min_value, max_value, \
                            value_granularity)                                 \
  Atomic_sysvar<ulong> innodb_##sysvar_name_suffix(after_change);              \
  static MYSQL_SYSVAR_ULONG(                                                   \
      sysvar_name_suffix, innodb_##sysvar_name_suffix.registrable(), flags,    \
      description, validate, innodb_##sysvar_name_suffix.update,               \
      default_value, min_value, max_value, value_granularity)
