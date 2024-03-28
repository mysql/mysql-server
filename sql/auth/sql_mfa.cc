/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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

#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_authentication_registration.h>

#include "base64.h" /* base64_encode */

#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin_auth.h"
#include "sql/auth/authentication_policy.h"
#include "sql/auth/sql_mfa.h"
#include "sql/derror.h" /* ER_THD */
#include "sql/mysqld.h"
#include "sql/sql_lex.h"
#include "sql/strfunc.h" /* lex_string_strmake */

#include "mysqld_error.h"

namespace mfa_consts {
constexpr const char requires_registration[] = "requires_registration";
constexpr const char passwordless[] = "passwordless";
constexpr const char auth_string[] = "authentication_string";
constexpr const char auth_plugin[] = "plugin";
}  // namespace mfa_consts

Multi_factor_auth_list::Multi_factor_auth_list(MEM_ROOT *mem_root)
    : m_factor(Mem_root_allocator<I_multi_factor_auth *>(mem_root)) {}

Multi_factor_auth_list::~Multi_factor_auth_list() { m_factor.clear(); }

/**
  This method checks MFA methods present in ACL_USER against new factor
  specified as part of ALTER USER sql.

  @param [in]  thd       Connection handler
  @param [in]  user      Handler to LEX_USER whose Multi Factor Auth methods
  will being added/dropped or modified

  @returns status of the validation
    @retval false Success ALTER USER can proceed further
    @retval true  Failure report error for ALTER USER
*/
bool Multi_factor_auth_list::is_alter_allowed(THD *thd, LEX_USER *user) {
  List<LEX_MFA> &list = user->mfa_list;
  List_iterator<LEX_MFA> lex_mfa_list(list);
  LEX_MFA *new_factor = nullptr;
  while ((new_factor = lex_mfa_list++)) {
    if (get_mfa_list_size() == 0) {
      if (new_factor->add_factor) {
        /* ensure MFA methods are added in an order. */
        if ((list.size() == 1) && new_factor->nth_factor > 2) {
          /* create user u1;
             alter user u1 add 3 factor ... is not allowed. */
          auto n = new_factor->nth_factor - 1;
          my_error(ER_MFA_METHOD_NOT_EXISTS, MYF(0), n, n);
          return true;
        }
        if ((list.size() == 2) && !lex_mfa_list.is_last()) {
          LEX_MFA *next_factor = (LEX_MFA *)lex_mfa_list.next();
          /*
            create user u1;
            alter user u1 add 3 factor .. add 2 factor ..;
            is not allowed.
          */
          if (new_factor->nth_factor > next_factor->nth_factor) {
            my_error(ER_MFA_METHODS_INVALID_ORDER, MYF(0),
                     next_factor->nth_factor, new_factor->nth_factor);
            return true;
          }
        }
      } else {
        /*
          This user does not have any MFA methods defined, thus only allowed
          operation should be ADD, else report error.
        */
        auto n = new_factor->nth_factor;
        my_error(ER_MFA_METHOD_NOT_EXISTS, MYF(0), n, n);
        return true;
      }
    } else {
      if (new_factor->add_factor) {
        /* check for MFA method which already exists */
        for (auto m_it : get_mfa_list()) {
          Multi_factor_auth_info *acl_mfa_info =
              m_it->get_multi_factor_auth_info();
          /*
            user account configured with passwordless auth methods should not
            be allowed to perform ADD/DROP operations.
          */
          if (acl_mfa_info->is_passwordless()) {
            auto s = acl_mfa_info->get_command_string(thd->lex->sql_command);
            s.append("... ADD ");
            my_error(ER_INVALID_MFA_OPERATIONS_FOR_PASSWORDLESS_USER, MYF(0),
                     s.c_str(), user->user.str, user->host.str);
            return true;
          }
          if (new_factor->nth_factor == acl_mfa_info->get_nth_factor()) {
            auto n = acl_mfa_info->get_nth_factor();
            my_error(ER_MFA_METHOD_EXISTS, MYF(0), n, n, n, n);
            return true;
          }
        }
      } else if (new_factor->drop_factor || new_factor->unregister ||
                 new_factor->modify_factor) {
        bool exists = false;
        /*
          check if MFA method we are dropping/modify does exists, else report
          error
        */
        for (auto m_it : get_mfa_list()) {
          if (exists) break;
          Multi_factor_auth_info *acl_mfa_info =
              m_it->get_multi_factor_auth_info();
          /*
            FINISH REGISTRATION step for user account configured with
            passwordless auth methods is binlogged as
            ALTER USER .. MODIFY 2 FACTOR IDENTIFIED WITH ... AS 'blob'
            Thus allow MODIFY operation only if user has PASSWORDLESS_USER_ADMIN
            privilege.
          */
          if (acl_mfa_info->is_passwordless()) {
            bool priv_exist = thd->security_context()
                                  ->has_global_grant(STRING_WITH_LEN(
                                      "PASSWORDLESS_USER_ADMIN"))
                                  .first;
            if (!(priv_exist && new_factor->modify_factor)) {
              auto s = acl_mfa_info->get_command_string(thd->lex->sql_command);
              if (new_factor->add_factor)
                s.append("... ADD ");
              else if (new_factor->drop_factor)
                s.append("... DROP ");
              else if (new_factor->unregister)
                s.append("... UNREGISTER ");
              my_error(ER_INVALID_MFA_OPERATIONS_FOR_PASSWORDLESS_USER, MYF(0),
                       s.c_str(), user->user.str, user->host.str);
              return true;
            }
            /* ensure that plugin is still fido */
            if (my_strcasecmp(system_charset_info,
                              acl_mfa_info->get_plugin_str(),
                              new_factor->plugin.str)) {
              my_error(ER_INVALID_MFA_OPERATIONS_FOR_PASSWORDLESS_USER, MYF(0),
                       "ALTER USER ... MODIFY ", user->user.str,
                       user->host.str);
              return true;
            }
          }
          if (new_factor->nth_factor == acl_mfa_info->get_nth_factor()) {
            exists = true;
          }
        }
        if (!exists) {
          auto n = new_factor->nth_factor;
          my_error(ER_MFA_METHOD_NOT_EXISTS, MYF(0), n, n);
          return true;
        }
      } else if (new_factor->requires_registration) {
        for (auto m_it : get_mfa_list()) {
          Multi_factor_auth_info *acl_mfa_info =
              m_it->get_multi_factor_auth_info();
          /* MFA method exists and we are doing registration */
          if (new_factor->nth_factor == acl_mfa_info->get_nth_factor()) {
            /* in case registration is already done, report error */
            if (!acl_mfa_info->get_requires_registration()) {
              my_error(ER_PLUGIN_REGISTRATION_DONE, MYF(0),
                       acl_mfa_info->get_nth_factor());
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

/**
  This method modifies the Multi factor authentication interface based on ALTER
  USER sql. This method refers to Multi factor authentication interface present
  in ACL_USER against the new interface which is passed as an input and updates
  the nth factor method in new interface by either adding or dropping the nth
  factor methods. An expression like new_mfa_interface = ACL_USER::m_mfa;

  @param [in, out]  m      handler to new Multi factor authentication interface
*/
void Multi_factor_auth_list::alter_mfa(I_multi_factor_auth *m) {
  /* this pointer holds in memory copy, m holds new factor to be modified */
  Multi_factor_auth_list *lhs = m->get_multi_factor_auth_list();

  bool drop_2nd_factor = false;
  bool drop_3rd_factor = false;

  for (auto new_it : lhs->m_factor) {
    Multi_factor_auth_info *new_factor = new_it->get_multi_factor_auth_info();
    if (new_factor->is_add_factor()) {
      /* append in memory copy to new factor */
      for (auto m_it : this->m_factor) {
        Multi_factor_auth_info *acl_factor = m_it->get_multi_factor_auth_info();
        lhs->add_factor(acl_factor);
        lhs->sort_mfa();
      }
    } else if (new_factor->is_drop_factor()) {
      /*
        when iterating through mfa list its not possible to modify the the list
        to perform a drop operation, thus make a note of what factors need to
        be dropped and add them to lhs, so that factors can be dropped later
      */
      for (auto m_it : this->m_factor) {
        Multi_factor_auth_info *acl_factor = m_it->get_multi_factor_auth_info();
        if (new_factor->get_factor() == acl_factor->get_factor()) {
          if (new_factor->get_factor() == nthfactor::SECOND_FACTOR)
            drop_2nd_factor = true;
          else if (new_factor->get_factor() == nthfactor::THIRD_FACTOR)
            drop_3rd_factor = true;
        } else {
          /* add factors to lhs */
          if (lhs->get_mfa_list_size() < this->get_mfa_list_size()) {
            lhs->add_factor(acl_factor);
            lhs->sort_mfa();
          }
        }
      }
    } else if (new_factor->get_requires_registration()) {
      for (auto m_it : this->m_factor) {
        Multi_factor_auth_info *acl_factor = m_it->get_multi_factor_auth_info();
        if (new_factor->get_factor() == acl_factor->get_factor())
          *new_factor = *acl_factor;
        else {
          lhs->add_factor(acl_factor);
          lhs->sort_mfa();
        }
      }
    } else if (new_factor->get_unregister()) {
      for (auto m_it : this->m_factor) {
        Multi_factor_auth_info *acl_factor = m_it->get_multi_factor_auth_info();
        if (new_factor->get_factor() == acl_factor->get_factor())
          *new_factor = *acl_factor;
        else {
          if (lhs->get_mfa_list_size() < this->get_mfa_list_size()) {
            lhs->add_factor(acl_factor);
            lhs->sort_mfa();
          }
        }
      }
    } else if (new_factor->is_modify_factor()) {
      for (auto m_it : this->m_factor) {
        Multi_factor_auth_info *acl_factor = m_it->get_multi_factor_auth_info();
        if (acl_factor->is_passwordless()) {
          new_factor->set_passwordless(true);
          break;
        }
        if (lhs->get_mfa_list_size() == this->get_mfa_list_size()) break;
        if (new_factor->get_factor() != acl_factor->get_factor()) {
          lhs->add_factor(acl_factor);
          lhs->sort_mfa();
        }
      }
    }
  }
  /*
    at this point lhs has all auth factors, based on which factor to
    drop do needed action.
  */
  if (drop_2nd_factor && drop_3rd_factor) {
    lhs->m_factor.clear();
  } else {
    uint sz = lhs->m_factor.size();
    if (sz == 2) {
      auto sf = lhs->m_factor[0];
      auto tf = lhs->m_factor[1];
      /* drop 2nd factor and overwrite 2nd with 3rd factor */
      if (drop_2nd_factor) {
        lhs->m_factor.clear();
        Multi_factor_auth_info *t = tf->get_multi_factor_auth_info();
        t->set_factor(nthfactor::SECOND_FACTOR);
        lhs->add_factor(tf);
      } else if (drop_3rd_factor) {
        lhs->m_factor.clear();
        lhs->add_factor(sf);
      }
    } else if (sz == 1 && (drop_2nd_factor || drop_3rd_factor)) {
      lhs->m_factor.clear();
    }
  }
}

/**
  This method checks the modified Multi factor authentication interface methods
  based on ALTER USER sql against authentication policy.

  @param [in]  thd              Connection handle
  @param [in]  policy_factors   Authentication policy factors

  @returns status of the validation
    @retval false Success (modified mfa methods match policy)
    @retval true  Failure (authentication policy is vioalted)
*/
bool Multi_factor_auth_list::validate_against_authentication_policy(
    THD *thd, const authentication_policy::Factors &policy_factors) {
  bool policy_priv_exist =
      thd->security_context()
          ->has_global_grant(STRING_WITH_LEN("AUTHENTICATION_POLICY_ADMIN"))
          .first;
  uint nth_factor = 1;
  auto acl_it = m_factor.begin();
  auto factors_it = policy_factors.begin();
  /* skip first factor plugin name in policy list */
  factors_it++;
  for (; (acl_it != m_factor.end() && factors_it != policy_factors.end());
       ++factors_it, ++acl_it) {
    Multi_factor_auth_info *acl_factor =
        (*acl_it)->get_multi_factor_auth_info();
    nth_factor = acl_factor->get_nth_factor();
    /* mfa plugin method is not mandatory so allow */
    if (!factors_it->is_mandatory_specified()) continue;
    /* mfa plugin method does not match against policy */
    if (factors_it->get_mandatory_plugin().compare(
            acl_factor->get_plugin_str()))
      goto error;
  }
  nth_factor++;
  /* if more plugin exists in policy check that its optional only */
  while (factors_it != policy_factors.end()) {
    if (!factors_it->is_optional()) goto error;
    factors_it++;
  }
  return false;
error:
  if (policy_priv_exist) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_AUTHENTICATION_POLICY_MISMATCH,
        ER_THD(thd, ER_AUTHENTICATION_POLICY_MISMATCH), nth_factor);
    return false;
  }
  my_error(ER_AUTHENTICATION_POLICY_MISMATCH, MYF(0), nth_factor);
  return true;
}

/**
  Helper method to sort nth factor methods in multi-factor authentication
  interface such that 2nd factor method always precedes 3rd factor method.
*/
void Multi_factor_auth_list::sort_mfa() {
  assert(m_factor.size() == 2);
  Multi_factor_auth_info *sf = m_factor[0]->get_multi_factor_auth_info();
  Multi_factor_auth_info *tf = m_factor[1]->get_multi_factor_auth_info();
  if (sf->get_nth_factor() > tf->get_nth_factor()) {
    /* swap elements. */
    m_factor[0] = tf;
    m_factor[1] = sf;
  }
}

/**
  Interface method to validate the auth plugin chain before updating
  the user_attributes in mysql.user table.

  @param [in]  thd              Connection handler
  @param [in]  policy_factors   Authentication policy factors

  @returns status of the validation
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::validate_plugins_in_auth_chain(
    THD *thd, const authentication_policy::Factors &policy_factors) {
  for (auto m : m_factor) {
    if (m->validate_plugins_in_auth_chain(thd, policy_factors)) return true;
  }
  return false;
}

/**
  Interface method to update user_attributes.

  @returns status of update operation
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::update_user_attributes() {
  for (auto m : m_factor) {
    if (m->update_user_attributes()) return true;
  }
  return false;
}

/**
  Interface method to convert this interface into a valid
  JSON object.

  @param [in, out]  mfa_arr      A json array into which nth factor
  Multi factor authentication methods needs to be added.

  @returns status of serialization
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::serialize(Json_array &mfa_arr) {
  for (auto m : m_factor) {
    if (m->serialize(mfa_arr)) return true;
  }
  return false;
}

/**
  Interface method to convert a valid JSON object into this interface.

  @param [in]  nth_factor Refers to which factor needs to be deserialized
  @param [in]  mfa_dom    JSON dom object which should be deserialized

  @returns status of deserialization
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::deserialize(uint nth_factor, Json_dom *mfa_dom) {
  if (m_factor[nth_factor]->deserialize(nth_factor, mfa_dom)) return true;
  return false;
}

/**
  Interface method to initiate registration.

  @param [in]       thd        Connection handler
  @param [in]       nth_factor Refers to which factor needs registration

  @returns status of registration step
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::init_registration(THD *thd, uint nth_factor) {
  for (auto m : m_factor) {
    if (m->init_registration(thd, nth_factor)) return true;
  }
  return false;
}

/**
  Interface method to finish registration step.

  @param [in]       thd        Connection handler
  @param [in]       user_name  Handler to LEX_USER
  @param [in]       nth_factor Refers to which factor needs registration

  @returns status of registration step
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_list::finish_registration(THD *thd, LEX_USER *user_name,
                                                 uint nth_factor) {
  for (auto m : m_factor) {
    if (m->finish_registration(thd, user_name, nth_factor)) return true;
  }
  return false;
}

/**
  Interface method to check if registration step in for passwordless
  authentication method.

  @retval false Success
  @retval true  Failure
*/
bool Multi_factor_auth_list::is_passwordless() {
  bool v = false;
  for (auto m : m_factor) {
    v |= m->is_passwordless();
  }
  return v;
}

/**
  Interface method to fill in Multi factor authentication method details
  during query rewrite

  @param [in]       thd        Connection handler
  @param [in]       user_name  Handler to LEX_USER
*/
void Multi_factor_auth_list::get_info_for_query_rewrite(THD *thd,
                                                        LEX_USER *user_name) {
  for (auto m : m_factor) {
    m->get_info_for_query_rewrite(thd, user_name);
  }
}

/**
  Interface method to fill in generated passwords from Multi factor
  authentication methods

  @param [out]  gp        List holding all generated passwords.
  @param [in]   u         Name of user
  @param [in]   h         Host name

*/
void Multi_factor_auth_list::get_generated_passwords(Userhostpassword_list &gp,
                                                     const char *u,
                                                     const char *h) {
  for (auto m : m_factor) {
    m->get_generated_passwords(gp, u, h);
  }
}

/**
  Interface method to fill in generated server challenge from init
  registration step.

  @param [out]   sc       Buffer to hold server challenge

*/
void Multi_factor_auth_list::get_server_challenge_info(
    server_challenge_info_vector &sc) {
  for (auto m : m_factor) {
    m->get_server_challenge_info(sc);
  }
}

my_vector<I_multi_factor_auth *> &Multi_factor_auth_list::get_mfa_list() {
  return m_factor;
}

size_t Multi_factor_auth_list::get_mfa_list_size() { return m_factor.size(); }

Multi_factor_auth_info::Multi_factor_auth_info(MEM_ROOT *mem_root)
    : m_mem_root(mem_root) {
  m_multi_factor_auth = new (m_mem_root) LEX_MFA;
  m_multi_factor_auth->reset();
}

Multi_factor_auth_info::Multi_factor_auth_info(MEM_ROOT *mem_root, LEX_MFA *m)
    : m_mem_root(mem_root) {
  m_multi_factor_auth = new (m_mem_root) LEX_MFA;
  m_multi_factor_auth->reset();
  m_multi_factor_auth->copy(m, m_mem_root);
}

/**
  This method validates nth factor authentication plugin during
  ALTER/CREATE USER sql.

  @param [in]  thd              Connection handler
  @param [in]  policy_factors   Authentication policy factors

  @returns status of the validation
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_info::validate_plugins_in_auth_chain(
    THD *thd, const authentication_policy::Factors &policy_factors) {
  if (is_identified_by() && !is_identified_with()) {
    if (policy_factors.size() < get_nth_factor()) return true;
    auto policy_factor = policy_factors[get_nth_factor() - 1];
    const std::string &plugin_name(
        policy_factor.get_mandatory_or_default_plugin());
    set_plugin_str(plugin_name.c_str(), plugin_name.length());
  }
  plugin_ref plugin = my_plugin_lock_by_name(nullptr, plugin_name(),
                                             MYSQL_AUTHENTICATION_PLUGIN);
  /* check if plugin is loaded */
  if (!plugin) {
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), get_plugin_str());
    return (true);
  }
  st_mysql_auth *auth = (st_mysql_auth *)plugin_decl(plugin)->info;
  if ((auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE) ||
      (auth->authentication_flags &
       AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE)) {
    /*
      Auth plugin which supports registration process can only be used to create
      passwordless user account.
    */
    if (is_passwordless()) {
      my_error(ER_INVALID_PLUGIN_FOR_REGISTRATION, MYF(0), get_plugin_str());
      plugin_unlock(nullptr, plugin);
      return (true);
    }
    /*
      If it is a registration step or de-registration step ensure
      plugin does support registration process.
    */
    if (m_multi_factor_auth->requires_registration ||
        m_multi_factor_auth->unregister) {
      my_error(ER_INVALID_PLUGIN_FOR_REGISTRATION, MYF(0), get_plugin_str());
      plugin_unlock(nullptr, plugin);
      return (true);
    } else {
      if (auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE) {
        /*
          2nd and 3rd factor auth plugin should not store passwords
          internally
        */
        my_error(ER_INVALID_MFA_PLUGIN_SPECIFIED, MYF(0), get_plugin_str(),
                 m_multi_factor_auth->nth_factor,
                 get_command_string(thd->lex->sql_command).c_str());
        plugin_unlock(nullptr, plugin);
        return (true);
      }
    }
  } else if (auth->authentication_flags & AUTH_FLAG_REQUIRES_REGISTRATION) {
    if (!get_auth_str_len()) set_requires_registration(true);
    if (is_identified_by()) {
      /*
        IDENTIFIED BY not allowed for plugins which require registration
      */
      my_error(ER_IDENTIFIED_BY_UNSUPPORTED, MYF(0),
               get_command_string(thd->lex->sql_command).c_str(),
               get_plugin_str());
      plugin_unlock(nullptr, plugin);
      return (true);
    }
  }
  /* generate auth string */
  if (is_identified_by()) {
    const char *inbuf = get_auth_str();
    unsigned int inbuflen = (unsigned)get_auth_str_len();
    char outbuf[MAX_FIELD_WIDTH] = {0};
    unsigned int buflen = MAX_FIELD_WIDTH;
    const char *password = nullptr;
    std::string gen_password;
    if (m_multi_factor_auth->has_password_generator) {
      thd->m_disable_password_validation = true;
      generate_random_password(&gen_password,
                               thd->variables.generated_random_password_length);
      inbuf = gen_password.c_str();
      inbuflen = gen_password.length();
      set_generated_password(gen_password.c_str(), gen_password.length());
    }
    if (auth->generate_authentication_string(outbuf, &buflen, inbuf,
                                             inbuflen)) {
      plugin_unlock(nullptr, plugin);
      return (true);
    }
    if (buflen) {
      password = strmake_root(m_mem_root, outbuf, buflen);
    } else {
      password = const_cast<char *>("");
    }
    if (inbuflen > 0) memset(const_cast<char *>(inbuf), 0, inbuflen);
    /* Use the authentication_string field as password */
    set_auth_str(password, buflen);
  }
  plugin_unlock(nullptr, plugin);
  return false;
}

/**
  Interface method to validate the auth plugin chain if user_attributes in
  mysql.user table is modified using INSERT, UPDATE sql.

  @returns status of row validation
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_info::validate_row() {
  /* validate all inputs */
  plugin_ref plugin = my_plugin_lock_by_name(nullptr, plugin_name(),
                                             MYSQL_AUTHENTICATION_PLUGIN);
  /* check if plugin is loaded */
  if (!plugin) {
    LogErr(WARNING_LEVEL, ER_MFA_PLUGIN_NOT_LOADED, get_plugin_str());
    return (true);
  }
  st_mysql_auth *auth = (st_mysql_auth *)plugin_decl(plugin)->info;
  if (auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE) {
    /* if registration flag is set then user_attributes is corrupt */
    if (get_requires_registration()) {
      char msg[64];
      sprintf(msg, "Please check requires_registration flag for %d factor",
              get_nth_factor());
      LogErr(WARNING_LEVEL, ER_MFA_USER_ATTRIBUTES_CORRUPT, msg);
    } else {
      /* 2nd and 3rd factor auth plugin should not store passwords internally */
      char msg[256];
      sprintf(msg, "Please check authentication plugin for %s factor",
              get_plugin_str());
      LogErr(WARNING_LEVEL, ER_MFA_USER_ATTRIBUTES_CORRUPT, msg);
    }
    plugin_unlock(nullptr, plugin);
    return (true);
  }
  plugin_unlock(nullptr, plugin);
  return false;
}

/**
  Method to update User_attributes column in mysql.user table.

  @returns status of the operation
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_info::update_user_attributes() {
  /* update details of Multi factor authentication method */
  m_update.m_what |= USER_ATTRIBUTES;
  m_update.m_user_attributes = acl_table::USER_ATTRIBUTE_NONE;
  if (m_multi_factor_auth->unregister) {
    set_auth_str(nullptr, 0);
    set_requires_registration(true);
  }
  return false;
}

