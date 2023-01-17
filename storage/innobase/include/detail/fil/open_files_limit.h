/*****************************************************************************

Copyright (c) 2021, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/detail/fil/open_files_limit.h
Implementation of innodb_open_files limit management. */

#ifndef detail_fil_open_files_limit_h
#define detail_fil_open_files_limit_h

#include <atomic>

#include "srv0dynamic_procedures.h"

bool fil_open_files_limit_update(size_t &new_max_open_files);

namespace fil {
namespace detail {

class Open_files_limit {
 public:
  Open_files_limit(size_t limit) : m_limit{limit}, m_old_limit{0} {
    m_is_change_in_progress.clear();
#ifndef UNIV_HOTBACKUP
    m_dynamic_procedures.register_procedures();
#endif
  }
  ~Open_files_limit() {
#ifndef UNIV_HOTBACKUP
    m_dynamic_procedures.unregister();
#endif
  }
  size_t get_limit() const { return m_limit.load(); }
  bool set_desired_limit(size_t desired) {
    /* Try to reserve right to change the limit. */
    if (!m_is_change_in_progress.test_and_set()) {
      /* We have a right to change the limit now. Now we can just store the old
      limit and set the new desired one. */
      m_old_limit = m_limit.load();
      m_limit.store(desired);
      DEBUG_SYNC_C("fil_open_files_desired_limit_set");
      return true;
    }
    return false;
  }
  void commit_desired_limit() {
    /* The old value must have been set to a valid value, which must be
    MINIMUM_VALID_VALUE or more. */
    ut_a(m_old_limit >= MINIMUM_VALID_VALUE);
    m_is_change_in_progress.clear();
  }
  void revert_desired_limit() {
    /* The old value must have been set to a valid value, which must be
    MINIMUM_VALID_VALUE or more. */
    ut_a(m_old_limit >= MINIMUM_VALID_VALUE);
    m_limit.store(m_old_limit);
    m_is_change_in_progress.clear();
  }

 private:
#ifndef UNIV_HOTBACKUP
  class Dynamic_procedures : public srv::Dynamic_procedures {
   protected:
    std::vector<srv::dynamic_procedure_data_t> get_procedures() const override {
      return {{get_procedure_name(), innodb_set_open_files_limit,
               innodb_set_open_files_limit_init,
               innodb_set_open_files_limit_deinit}};
    }
    std::string get_module_name() const override {
      return "innodb_open_files_limit";
    }

   private:
    static std::string get_procedure_name() {
      return "innodb_set_open_files_limit";
    }

    static long long innodb_set_open_files_limit(UDF_INIT *, UDF_ARGS *args,
                                                 unsigned char *,
                                                 unsigned char *) {
      Security_context *sctx = current_thd->security_context();
      if (!sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
               .first &&
          !sctx->check_access(SUPER_ACL)) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or SYSTEM_VARIABLES_ADMIN");
        return 0;
      }
      long long new_value_ll = *((long long *)args->args[0]);

      if (new_value_ll < MINIMUM_VALID_VALUE) {
        /* Complain with an error that the limit was not changed.
        "Incorrect arguments to ..." */
        std::string msg = get_procedure_name() +
                          ". New limit value can't be smaller than " +
                          std::to_string(MINIMUM_VALID_VALUE) + ".";
        my_error(ER_WRONG_ARGUMENTS, MYF(0), msg.c_str());
        return 0;
      }
      if (new_value_ll > std::numeric_limits<int32_t>::max()) {
        /* Complain with an error that the limit was not changed.
        "Incorrect arguments to ..." */
        std::string msg =
            get_procedure_name() + ". New limit value can't be larger than " +
            std::to_string(std::numeric_limits<int32_t>::max()) + ".";
        my_error(ER_WRONG_ARGUMENTS, MYF(0), msg.c_str());
        return 0;
      }

      size_t new_value = static_cast<size_t>(new_value_ll);
      if (!fil_open_files_limit_update(new_value)) {
        /* Complain with an error that the limit was not changed. */
        if (new_value == 0) {
          my_error(ER_CONCURRENT_PROCEDURE_USAGE, MYF(0),
                   get_procedure_name().c_str(), get_procedure_name().c_str());
        } else {
          /* "Incorrect arguments to ..." */
          std::string msg =
              get_procedure_name() +
              ". Cannot update innodb_open_files to "
              "this value. Not enough files could be closed in last 5 seconds "
              "or a number of files that cannot be closed would exceed 90% of "
              "the new limit. Consider setting it above " +
              std::to_string(new_value) + ".";
          my_error(ER_WRONG_ARGUMENTS, MYF(0), msg.c_str());
        }
        return 0;
      }
      return new_value_ll;
    }

    /** Initialize dynamic procedure innodb_set_open_files_limit */
    static bool innodb_set_open_files_limit_init(UDF_INIT *, UDF_ARGS *args,
                                                 char *message) {
      if (args->arg_count != 1) {
        snprintf(message, MYSQL_ERRMSG_SIZE, "Invalid number of arguments.");
        return true;
      }
      if (args->args[0] == nullptr) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "First argument must not be null.");
        return true;
      }
      if (args->arg_type[0] != INT_RESULT) {
        snprintf(message, MYSQL_ERRMSG_SIZE, "Invalid first argument type.");
        return true;
      }
      return false;
    }

    /** Deinitialize dynamic procedure innodb_set_open_files_limit */
    static void innodb_set_open_files_limit_deinit(UDF_INIT *) { return; }
  };

  Dynamic_procedures m_dynamic_procedures;
#endif

  /** The maximum limit for opened files. fil_n_files_open should not exceed
  this. It can be changed dynamically by Fil_system::set_open_files_limit()
  method. */
  std::atomic<size_t> m_limit;

  /** The old value of the limit when a change is in progress. It is used in
  case we need to rollback.*/
  size_t m_old_limit;

  /** Atomic flag stating if a change of the limit is in progress. Used to
  disallow multiple threads from processing a limit change. */
  std::atomic_flag m_is_change_in_progress;

  /* Minimum valid value for the open_files_limit setting. */
  static constexpr int MINIMUM_VALID_VALUE = 10;
};

}  // namespace detail
}  // namespace fil

#endif /* detail_fil_open_files_limit_h */