/**
  Helper function to convert an instance of Multi_factor_auth_info
  into a JSON object.

  @param [out]  mfa_arr       Json Array holding details about Multi factor
  authentication methods.

  @retval false Success
  @retval true  Failure
*/
bool Multi_factor_auth_info::serialize(Json_array &mfa_arr) {
  if (m_update.m_user_attributes & acl_table::USER_ATTRIBUTE_NONE) return false;
  Json_object auth_factor;

  Json_int rr(m_multi_factor_auth->requires_registration ? 1 : 0);
  auth_factor.add_clone(mfa_consts::requires_registration, &rr);

  Json_int pl(m_multi_factor_auth->passwordless ? 1 : 0);
  auth_factor.add_clone(mfa_consts::passwordless, &pl);

  const std::string auth_plugin(this->get_plugin_str(),
                                this->get_plugin_str_len());
  Json_string auth_plugin_str(auth_plugin);
  auth_factor.add_clone(mfa_consts::auth_plugin, &auth_plugin_str);

  const std::string auth_string(this->get_auth_str(), this->get_auth_str_len());
  Json_string auth_str(auth_string);
  auth_factor.add_clone(mfa_consts::auth_string, &auth_str);

  mfa_arr.append_clone(&auth_factor);
  return false;
}

/**
  Helper function to read details from Json object representing Multi factor
  authentication methods and filling details in Multi_factor_auth_info
  instance.

  @param [in]   nth_factor Number referring to nth factor.
  @param [out]  mfa_dom    Json object holding details about Multi factor
  authentication method.

  @retval false Success
  @retval true  Failure
*/
bool Multi_factor_auth_info::deserialize(uint nth_factor, Json_dom *mfa_dom) {
  if (mfa_dom->json_type() != enum_json_type::J_OBJECT) return true;
  (nth_factor ? set_factor(nthfactor::THIRD_FACTOR)
              : set_factor(nthfactor::SECOND_FACTOR));
  Json_object *with_fa_obj = down_cast<Json_object *>(mfa_dom);
  Json_dom *rr_dom = with_fa_obj->get(mfa_consts::requires_registration);
  if (rr_dom) {
    if (rr_dom->json_type() != enum_json_type::J_INT) return true;
    Json_int *rr_val = down_cast<Json_int *>(rr_dom);
    set_requires_registration((rr_val->value() ? true : false));
  }
  Json_dom *pl_dom = with_fa_obj->get(mfa_consts::passwordless);
  if (pl_dom) {
    if (pl_dom->json_type() != enum_json_type::J_INT) return true;
    Json_int *pl_val = down_cast<Json_int *>(pl_dom);
    set_passwordless((pl_val->value() ? true : false));
  }
  Json_dom *auth_str_dom = with_fa_obj->get(mfa_consts::auth_string);
  if (auth_str_dom) {
    if (auth_str_dom->json_type() != enum_json_type::J_STRING) return true;
    Json_string *auth_str = down_cast<Json_string *>(auth_str_dom);
    set_auth_str(auth_str->value().c_str(), auth_str->size());
  }
  Json_dom *auth_plugin_dom = with_fa_obj->get(mfa_consts::auth_plugin);
  if (auth_plugin_dom) {
    if (auth_plugin_dom->json_type() != enum_json_type::J_STRING) return true;
    Json_string *auth_plugin = down_cast<Json_string *>(auth_plugin_dom);
    set_plugin_str(auth_plugin->value().c_str(), auth_plugin->size());
  }
  /* validate details of Multi factor authentication methods read from row */
  if (validate_row()) return true;
  return false;
}

/**
  This method initiates registration step. This method calls plugin
  specific registration method to get details needed to do registration.
  This method further appends user name to it. This method will do nothing
  in case init registration is already done.

  @param [in]  thd        THD handle
  @param [in]  nth_factor Number representing which factor to init
  registration step

  Format of buffer is a length encoded string.
  [salt length][random salt][relying party ID length][relying party ID]
  [user name length][user name]

  @return registration status
    @retval false Success
    @retval true  Failure
*/
bool Multi_factor_auth_info::init_registration(THD *thd, uint nth_factor) {
  /* check if we are registerting correct multi-factor authentication method */
  if (get_nth_factor() != nth_factor) return false;

  plugin_ref plugin = my_plugin_lock_by_name(nullptr, plugin_name(),
                                             MYSQL_AUTHENTICATION_PLUGIN);
  /* check if plugin is loaded */
  if (!plugin) {
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), get_plugin_str());
    return (true);
  }
  st_mysql_auth *auth = (st_mysql_auth *)plugin_decl(plugin)->info;
  if (!(auth->authentication_flags & AUTH_FLAG_REQUIRES_REGISTRATION)) {
    my_error(ER_INVALID_PLUGIN_FOR_REGISTRATION, MYF(0), get_plugin_str());
    plugin_unlock(nullptr, plugin);
    return (true);
  }

  /*
    in case init registration is done, then server challenge will be
    in auth string
  */
  if (get_auth_str_len()) {
    const char *client_plugin = auth->client_auth_plugin;
    set_client_plugin(client_plugin, strlen(client_plugin));
    plugin_unlock(nullptr, plugin);
    return false;
  }

  unsigned char *plugin_buf = nullptr;
  unsigned int plugin_buf_len = 0;

  std::string service_name("mysql_authentication_registration.");
  service_name.append(get_plugin_str());

  my_h_service h_reg_svc = nullptr;
  SERVICE_TYPE(mysql_authentication_registration) * mysql_auth_reg_service;

  if (srv_registry->acquire(service_name.c_str(), &h_reg_svc)) return true;

  mysql_auth_reg_service =
      reinterpret_cast<SERVICE_TYPE(mysql_authentication_registration) *>(
          h_reg_svc);

  mysql_auth_reg_service->get_challenge_length(&plugin_buf_len);
  /* buffer allocated by server before passing to component service */
  plugin_buf = new (std::nothrow) unsigned char[plugin_buf_len];
  if (plugin_buf == nullptr) return true;
  if (mysql_auth_reg_service->init(&plugin_buf, plugin_buf_len)) {
    delete[] plugin_buf;
    srv_registry->release(h_reg_svc);
    return true;
  }

  srv_registry->release(h_reg_svc);

  /* `user name` + '@' + `host name` */
  Auth_id id(thd->security_context()->priv_user().str,
             thd->security_context()->priv_host().str);
  const std::string user_str = id.auth_str();
  size_t user_str_len = user_str.length();

  /* append user name to random challenge(32bit salt + RP id). */
  size_t buflen = plugin_buf_len + user_str_len + net_length_size(user_str_len);
  unsigned char *buf = new (std::nothrow) unsigned char[buflen];
  if (buf == nullptr) {
    delete[] plugin_buf;
    return true;
  }
  unsigned char *pos = buf;

  memcpy(pos, plugin_buf, plugin_buf_len);
  pos += plugin_buf_len;

  delete[] plugin_buf;

  pos = net_store_length(pos, user_str_len);
  memcpy(pos, user_str.c_str(), user_str_len);
  pos += user_str_len;

  assert(buflen == (uint)(pos - buf));

  /* convert auth string to base64 to be stored in mysql.user table */
  char outbuf[MAX_FIELD_WIDTH] = {0};
  unsigned int outbuflen = MAX_FIELD_WIDTH;
  if (auth->generate_authentication_string(
          outbuf, &outbuflen, reinterpret_cast<char *>(buf), buflen)) {
    if (buf) delete[] buf;
    plugin_unlock(nullptr, plugin);
    return (true);
  }
  if (buf) delete[] buf;
  /* turn OFF init registration flag */
  set_init_registration(false);

  /* save buffer in auth_string */
  set_auth_str(reinterpret_cast<const char *>(outbuf), outbuflen);
  /* save client plugin information */
  const char *client_plugin = auth->client_auth_plugin;
  set_client_plugin(client_plugin, strlen(client_plugin));
  plugin_unlock(nullptr, plugin);
  return false;
}

/**
  This method reads the credential details received from FIDO device
  and saves in user_attributes column of mysql.user table.

  @param [in]  thd        Connection handler
  @param [in]  user_name  Handler to LEX_USER
  @param [in]  nth_factor Number referrering to nth factor

  @retval false Success
  @retval true  Failure
*/
bool Multi_factor_auth_info::finish_registration(THD *thd, LEX_USER *user_name,
                                                 uint nth_factor) {
  if (get_nth_factor() != nth_factor) return false;
  plugin_ref plugin = my_plugin_lock_by_name(nullptr, plugin_name(),
                                             MYSQL_AUTHENTICATION_PLUGIN);
  /* check if plugin is loaded */
  if (!plugin) {
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), get_plugin_str());
    return (true);
  }
  st_mysql_auth *auth = (st_mysql_auth *)plugin_decl(plugin)->info;
  if (!(auth->authentication_flags & AUTH_FLAG_REQUIRES_REGISTRATION)) {
    my_error(ER_INVALID_PLUGIN_FOR_REGISTRATION, MYF(0), get_plugin_str());
    plugin_unlock(nullptr, plugin);
    return (true);
  }

  LEX_MFA *tmp_lex_mfa;
  List_iterator<LEX_MFA> mfa_list_it(user_name->mfa_list);
  unsigned char *signed_challenge = nullptr;
  unsigned int signed_challenge_len = 0;
  /* get challenge response */
  while ((tmp_lex_mfa = mfa_list_it++)) {
    signed_challenge = reinterpret_cast<unsigned char *>(
        const_cast<char *>(tmp_lex_mfa->challenge_response.str));
    signed_challenge_len = tmp_lex_mfa->challenge_response.length;
  }

  /* key handle should not exceed more than 256 bytes */
  unsigned int challenge_response_len = 256;
  unsigned char *challenge_response = new unsigned char[challenge_response_len];

  std::string service_name("mysql_authentication_registration.");
  service_name.append(get_plugin_str());

  my_h_service h_reg_svc = nullptr;
  SERVICE_TYPE(mysql_authentication_registration) * mysql_auth_reg_service;

  if (!srv_registry->acquire(service_name.c_str(), &h_reg_svc)) {
    mysql_auth_reg_service =
        reinterpret_cast<SERVICE_TYPE(mysql_authentication_registration) *>(
            h_reg_svc);

    if (mysql_auth_reg_service->finish(
            signed_challenge, signed_challenge_len,
            reinterpret_cast<const unsigned char *>(get_auth_str()),
            get_auth_str_len(), challenge_response, &challenge_response_len)) {
      my_error(ER_USER_REGISTRATION_FAILED, MYF(0));
      srv_registry->release(h_reg_svc);
      plugin_unlock(nullptr, plugin);
      return (true);
    }
  }
  /* release the service */
  srv_registry->release(h_reg_svc);
  /* convert auth string to base64 to be stored in mysql.user table */
  char outbuf[MAX_FIELD_WIDTH] = {0};
  unsigned int outbuflen = MAX_FIELD_WIDTH;
  if (auth->generate_authentication_string(
          outbuf, &outbuflen, reinterpret_cast<char *>(challenge_response),
          challenge_response_len)) {
    plugin_unlock(nullptr, plugin);
    return (true);
  }
  /* turn OFF finish registration flag */
  set_finish_registration(false);
  if (is_passwordless()) {
    /*
      In below case:
      CREATE USER u1 IDENTIFIED WITH authentication_webauthn INITIAL
          AUTHENTICATION BY 'pass';
      server for above ddl will interchange 1FA and 2FA so that user,
      for first time is expected to authenticate with an initial
      password to perform registration.

      Before registration:
      SELECT user,plugin,user_attributes FROM mysql.user WHERE
      user='u1'\G
      *************************** 1. row ***************************
      user: u1
      plugin: caching_sha2_password
      user_attributes: {"Multi_factor_authentication": [{"plugin":
          "authentication_webauthn", "passwordless": 1,
      "authentication_string":
          "", "requires_registration": 1}]}

      After registration:
      SELECT user,plugin,authentication_string,user_attributes FROM
      mysql.user WHERE user='u1'\G
      *************************** 1. row ***************************
      user: u1
      plugin: authentication_webauthn
      authentication_string: <fido credentials>
      user_attributes: NULL
    */
    lex_string_strmake(thd->mem_root, &user_name->first_factor_auth_info.plugin,
                       reinterpret_cast<const char *>(get_plugin_str()),
                       get_plugin_str_len());
    lex_string_strmake(thd->mem_root, &user_name->first_factor_auth_info.auth,
                       reinterpret_cast<const char *>(outbuf), outbuflen);
  } else {
    set_auth_str(reinterpret_cast<const char *>(outbuf), outbuflen);
  }
  delete[] challenge_response;
  set_requires_registration(false);

  plugin_unlock(nullptr, plugin);
  return false;
}

/**
  This method will fill in missing details like plugin name or
  authentication string, during CREATE/ALTER user sql so that
  binlog is logged with correct Multi factor authentication methods.

  @param [in]  thd        connection handler
  @param [in]  user_name  Handler to LEX_USER
*/
void Multi_factor_auth_info::get_info_for_query_rewrite(THD *thd,
                                                        LEX_USER *user_name) {
  List_iterator<LEX_MFA> mfa_list_it(user_name->mfa_list);
  LEX_MFA *tmp_mfa = nullptr;
  while ((tmp_mfa = mfa_list_it++)) {
    if (tmp_mfa->nth_factor == get_nth_factor()) break;
  }
  /* SHOW CREATE USER mfa list is empty */
  if (tmp_mfa == nullptr) {
    if (thd->lex->sql_command == SQLCOM_CREATE_USER ||
        thd->lex->sql_command == SQLCOM_SHOW_CREATE_USER) {
      LEX_MFA *lm = (LEX_MFA *)thd->alloc(sizeof(LEX_MFA));
      lm->reset();
      lm->plugin.str = get_plugin_str();
      lm->plugin.length = get_plugin_str_len();
      lm->auth.str = get_auth_str();
      lm->auth.length = get_auth_str_len();
      lm->init_registration = get_init_registration();
      lm->finish_registration = get_finish_registration();
      lm->passwordless = is_passwordless();
      user_name->mfa_list.push_back(lm);
      return;
    }
  } else {
    if (!tmp_mfa->plugin.length)
      lex_string_strmake(thd->mem_root, &tmp_mfa->plugin,
                         reinterpret_cast<const char *>(get_plugin_str()),
                         get_plugin_str_len());
    if (!tmp_mfa->auth.length) {
      /*
        if it is passwordless user, copy fido credentials saved as part of
        first factor.
      */
      if (is_passwordless()) {
        lex_string_strmake(thd->mem_root, &tmp_mfa->auth,
                           user_name->first_factor_auth_info.auth.str,
                           user_name->first_factor_auth_info.auth.length);
      } else {
        lex_string_strmake(thd->mem_root, &tmp_mfa->auth, get_auth_str(),
                           get_auth_str_len());
      }
    }
  }
}

/**
  This method will return randomly generated passwords as part of
  IDENTIFIED BY RANDOM PASSWORD clause, so that it can be sent to
  client.

  @param [out]  gp        List holding all generated passwords.
  @param [in]   u         Name of user
  @param [in]   h         Host name
*/
void Multi_factor_auth_info::get_generated_passwords(Userhostpassword_list &gp,
                                                     const char *u,
                                                     const char *h) {
  if (m_multi_factor_auth->has_password_generator) {
    random_password_info p{std::string(u), std::string(h),
                           std::string(get_generated_password_str()),
                           get_nth_factor()};
    gp.push_back(p);
    /* Once password is returned turn off the flag */
    m_multi_factor_auth->has_password_generator = false;
  }
}

/**
  This method will return randomly generated server challenge as part of
  ALTER USER .. INITIATE REGISTRATION clause, so that it can be sent to
  client.

  @param [out]   sc        List holding all generated server challenges.
*/
void Multi_factor_auth_info::get_server_challenge_info(
    server_challenge_info_vector &sc) {
  if (get_requires_registration() && get_auth_str_len()) {
    auto result = std::make_pair(
        std::string(get_auth_str(), get_auth_str_len()),
        std::string(get_client_plugin_str(), get_client_plugin_len()));
    sc.push_back(result);
  }
}

Multi_factor_auth_info &Multi_factor_auth_info::operator=(
    Multi_factor_auth_info &new_af) {
  if (this != &new_af) {
    if (new_af.get_plugin_str_len()) /* copy plugin name */
      set_plugin_str(new_af.get_plugin_str(), new_af.get_plugin_str_len());
    if (new_af.get_auth_str_len()) /* copy auth str */
      set_auth_str(new_af.get_auth_str(), new_af.get_auth_str_len());
    set_passwordless(new_af.is_passwordless());
    set_requires_registration(new_af.get_requires_registration());
  }
  return *this;
}

void Multi_factor_auth_list::add_factor(I_multi_factor_auth *m) {
  m_factor.push_back(m);
}

bool Multi_factor_auth_info::is_identified_by() {
  return m_multi_factor_auth->uses_identified_by_clause;
}
bool Multi_factor_auth_info::is_identified_with() {
  return m_multi_factor_auth->uses_identified_with_clause;
}

LEX_CSTRING &Multi_factor_auth_info::plugin_name() {
  return m_multi_factor_auth->plugin;
}

const char *Multi_factor_auth_info::get_auth_str() {
  return m_multi_factor_auth->auth.str;
}

size_t Multi_factor_auth_info::get_auth_str_len() {
  return m_multi_factor_auth->auth.length;
}

const char *Multi_factor_auth_info::get_generated_password_str() {
  return m_multi_factor_auth->generated_password.str;
}

size_t Multi_factor_auth_info::get_generated_password_len() {
  return m_multi_factor_auth->generated_password.length;
}

const char *Multi_factor_auth_info::get_plugin_str() {
  return m_multi_factor_auth->plugin.str;
}
size_t Multi_factor_auth_info::get_plugin_str_len() {
  return m_multi_factor_auth->plugin.length;
}

const char *Multi_factor_auth_info::get_client_plugin_str() {
  return m_multi_factor_auth->client_plugin.str;
}

size_t Multi_factor_auth_info::get_client_plugin_len() {
  return m_multi_factor_auth->client_plugin.length;
}

nthfactor Multi_factor_auth_info::get_factor() {
  if (m_multi_factor_auth->nth_factor == 2)
    return nthfactor::SECOND_FACTOR;
  else if (m_multi_factor_auth->nth_factor == 3)
    return nthfactor::THIRD_FACTOR;
  else
    return nthfactor::NONE;
}

unsigned int Multi_factor_auth_info::get_nth_factor() {
  return m_multi_factor_auth->nth_factor;
}

bool Multi_factor_auth_info::is_add_factor() {
  return m_multi_factor_auth->add_factor;
}

bool Multi_factor_auth_info::is_drop_factor() {
  return m_multi_factor_auth->drop_factor;
}
bool Multi_factor_auth_info::is_modify_factor() {
  return m_multi_factor_auth->modify_factor;
}

bool Multi_factor_auth_info::is_passwordless() {
  return m_multi_factor_auth->passwordless;
}

bool Multi_factor_auth_info::get_init_registration() {
  return m_multi_factor_auth->init_registration;
}

bool Multi_factor_auth_info::get_finish_registration() {
  return m_multi_factor_auth->finish_registration;
}

bool Multi_factor_auth_info::get_requires_registration() {
  return m_multi_factor_auth->requires_registration;
}

bool Multi_factor_auth_info::get_unregister() {
  return m_multi_factor_auth->unregister;
}

LEX_MFA *Multi_factor_auth_info::get_lex_mfa() { return m_multi_factor_auth; }

void Multi_factor_auth_info::set_auth_str(const char *str, size_t l) {
  lex_string_strmake(m_mem_root, &m_multi_factor_auth->auth, str, l);
}

void Multi_factor_auth_info::set_plugin_str(const char *str, size_t l) {
  lex_string_strmake(m_mem_root, &m_multi_factor_auth->plugin, str, l);
}

void Multi_factor_auth_info::set_generated_password(const char *str, size_t l) {
  lex_string_strmake(m_mem_root, &m_multi_factor_auth->generated_password, str,
                     l);
}

void Multi_factor_auth_info::set_client_plugin(const char *str, size_t l) {
  lex_string_strmake(m_mem_root, &m_multi_factor_auth->client_plugin, str, l);
}

void Multi_factor_auth_info::set_factor(nthfactor f) {
  if (f == nthfactor::SECOND_FACTOR)
    m_multi_factor_auth->nth_factor = 2;
  else if (f == nthfactor::THIRD_FACTOR)
    m_multi_factor_auth->nth_factor = 3;
  else
    m_multi_factor_auth->nth_factor = 0;
}

void Multi_factor_auth_info::set_passwordless(int v) {
  m_multi_factor_auth->passwordless = (v ? true : false);
}

void Multi_factor_auth_info::set_init_registration(bool v) {
  m_multi_factor_auth->init_registration = v;
}

void Multi_factor_auth_info::set_finish_registration(bool v) {
  m_multi_factor_auth->finish_registration = v;
}

void Multi_factor_auth_info::set_requires_registration(int v) {
  m_multi_factor_auth->requires_registration = (v ? true : false);
}

std::string Multi_factor_auth_info::get_command_string(
    enum_sql_command sql_command) {
  switch (sql_command) {
    case SQLCOM_CREATE_USER:
      return std::string("CREATE USER");
      break;
    case SQLCOM_ALTER_USER:
      return std::string("ALTER USER");
      break;
    default:
      assert(0);
  }
  return std::string();
}
