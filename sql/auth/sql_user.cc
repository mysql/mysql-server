/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>
#include <sys/types.h>
#include <set>

#include "auth_acls.h"
#include "auth_common.h"
#include "handler.h"
#include "hash.h"
#include "item.h"
#include "lex_string.h"
#include "log_event.h"                  /* append_query_string */
#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/plugin.h"
#include "mysql/plugin_auth.h"
#include "mysql/psi/psi_base.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "password.h"                   /* my_make_scrambled_password */
#include "protocol.h"
#include "session_tracker.h"
#include "sql_admin.h"
#include "sql_class.h"
#include "sql_connect.h"
#include "sql_const.h"
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_parse.h"                  /* check_access */
#include "sql_plugin.h"                 /* lock_plugin_data etc. */
#include "sql_plugin_ref.h"
#include "sql_security_ctx.h"
#include "sql_string.h"
#include "system_variables.h"
#include "table.h"
#include "violite.h"
                                        /* key_restore */

#include "auth_internal.h"
#include "current_thd.h"
#include "derror.h"                     /* ER_THD */
#include "log.h"                        /* sql_print_warning */
#include "mysqld.h"
#include "prealloced_array.h"
#include "sql_auth_cache.h"
#include "sql_authentication.h"
#include "sql_user_table.h"

/**
  Auxiliary function for constructing a  user list string.
  This function is used for error reporting and logging.

  @param thd     Thread context
  @param str     A String to store the user list.
  @param user    A LEX_USER which will be appended into user list.
  @param comma   If TRUE, append a ',' before the the user.
  @param ident   If TRUE, append ' IDENTIFIED BY/WITH...' after the user,
                 if the given user has credentials set with 'IDENTIFIED BY/WITH'
 */
void append_user(THD *thd, String *str, LEX_USER *user, bool comma= true,
                 bool ident= false)
{
  String from_user(user->user.str, user->user.length, system_charset_info);
  String from_plugin(user->plugin.str, user->plugin.length, system_charset_info);
  String from_auth(user->auth.str, user->auth.length, system_charset_info);
  String from_host(user->host.str, user->host.length, system_charset_info);

  if (comma)
    str->append(',');
  append_query_string(thd, system_charset_info, &from_user, str);
  str->append(STRING_WITH_LEN("@"));
  append_query_string(thd, system_charset_info, &from_host, str);

  if (ident)
  {
    if (user->plugin.str && (user->plugin.length > 0) &&
        memcmp(user->plugin.str, native_password_plugin_name.str,
               user->plugin.length))
    {
      /**
          The plugin identifier is allowed to be specified,
          both with and without quote marks. We log it with
          quotes always.
        */
      str->append(STRING_WITH_LEN(" IDENTIFIED WITH "));
      append_query_string(thd, system_charset_info, &from_plugin, str);

      if (user->auth.str && (user->auth.length > 0))
      {
        str->append(STRING_WITH_LEN(" AS "));
        append_query_string(thd, system_charset_info, &from_auth, str);
      }
    }
    else if (user->auth.str)
    {
      str->append(STRING_WITH_LEN(" IDENTIFIED BY PASSWORD '"));
      if (user->uses_identified_by_password_clause ||
          user->uses_authentication_string_clause)
      {
        str->append(user->auth.str, user->auth.length);
        str->append("'");
      }
      else
      {
        /*
          Password algorithm is chosen based on old_passwords variable or
          TODO the new password_algorithm variable.
          It is assumed that the variable hasn't changed since parsing.
        */
        if (thd->variables.old_passwords == 0)
        {
          /*
            my_make_scrambled_password_sha1() requires a target buffer size of
            SCRAMBLED_PASSWORD_CHAR_LENGTH + 1.
            The extra character is for the probably originate from either '\0'
            or the initial '*' character.
          */
          char tmp[SCRAMBLED_PASSWORD_CHAR_LENGTH + 1];
          my_make_scrambled_password_sha1(tmp, user->auth.str,
                                          user->auth.length);
          str->append(tmp);
        }
        else
        {
          /*
            With old_passwords == 2 the scrambled password will be binary.
          */
          DBUG_ASSERT(thd->variables.old_passwords = 2);
          str->append("<secret>");
        }
        str->append("'");
      }
    }
  }
}

void append_user_new(THD *thd, String *str, LEX_USER *user, bool comma= true)
{
  String from_user(user->user.str, user->user.length, system_charset_info);
  String from_plugin(user->plugin.str, user->plugin.length, system_charset_info);
  String default_plugin(default_auth_plugin_name.str,
                        default_auth_plugin_name.length, system_charset_info);
  String from_auth(user->auth.str, user->auth.length, system_charset_info);
  String from_host(user->host.str, user->host.length, system_charset_info);

  if (comma)
    str->append(',');
  append_query_string(thd, system_charset_info, &from_user, str);
  str->append(STRING_WITH_LEN("@"));
  append_query_string(thd, system_charset_info, &from_host, str);

  /* CREATE USER is always rewritten with IDENTIFIED WITH .. AS */
  if (thd->lex->sql_command == SQLCOM_CREATE_USER)
  {
    str->append(STRING_WITH_LEN(" IDENTIFIED WITH "));
    if (user->plugin.length > 0)
      append_query_string(thd, system_charset_info, &from_plugin, str);
    else
      append_query_string(thd, system_charset_info, &default_plugin, str);
    if (user->auth.length > 0)
    {
      str->append(STRING_WITH_LEN(" AS "));
      if (thd->lex->contains_plaintext_password)
      {
        str->append("'");
        str->append(STRING_WITH_LEN("<secret>"));
        str->append("'");
      }
      else
        append_query_string(thd, system_charset_info, &from_auth, str);
    }
  }
  else
  {
    if (user->uses_identified_by_clause ||
        user->uses_identified_with_clause ||
        user->uses_identified_by_password_clause)
    {
      str->append(STRING_WITH_LEN(" IDENTIFIED WITH "));
      if (user->plugin.length > 0)
        append_query_string(thd, system_charset_info, &from_plugin, str);
      else
        append_query_string(thd, system_charset_info, &default_plugin, str);
      if (user->auth.length > 0)
      {
        str->append(STRING_WITH_LEN(" AS "));
        if (thd->lex->contains_plaintext_password)
        {
          str->append("'");
          str->append(STRING_WITH_LEN("<secret>"));
          str->append("'");
        }
        else
          append_query_string(thd, system_charset_info, &from_auth, str);
      }
    }
  }
}


extern bool initialized;

/*
 Enumeration of various ACL's and Hashes used in handle_grant_struct()
*/
enum enum_acl_lists
{
  USER_ACL= 0,
  DB_ACL,
  COLUMN_PRIVILEGES_HASH,
  PROC_PRIVILEGES_HASH,
  FUNC_PRIVILEGES_HASH,
  PROXY_USERS_ACL
};

int check_change_password(THD *thd, const char *host, const char *user,
                          const char *new_password, size_t new_password_len)
{
  Security_context *sctx;
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return(1);
  }

  sctx= thd->security_context();
  if (!thd->slave_thread &&
      (strcmp(sctx->user().str, user) ||
       my_strcasecmp(system_charset_info, host,
                     sctx->priv_host().str)))
  {
    if (sctx->password_expired())
    {
      my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
      return(1);
    }
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 0))
      return(1);
  }
  if (!thd->slave_thread &&
      likely((get_server_state() == SERVER_OPERATING)) &&
      !strcmp(thd->security_context()->priv_user().str,""))
  {
    my_error(ER_PASSWORD_ANONYMOUS_USER, MYF(0));
    return(1);
  }

  return(0);
}

/**
  Auxiliary function for constructing CREATE USER sql for a given user.

  @param thd          Thread context
  @param user_name    user for which the sql should be constructed.

  @retval
    0         OK.
    1         Error.
 */

bool mysql_show_create_user(THD *thd, LEX_USER *user_name)
{
  int error= 0;
  ACL_USER *acl_user;
  LEX *lex= thd->lex;
  Protocol *protocol= thd->get_protocol();
  USER_RESOURCES tmp_user_resource;
  enum SSL_type ssl_type;
  char *ssl_cipher, *x509_issuer, *x509_subject;
  char buff[256];
  Item_string *field= NULL;
  List<Item> field_list;
  String sql_text(buff,sizeof(buff),system_charset_info);
  LEX_ALTER alter_info;

  DBUG_ENTER("mysql_show_create_user");
  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::READ_MODE);
  if (!acl_cache_lock.lock())
    DBUG_RETURN(true);

  if (!(acl_user= find_acl_user(user_name->host.str, user_name->user.str, TRUE)))
  {
    String wrong_users;
    append_user(thd, &wrong_users, user_name, wrong_users.length() > 0, false);
    my_error(ER_CANNOT_USER, MYF(0), "SHOW CREATE USER",
             wrong_users.c_ptr_safe());
    DBUG_RETURN(true);
  }
  /* fill in plugin, auth_str from acl_user */
  user_name->auth.str= acl_user->auth_string.str;
  user_name->auth.length= acl_user->auth_string.length;
  user_name->plugin= acl_user->plugin;
  user_name->uses_identified_by_clause= true;
  user_name->uses_identified_with_clause= false;
  user_name->uses_identified_by_password_clause= false;
  user_name->uses_authentication_string_clause= false;

  /* make a copy of user resources, ssl and password expire attributes */
  tmp_user_resource= lex->mqh;
  lex->mqh= acl_user->user_resource;

  /* Set specified_limits flags so user resources are shown properly. */
  if (lex->mqh.user_conn)
    lex->mqh.specified_limits|= USER_RESOURCES::USER_CONNECTIONS;
  if (lex->mqh.questions)
    lex->mqh.specified_limits|= USER_RESOURCES::QUERIES_PER_HOUR;
  if (lex->mqh.updates)
    lex->mqh.specified_limits|= USER_RESOURCES::UPDATES_PER_HOUR;
  if (lex->mqh.conn_per_hour)
    lex->mqh.specified_limits|= USER_RESOURCES::CONNECTIONS_PER_HOUR;

  ssl_type= lex->ssl_type;
  ssl_cipher= lex->ssl_cipher;
  x509_issuer= lex->x509_issuer;
  x509_subject= lex->x509_subject;

  lex->ssl_type= acl_user->ssl_type;
  lex->ssl_cipher= const_cast<char*>(acl_user->ssl_cipher);
  lex->x509_issuer= const_cast<char*>(acl_user->x509_issuer);
  lex->x509_subject= const_cast<char*>(acl_user->x509_subject);

  alter_info= lex->alter_password;

  lex->alter_password.update_password_expired_column= acl_user->password_expired;
  lex->alter_password.use_default_password_lifetime= acl_user->use_default_password_lifetime;
  lex->alter_password.expire_after_days= acl_user->password_lifetime;
  lex->alter_password.update_account_locked_column= true;
  lex->alter_password.account_locked= acl_user->account_locked;
  lex->alter_password.update_password_expired_fields= true;

  /* send the metadata to client */
  field=new Item_string("",0,&my_charset_latin1);
  field->max_length=256;
  strxmov(buff,"CREATE USER for ",user_name->user.str,"@",
          user_name->host.str,NullS);
  field->item_name.set(buff);
  field_list.push_back(field);
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    error= 1;
    goto err;
  }
  sql_text.length(0);
  lex->users_list.push_back(user_name);
  mysql_rewrite_create_alter_user(thd, &sql_text);
  /* send the result row to client */
  protocol->start_row();
  protocol->store(sql_text.ptr(),sql_text.length(),sql_text.charset());
  if (protocol->end_row())
  {
    error= 1;
    goto err;
  }

err:
  /* restore user resources, ssl and password expire attributes */
  lex->mqh= tmp_user_resource;
  lex->ssl_type= ssl_type;
  lex->ssl_cipher= ssl_cipher;
  lex->x509_issuer= x509_issuer;
  lex->x509_subject= x509_subject;

  lex->alter_password= alter_info;
  my_eof(thd);
  DBUG_RETURN(error);
}

/**
   This function does following:
   1. Convert plain text password to hash and update the same in
      user definition.
   2. Validate hash string if specified in user definition.
   3. Identify what all fields needs to be updated in mysql.user
      table based on user definition.

   If the is_role flag is set, the password validation is not used.

  @param thd          Thread context
  @param Str          user on which attributes has to be applied
  @param what_to_set  User attributes
  @param is_privileged_user     Whether caller has CREATE_USER_ACL
                                or UPDATE_ACL over mysql.*
  @param is_role      CREATE ROLE was used to create the authid.

  @retval 0 ok
  @retval 1 ERROR;
*/

bool set_and_validate_user_attributes(THD *thd,
                                      LEX_USER *Str,
                                      ulong &what_to_set,
                                      bool is_privileged_user,
                                      bool is_role)
{
  bool user_exists= false;
  ACL_USER *acl_user;
  plugin_ref plugin= NULL;
  char outbuf[MAX_FIELD_WIDTH]= {0};
  unsigned int buflen= MAX_FIELD_WIDTH, inbuflen;
  const char *inbuf;
  char *password= NULL;
  enum_sql_command command= thd->lex->sql_command;

  what_to_set= 0;
  /* update plugin,auth str attributes */
  if (Str->uses_identified_by_clause ||
      Str->uses_identified_by_password_clause ||
      Str->uses_identified_with_clause ||
      Str->uses_authentication_string_clause)
    what_to_set|= PLUGIN_ATTR;
  else
    what_to_set|= DEFAULT_AUTH_ATTR;

  /* update ssl attributes */
  if (thd->lex->ssl_type != SSL_TYPE_NOT_SPECIFIED)
    what_to_set|= SSL_ATTR;
  /* update connection attributes */
  if (thd->lex->mqh.specified_limits)
    what_to_set|= RESOURCE_ATTR;

  if ((acl_user= find_acl_user(Str->host.str, Str->user.str, TRUE)))
    user_exists= true;

  /* copy password expire attributes to individual user */
  Str->alter_status= thd->lex->alter_password;

  /* update password expire attributes */
  if (Str->alter_status.update_password_expired_column ||
      !Str->alter_status.use_default_password_lifetime ||
      Str->alter_status.expire_after_days)
    what_to_set|= PASSWORD_EXPIRE_ATTR;

  /* update account lock attribute */
  if (Str->alter_status.update_account_locked_column)
    what_to_set|= ACCOUNT_LOCK_ATTR;

  if (Str->plugin.length)
    optimize_plugin_compare_by_pointer(&Str->plugin);

  if (user_exists)
  {
    switch (command)
    {
      case SQLCOM_CREATE_USER:
      {
        /*
          Since user exists, we are likely going to fail
          unless IF NOT EXISTS is specified. In that case
          we need to use default plugin to generate password
          so that binlog entry is correct.
        */
        if (!Str->uses_identified_with_clause)
          Str->plugin= default_auth_plugin_name;
        break;
      }
      case SQLCOM_ALTER_USER:
      {
        if (!Str->uses_identified_with_clause)
        {
          /* If no plugin is given, get existing plugin */
          Str->plugin= acl_user->plugin;
        }
        else if (!(Str->uses_identified_by_clause ||
                   Str->uses_authentication_string_clause) &&
                 auth_plugin_supports_expiration(Str->plugin.str))
        {
          /*
            This is an attempt to change existing users authentication plugin
            without specifying any password. In such cases, expire user's
            password so we can force password change on next login
          */
          Str->alter_status.update_password_expired_column= true;
          what_to_set|= PASSWORD_EXPIRE_ATTR;
        }
        /*
          always check for password expire/interval attributes as there is no
          way to differentiate NEVER EXPIRE and EXPIRE DEFAULT scenario
        */
        if (Str->alter_status.update_password_expired_fields)
          what_to_set|= PASSWORD_EXPIRE_ATTR;
        break;
      }
      default:
      {
        /*
          If we are here, authentication related information can be provided
          only if GRANT statement was used to change user's credentials.
        */

        if (!Str->uses_identified_with_clause)
        {
          /* if IDENTIFIED WITH is not specified set plugin from cache */
          Str->plugin= acl_user->plugin;
          /* set auth str from cache when not specified for existing user */
          if (!(Str->uses_identified_by_clause ||
                Str->uses_identified_by_password_clause ||
                Str->uses_authentication_string_clause))
          {
            Str->auth.str= acl_user->auth_string.str;
            Str->auth.length= acl_user->auth_string.length;
          }
        }
        else if (!(Str->uses_identified_by_clause ||
                   Str->uses_authentication_string_clause) &&
                 auth_plugin_supports_expiration(Str->plugin.str))
        {
          /*
            This is an attempt to change existing users authentication plugin
            without specifying any password. In such cases, expire user's
            password so we can force password change on next login
          */
          Str->alter_status.update_password_expired_column= true;
          what_to_set|= PASSWORD_EXPIRE_ATTR;
        }
      }
    };
  }
  else
  {
    /* set default plugin for new users if not specified */
    if (!Str->uses_identified_with_clause)
      Str->plugin= default_auth_plugin_name;
  }

  optimize_plugin_compare_by_pointer(&Str->plugin);
  plugin= my_plugin_lock_by_name(0, Str->plugin,
                                 MYSQL_AUTHENTICATION_PLUGIN);

  /* check if plugin is loaded */
  if (!plugin)
  {
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), Str->plugin.str);
    return(1);
  }

  if (user_exists &&
      (what_to_set & PLUGIN_ATTR))
  {
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    if (auth->authentication_flags &
         AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE)
    {
      if (!is_privileged_user &&
          (command == SQLCOM_ALTER_USER ||
           command == SQLCOM_GRANT))
      {
        /*
          An external plugin that prevents user
          to change authentication_string information
          unless user is privileged.
        */
        what_to_set= NONE_ATTR;
        my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
                 thd->security_context()->priv_user().str,
                 thd->security_context()->priv_host().str,
                 thd->password ? ER_THD(thd, ER_YES) : ER_THD(thd, ER_NO));
        plugin_unlock(0, plugin);
        return (1);
      }
    }

    if (!(auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE))
    {
      if (command == SQLCOM_SET_PASSWORD)
      {
        /*
          A plugin that does not use internal storage and
          hence does not support SET PASSWORD
        */
        char warning_buffer[MYSQL_ERRMSG_SIZE];
        my_snprintf(warning_buffer, sizeof(warning_buffer),
                    "SET PASSWORD has no significance for user '%s'@'%s' as "
                    "authentication plugin does not support it.",
                    Str->user.str, Str->host.str);
        warning_buffer[MYSQL_ERRMSG_SIZE-1]= '\0';
        push_warning(thd, Sql_condition::SL_NOTE,
                     ER_SET_PASSWORD_AUTH_PLUGIN,
                     warning_buffer);
        plugin_unlock(0, plugin);
        what_to_set= NONE_ATTR;
        return (0);
      }
    }
  }

  /*
    If auth string is specified, change it to hash.
    Validate empty credentials for new user ex: CREATE USER u1;
    We skip authentication string generation if the issued statement was
    CREATE ROLE.
  */
  if (!is_role &&
      (Str->uses_identified_by_clause ||
       (Str->auth.length == 0 && !user_exists)))
  {
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    inbuf= Str->auth.str;
    inbuflen= Str->auth.length;
    if (auth->generate_authentication_string(outbuf,
                                             &buflen,
                                             inbuf,
                                             inbuflen))
    {
      plugin_unlock(0, plugin);
      return(1);
    }
    if (buflen)
    {
      password= (char *) thd->alloc(buflen);
      memcpy(password, outbuf, buflen);
    }
    else
      password= const_cast<char*>("");
    /* erase in memory copy of plain text password */
    if (Str->auth.length > 0)
      memset((char*)(Str->auth.str), 0, Str->auth.length);
    /* Use the authentication_string field as password */
    Str->auth.str= password;
    Str->auth.length= buflen;
    thd->lex->contains_plaintext_password= false;
  }

  /* Validate hash string */
  if((Str->uses_identified_by_password_clause ||
      Str->uses_authentication_string_clause))
  {
    /*
      The statement CREATE ROLE calls mysql_create_user() with a set of
      lexicographic parameters: users_identified_by_password_caluse= false etc
      It also sets is_role= true. We don't have to check this parameter here
      since we're already know that the above parameters will be false
      but we place an extra assert here to remind us about the complex
      interdependencies if mysql_create_user() is refactored.
    */
    DBUG_ASSERT(!is_role);
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    if (auth->validate_authentication_string((char*)Str->auth.str,
                                             Str->auth.length))
    {
      my_error(ER_PASSWORD_FORMAT, MYF(0));
      plugin_unlock(0, plugin);
      return(1);
    }
  }
  plugin_unlock(0, plugin);
  return(0);
}

/**
  Change a password hash for a user.

  @param thd Thread handle
  @param host Hostname
  @param user User name
  @param new_password New password hash for host\@user

  Note : it will also reset the change_password flag.
  This is safe to do unconditionally since the simple userless form
  SET PASSWORD = 'text' will be the only allowed form when
  this flag is on. So we don't need to check user names here.


  @see set_var_password::update(THD *thd)

  @return Error code
   @retval 0 ok
   @retval 1 ERROR; In this case the error is sent to the client.
*/

bool change_password(THD *thd, const char *host, const char *user,
                     char *new_password)
{
  TABLE_LIST tables[ACL_TABLES::LAST_ENTRY];
  TABLE *table;
  Acl_table_intact table_intact(thd);
  LEX_USER *combo= NULL;
  std::set<LEX_USER *> users;
  ulong what_to_set= 0;
  size_t new_password_len= strlen(new_password);
  bool transactional_tables;
  bool result= false;
  int ret;

  DBUG_ENTER("change_password");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'  new_password: '%s'",
                      host,user,new_password));
  DBUG_ASSERT(host != 0);                        // Ensured by parent

  if (check_change_password(thd, host, user, new_password, new_password_len))
    DBUG_RETURN(true);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The binlog state will be cleared here to
    statement based replication and will be reset to the originals
    values when we are out of this function scope
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  if ((ret= open_grant_tables(thd, tables, &transactional_tables)))
    DBUG_RETURN(ret != 1);

  table= tables[ACL_TABLES::TABLE_USER].table;

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  if (table_intact.check(thd, table, &mysql_user_table_def))
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  ACL_USER *acl_user;
  if (!(acl_user= find_acl_user(host, user, TRUE)))
  {
    my_error(ER_PASSWORD_NO_MATCH, MYF(0));
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  DBUG_ASSERT(acl_user->plugin.length != 0);

  if (!(combo=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
    DBUG_RETURN(true);

  combo->user.str= user;
  combo->host.str= host;
  combo->user.length= strlen(user);
  combo->host.length= strlen(host);

  thd->make_lex_string(&combo->user,
                       combo->user.str, strlen(combo->user.str), 0);
  thd->make_lex_string(&combo->host,
                       combo->host.str, strlen(combo->host.str), 0);

  combo->plugin= EMPTY_CSTR;
  combo->auth.str= new_password;
  combo->auth.length= new_password_len;
  combo->uses_identified_by_clause= true;
  combo->uses_identified_with_clause= false;
  combo->uses_identified_by_password_clause= false;
  combo->uses_authentication_string_clause= false;
  /* set default values */
  thd->lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
  memset(&(thd->lex->mqh), 0, sizeof(thd->lex->mqh));
  thd->lex->alter_password.update_password_expired_column= false;
  thd->lex->alter_password.use_default_password_lifetime= true;
  thd->lex->alter_password.expire_after_days= 0;
  thd->lex->alter_password.update_account_locked_column= false;
  thd->lex->alter_password.account_locked= false;
  thd->lex->alter_password.update_password_expired_fields= false;

  /*
    When @@log-backward-compatible-user-definitions variable is ON
    and its a slave thread, then the password is already hashed. So
    do not generate another hash.
  */
  if (opt_log_builtin_as_identified_by_password &&
      thd->slave_thread)
    combo->uses_identified_by_clause= false;

  if (set_and_validate_user_attributes(thd, combo, what_to_set, true, false))
  {
    result= 1;
    goto end;
  }
  ret= replace_user_table(thd, table, combo, 0, false, true, what_to_set);
  if (ret)
  {
    result= 1;
    goto end;
  }
  if (!update_sctx_cache(thd->security_context(), acl_user, false) &&
       thd->security_context()->password_expired())
  {
    /* the current user is not the same as the user we operate on */
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    result= 1;
    goto end;
  }

  result= 0;
  users.insert(combo);

end:
  result= log_and_commit_acl_ddl(thd, transactional_tables, &users,
                                 false, !result, false);

  DBUG_RETURN(result);
}

/**
  Handle an in-memory privilege structure.

  @param struct_no  The number of the structure to handle (0..5).
  @param drop       If user_from is to be dropped.
  @param user_from  The the user to be searched/dropped/renamed.
  @param user_to    The new name for the user if to be renamed, NULL otherwise.

  @note
    Scan through all elements in an in-memory grant structure and apply
    the requested operation.
    Delete from grant structure if drop is true.
    Update in grant structure if drop is false and user_to is not NULL.
    Search in grant structure if drop is false and user_to is NULL.
    Structures are enumerated as follows:
    0 ACL_USER
    1 ACL_DB
    2 COLUMN_PRIVILIGES_HASH
    3 PROC_PRIVILEGES_HASH
    4 FUNC_PRIVILEGES_HASH
    5 ACL_PROXY_USERS

  @retval > 0  At least one element matched.
  @retval 0    OK, but no element matched.
  @retval -1   Wrong arguments to function or Out of Memory.
*/

static int handle_grant_struct(enum enum_acl_lists struct_no, bool drop,
                               LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  size_t idx;
  size_t elements;
  const char *user= NULL;
  const char *host= NULL;
  ACL_USER *acl_user= NULL;
  ACL_DB *acl_db= NULL;
  ACL_PROXY_USER *acl_proxy_user= NULL;
  GRANT_NAME *grant_name= NULL;
  /*
    Dynamic array acl_grant_name used to store pointers to all
    GRANT_NAME objects
  */
  Prealloced_array<GRANT_NAME *, 16> acl_grant_name(PSI_INSTRUMENT_ME);
  HASH *grant_name_hash= NULL;
  DBUG_ENTER("handle_grant_struct");
  DBUG_PRINT("info",("scan struct: %u  search: '%s'@'%s'",
                     struct_no, user_from->user.str, user_from->host.str));

  DBUG_ASSERT(assert_acl_cache_write_lock(current_thd));

  /* Get the number of elements in the in-memory structure. */
  switch (struct_no) {
  case USER_ACL:
    elements= acl_users->size();
    break;
  case DB_ACL:
    elements= acl_dbs->size();
    break;
  case COLUMN_PRIVILEGES_HASH:
    elements= column_priv_hash.records;
    grant_name_hash= &column_priv_hash;
    break;
  case PROC_PRIVILEGES_HASH:
    elements= proc_priv_hash.records;
    grant_name_hash= &proc_priv_hash;
    break;
  case FUNC_PRIVILEGES_HASH:
    elements= func_priv_hash.records;
    grant_name_hash= &func_priv_hash;
    break;
  case PROXY_USERS_ACL:
    elements= acl_proxy_users->size();
    break;
  default:
    DBUG_RETURN(-1);
  }

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  search    user: '%s'  host: '%s'",
                       struct_no, user_from->user.str, user_from->host.str));
#endif
  /* Loop over all elements. */
  for (idx= 0; idx < elements; idx++)
  {
    /*
      Get a pointer to the element.
    */
    switch (struct_no) {
    case USER_ACL:
      acl_user= &acl_users->at(idx);
      user= acl_user->user;
      host= acl_user->host.get_host();
    break;

    case DB_ACL:
      acl_db= &acl_dbs->at(idx);
      user= acl_db->user;
      host= acl_db->host.get_host();
      break;

    case COLUMN_PRIVILEGES_HASH:
    case PROC_PRIVILEGES_HASH:
    case FUNC_PRIVILEGES_HASH:
      grant_name= (GRANT_NAME*) my_hash_element(grant_name_hash, idx);
      user= grant_name->user;
      host= grant_name->host.get_host();
      break;

    case PROXY_USERS_ACL:
      acl_proxy_user= &acl_proxy_users->at(idx);
      user= acl_proxy_user->get_user();
      host= acl_proxy_user->host.get_host();
      break;

    default:
      MY_ASSERT_UNREACHABLE();
    }
    if (! user)
      user= "";
    if (! host)
      host= "";

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  index: %zu  user: '%s'  host: '%s'",
                       struct_no, idx, user, host));
#endif
    if (strcmp(user_from->user.str, user) ||
        my_strcasecmp(system_charset_info, user_from->host.str, host))
      continue;

    result= 1; /* At least one element found. */
    if ( drop )
    {
      switch ( struct_no ) {
      case USER_ACL:
        acl_users->erase(idx);
        elements--;
        /*
        - If we are iterating through an array then we just have moved all
          elements after the current element one position closer to its head.
          This means that we have to take another look at the element at
          current position as it is a new element from the array's tail.
        - This is valid for case USER_ACL, DB_ACL and PROXY_USERS_ACL.
        */
        idx--;
        break;

      case DB_ACL:
        acl_dbs->erase(idx);
        elements--;
        idx--;
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Deleting while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.push_back(grant_name))
          DBUG_RETURN(-1);
        break;

      case PROXY_USERS_ACL:
        acl_proxy_users->erase(idx);
        elements--;
        idx--;
        break;
      }
    }
    else if ( user_to )
    {
      switch ( struct_no ) {
      case USER_ACL:
        acl_user->user= strdup_root(&global_acl_memory, user_to->user.str);
        acl_user->host.update_hostname(strdup_root(&global_acl_memory, user_to->host.str));
        break;

      case DB_ACL:
        acl_db->user= strdup_root(&global_acl_memory, user_to->user.str);
        acl_db->host.update_hostname(strdup_root(&global_acl_memory, user_to->host.str));
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Updating while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.push_back(grant_name))
          DBUG_RETURN(-1);
        break;

      case PROXY_USERS_ACL:
        acl_proxy_user->set_user(&global_acl_memory, user_to->user.str);
        acl_proxy_user->host.update_hostname((user_to->host.str && *user_to->host.str) ?
                                             strdup_root(&global_acl_memory, user_to->host.str) : NULL);
        break;
      }
    }
    else
    {
      /* If search is requested, we do not need to search further. */
      break;
    }
  }

  if (drop || user_to)
  {
    /*
      Traversing the elements stored in acl_grant_name dynamic array
      to either delete or update them.
    */
    for (GRANT_NAME **iter= acl_grant_name.begin();
         iter != acl_grant_name.end(); ++iter)
    {
      grant_name= *iter;

      if (drop)
      {
        my_hash_delete(grant_name_hash, (uchar *) grant_name);
      }
      else
      {
        /*
          Save old hash key and its length to be able properly update
          element position in hash.
        */
        char *old_key= grant_name->hash_key;
        size_t old_key_length= grant_name->key_length;

        /*
          Update the grant structure with the new user name and host name.
        */
        grant_name->set_user_details(user_to->host.str, grant_name->db,
                                     user_to->user.str, grant_name->tname,
                                     TRUE);

        /*
          Since username is part of the hash key, when the user name
          is renamed, the hash key is changed. Update the hash to
          ensure that the position matches the new hash key value
        */
        my_hash_update(grant_name_hash, (uchar*) grant_name, (uchar*) old_key,
                       old_key_length);
      }
    }
  }

#ifdef EXTRA_DEBUG
  DBUG_PRINT("loop",("scan struct: %u  result %d", struct_no, result));
#endif

  DBUG_RETURN(result);
}


/**
  Handle all privilege tables and in-memory privilege structures.
    @param  thd                 Thread handle
    @param  tables              The array with the seven open tables.
    @param  drop                If user_from is to be dropped.
    @param  user_from           The the user to be searched/dropped/renamed.
    @param  user_to             The new name for the user if to be renamed,
                                NULL otherwise.

  @note
    Go through all grant tables and in-memory grant structures and apply
    the requested operation.
    Delete from grant data if drop is true.
    Update in grant data if drop is false and user_to is not NULL.
    Search in grant data if drop is false and user_to is NULL.

  @return  operation result
    @retval > 0  At least one element matched.
    @retval 0  OK, but no element matched.
    @retval < 0  System error (OOM, error from storage engine).
*/

static int handle_grant_data(THD *thd, TABLE_LIST *tables, bool drop,
                             LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int found;
  int ret;
  Acl_table_intact table_intact(thd);
  DBUG_ENTER("handle_grant_data");

  if (drop)
  {
    /*
      Tables are defined by open_grant_tables()
      index 6 := mysql.role_edges
      index 7 := mysql.default_roles
    */
    if (revoke_all_roles_from_user(thd,
                                   tables[ACL_TABLES::TABLE_ROLE_EDGES].table,
                                   tables[ACL_TABLES::TABLE_DEFAULT_ROLES].table,
                                   user_from))
    {
      DBUG_RETURN(-1);
    }
    /* Remove all associated dynamic privileges on a best effort basis */
    Update_dynamic_privilege_table
      update_table(thd, tables[ACL_TABLES::TABLE_DYNAMIC_PRIV].table);
    result= revoke_all_dynamic_privileges(user_from->user,
                                          user_from->host,
                                          update_table);
  }

  /* Handle user table. */
  if (table_intact.check(thd, tables[ACL_TABLES::TABLE_USER].table,
                         &mysql_user_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_USER,
                                 drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    DBUG_RETURN(-1);
  }
  else
  {
    /* Handle user array. */
    if ((ret= handle_grant_struct(USER_ACL, drop, user_from, user_to) > 0) ||
         found)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle db table. */
  if (table_intact.check(thd, tables[ACL_TABLES::TABLE_DB].table,
                         &mysql_db_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_DB,
                                 drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    DBUG_RETURN(-1);
  }
  else
  {
    /* Handle db array. */
    if ((((ret= handle_grant_struct(DB_ACL, drop, user_from, user_to) > 0) &&
          ! result) || found) && ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_routine_table",
                  DBUG_SET("+d,wl7158_handle_grant_table_2"););
  /* Handle stored routines table. */
  if (table_intact.check(thd, tables[ACL_TABLES::TABLE_PROCS_PRIV].table,
                         &mysql_procs_priv_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_PROCS_PRIV,
                                 drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch in-memory array. */
    DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_routine_table",
                    DBUG_SET("-d,wl7158_handle_grant_table_2"););
    DBUG_RETURN(-1);
  }
  else
  {
    /* Handle procs array. */
    if ((((ret= handle_grant_struct(PROC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
    /* Handle funcs array. */
    if ((((ret= handle_grant_struct(FUNC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_tables_table",
                  DBUG_SET("+d,wl7158_handle_grant_table_2"););
  /* Handle tables table. */
  if (table_intact.check(thd, tables[ACL_TABLES::TABLE_TABLES_PRIV].table,
                         &mysql_tables_priv_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_TABLES_PRIV,
                                 drop, user_from, user_to)) < 0)
  {
    DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_tables_table",
                    DBUG_SET("-d,wl7158_handle_grant_table_2"););
    /* Handle of table failed, don't touch columns and in-memory array. */
    DBUG_RETURN(-1);
  }
  else
  {
    if (found && ! result)
    {
      result= 1; /* At least one record found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }

    DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_columns_table",
                    DBUG_SET("+d,wl7158_handle_grant_table_2"););

    /* Handle columns table. */
    if (table_intact.check(thd, tables[ACL_TABLES::TABLE_COLUMNS_PRIV].table,
                           &mysql_columns_priv_table_def))
    {
      result= -1;
      goto end;
    }

    if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_COLUMNS_PRIV,
                                   drop, user_from, user_to)) < 0)
    {
      DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_columns_table",
                      DBUG_SET("-d,wl7158_handle_grant_table_2"););
      /* Handle of table failed, don't touch the in-memory array. */
      DBUG_RETURN(-1);
    }
    else
    {
      /* Handle columns hash. */
      if ((((ret= handle_grant_struct(COLUMN_PRIVILEGES_HASH, drop, user_from,
                                      user_to) > 0) && ! result) || found) &&
          ! result)
        result= 1; /* At least one record/element found. */
      else if (ret < 0)
        result= -1;
    }
  }

  /* Handle proxies_priv table. */
  if (tables[ACL_TABLES::TABLE_PROXIES_PRIV].table)
  {
    DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_proxies_priv_table",
                    DBUG_SET("+d,wl7158_handle_grant_table_2"););

    if (table_intact.check(thd, tables[ACL_TABLES::TABLE_PROXIES_PRIV].table,
                           &mysql_proxies_priv_table_def))
    {
      result= -1;
      goto end;
    }

    if ((found= handle_grant_table(thd, tables, ACL_TABLES::TABLE_PROXIES_PRIV,
                                   drop, user_from, user_to)) < 0)
    {
      DBUG_EXECUTE_IF("mysql_handle_grant_data_fail_on_proxies_priv_table",
                      DBUG_SET("-d,wl7158_handle_grant_table_2"););
      /* Handle of table failed, don't touch the in-memory array. */
      DBUG_RETURN(-1);
    }
    else
    {
      /* Handle proxies_priv array. */
      if (((ret= handle_grant_struct(PROXY_USERS_ACL, drop, user_from, user_to) > 0)
           && !result) || found)
        result= 1; /* At least one record/element found. */
      else if (ret < 0)
        result= -1;
    }
  }
 end:
  DBUG_RETURN(result);
}


/*
  Create a list of users.

  SYNOPSIS
    mysql_create_user()
    thd                         The current thread.
    list                        The users to create.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_create_user(THD *thd, List <LEX_USER> &list, bool if_not_exists, bool is_role)
{
  int result= 0;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[ACL_TABLES::LAST_ENTRY];
  bool transactional_tables;
  ulong what_to_update= 0;
  bool is_anonymous_user= false;
  std::set<LEX_USER *> extra_users;
  DBUG_ENTER("mysql_create_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The binlog state will be cleared here to
    statement based replication and will be reset to the originals
    values when we are out of this function scope
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  /* CREATE USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
    DBUG_RETURN(result != 1);

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  while ((tmp_user_name= user_list++))
  {
    /*
      Ignore the current user as it already exists.
    */
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= 1;
      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                  false);
      continue;
    }

    if (set_and_validate_user_attributes(thd, user_name, what_to_update, true,
                                         is_role))
    {
      result= 1;
      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                  false);
      continue;
    }
    if (!strcmp(user_name->user.str,"") &&
        (what_to_update & PASSWORD_EXPIRE_ATTR))
    {
      is_anonymous_user= true;
      result= 1;
      continue;
    }

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    int ret;
    ret= handle_grant_data(thd, tables, 0, user_name, NULL);
    if (ret)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }
      if (if_not_exists)
      {
        String warn_user;
        append_user(thd, &warn_user, user_name, FALSE, FALSE);
        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_USER_ALREADY_EXISTS,
                            ER_THD(thd, ER_USER_ALREADY_EXISTS),
                            warn_user.c_ptr_safe());
        try
        {
          extra_users.insert(user_name);
        }
        catch (...) {
          sql_print_warning("Failed to add %s in extra_users. "
                            "Binary log entry may miss some of the users.",
                            warn_user.c_ptr_safe());
        }
        continue;
      }
      else
      {
        append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                    false);
        result= 1;
      }
      continue;
    }

    ret= replace_user_table(thd, tables[ACL_TABLES::TABLE_USER].table, user_name, 0, 0, 1, what_to_update);
    if (ret)
    {
      result= 1;
      if (ret < 0)
        break;

      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                  false);

      continue;
    }
  } // END while tmp_user_name= user_lists++

  /* In case of SE error, we would have raised error before reaching here. */
  if (result && !thd->is_error())
  {
      my_error(ER_CANNOT_USER, MYF(0), (is_role ? "CREATE ROLE" :
                                        "CREATE USER"),
               is_anonymous_user ?
                 "anonymous user" :
                 wrong_users.c_ptr_safe());
  }

  result= log_and_commit_acl_ddl(thd, transactional_tables, &extra_users);

  DBUG_RETURN(result);
}


/*
  Drop a list of users and all their privileges.

  SYNOPSIS
    mysql_drop_user()
    thd                         The current thread.
    list                        The users to drop.

  RETURN
    false       OK.
    true       Error.
*/

bool mysql_drop_user(THD *thd, List <LEX_USER> &list, bool if_exists)
{
  int result= 0;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[ACL_TABLES::LAST_ENTRY];
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  bool transactional_tables;
  DBUG_ENTER("mysql_drop_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The binlog state will be cleared here to
    statement based replication and will be reset to the originals
    values when we are out of this function scope
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  /* DROP USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
    DBUG_RETURN(result != 1);

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= 1;
      continue;
    }
    int ret= handle_grant_data(thd, tables, 1, user_name, NULL);
    if (ret <= 0)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }
      if (if_exists)
      {
        String warn_user;
        append_user(thd, &warn_user, user_name, FALSE, FALSE);
        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_USER_DOES_NOT_EXIST,
                            ER_THD(thd, ER_USER_DOES_NOT_EXIST),
                            warn_user.c_ptr_safe());
      }
      else
      {
        append_user(thd, &wrong_users, user_name, wrong_users.length() > 0, FALSE);
        result= 1;
      }
      continue;
    }
  }

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  if (result && !thd->is_error())
  {
    String operation_str;
    if (thd->query_plan.get_command() == SQLCOM_DROP_ROLE)
    {
      operation_str.append("DROP ROLE");
    }
    else
    {
      operation_str.append("DROP USER");
    }
    my_error(ER_CANNOT_USER, MYF(0), operation_str.c_ptr_quick(),
             wrong_users.c_ptr_safe());
  }

  if (!thd->is_error())
    result= populate_roles_caches(thd, (tables + ACL_TABLES::TABLE_ROLE_EDGES));

  result= log_and_commit_acl_ddl(thd, transactional_tables);

  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(result);
}


/*
  Rename a user.

  SYNOPSIS
    mysql_rename_user()
    thd                         The current thread.
    list                        The user name pairs: (from, to).

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_rename_user(THD *thd, List <LEX_USER> &list)
{
  int result= 0;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  LEX_USER *user_to, *tmp_user_to;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[ACL_TABLES::LAST_ENTRY];
  bool transactional_tables;
  DBUG_ENTER("mysql_rename_user");

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
    DBUG_RETURN(true);

  /* Is this auth id a role id? */
  {
    List_iterator <LEX_USER> authid_list_iterator(list);
    LEX_USER *authid;
    while ((authid= authid_list_iterator++))
    {
      if (!(authid= get_current_user(thd, authid)))
      {
        /*
          This user is not a role.
        */
        continue;
      }
      if (is_role_id(authid))
      {
        my_error(ER_RENAME_ROLE, MYF(0));
        DBUG_RETURN(true);
      }
    }
    acl_cache_lock.unlock();
  }

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The binlog state will be cleared here to
    statement based replication and will be reset to the originals
    values when we are out of this function scope
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  /* RENAME USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
    DBUG_RETURN(result != 1);

  if (!acl_cache_lock.lock())
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }
  while ((tmp_user_from= user_list++))
  {
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      result= 1;
      continue;
    }
    tmp_user_to= user_list++;
    if (!(user_to= get_current_user(thd, tmp_user_to)))
    {
      result= 1;
      continue;
    }
    DBUG_ASSERT(user_to != 0); /* Syntax enforces pairs of users. */

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    int ret= handle_grant_data(thd, tables, 0, user_to, NULL);

    if (ret != 0)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }

      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                  false);
      result= 1;
      continue;
    }

    ret= handle_grant_data(thd, tables, 0, user_from, user_to);

    if (ret <= 0)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }

      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0, FALSE);
      result= 1;
      continue;
    }

    Update_dynamic_privilege_table
      update_table(thd, tables[ACL_TABLES::TABLE_DYNAMIC_PRIV].table);
    if (rename_dynamic_grant(user_from->user, user_from->host, user_to->user,
                             user_to->host, update_table))
    {
      result= 1;
      break;
    }
    roles_rename_authid(thd,
                        tables[ACL_TABLES::TABLE_ROLE_EDGES].table,
                        tables[ACL_TABLES::TABLE_DEFAULT_ROLES].table,
                        user_from, user_to);
  }

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  if (result && !thd->is_error())
    my_error(ER_CANNOT_USER, MYF(0), "RENAME USER", wrong_users.c_ptr_safe());

  if (!thd->is_error())
    result= populate_roles_caches(thd, (tables + ACL_TABLES::TABLE_ROLE_EDGES));

  result= log_and_commit_acl_ddl(thd, transactional_tables);

  DBUG_RETURN(result);
}


/*
  Alter user list.

  SYNOPSIS
    mysql_alter_user()
    thd                         The current thread.
    list                        The user names.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_alter_user(THD *thd, List <LEX_USER> &list, bool if_exists)
{
  int result= 0;
  bool is_anonymous_user= false;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[ACL_TABLES::LAST_ENTRY];
  bool transactional_tables;
  bool is_privileged_user= false;
  std::set<LEX_USER *> extra_users;
  ACL_USER *self= NULL;
  bool password_expire_undo= false;
  Acl_table_intact table_intact(thd);

  DBUG_ENTER("mysql_alter_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The binlog state will be cleared here to
    statement based replication and will be reset to the originals
    values when we are out of this function scope
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
    DBUG_RETURN(result != 1);

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  if (table_intact.check(thd, tables[ACL_TABLES::TABLE_USER].table,
                         &mysql_user_table_def))
  {
    commit_and_close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  is_privileged_user= is_privileged_user_for_credential_change(thd);

  while ((tmp_user_from= user_list++))
  {
    int ret;
    ACL_USER *acl_user;
    ulong what_to_alter= 0;

    /* add the defaults where needed */
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      append_user(thd, &wrong_users, tmp_user_from, wrong_users.length() > 0,
                  false);
      result= 1;
      continue;
    }

    /* copy password expire attributes to individual lex user */
    user_from->alter_status= thd->lex->alter_password;

    if (set_and_validate_user_attributes(thd, user_from, what_to_alter,
                                         is_privileged_user, false))
    {
      result= 1;
      continue;
    }

    /*
      Check if the user's authentication method supports expiration only
      if PASSWORD EXPIRE attribute is specified
    */
    if (user_from->alter_status.update_password_expired_column &&
        !auth_plugin_supports_expiration(user_from->plugin.str))
    {
      result= 1;
      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                  false);
      continue;
    }

    if (!strcmp(user_from->user.str, "") &&
      (what_to_alter & PASSWORD_EXPIRE_ATTR) &&
        user_from->alter_status.update_password_expired_column)
    {
      result = 1;
      is_anonymous_user = true;
      continue;
    }

    acl_user= find_acl_user(user_from->host.str,
                            user_from->user.str, TRUE);

    ret= handle_grant_data(thd, tables, false, user_from, NULL);

    if (!acl_user || ret <= 0)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }

      if (if_exists)
      {
        String warn_user;
        append_user(thd, &warn_user, user_from, FALSE, FALSE);
        push_warning_printf(thd, Sql_condition::SL_NOTE,
          ER_USER_DOES_NOT_EXIST,
          ER_THD(thd, ER_USER_DOES_NOT_EXIST),
          warn_user.c_ptr_safe());
        try
        {
          extra_users.insert(tmp_user_from);
        }
        catch (...) {
          sql_print_warning("Failed to add %s in extra_users. "
                            "Binary log entry may miss some of the users.",
                            warn_user.c_ptr_safe());
        }
      }
      else
      {
        append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                    false);
        result= 1;
      }
      continue;
    }

    /* update the mysql.user table */
    ret= replace_user_table(thd, tables[ACL_TABLES::TABLE_USER].table, user_from, 0, false, true,
                            what_to_alter);
    if (ret)
    {
      if (ret < 0)
      {
        result= 1;
        break;
      }
      if (if_exists)
      {
        String warn_user;
        append_user(thd, &warn_user, user_from, FALSE, FALSE);
        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_USER_DOES_NOT_EXIST,
                            ER_THD(thd, ER_USER_DOES_NOT_EXIST),
                            warn_user.c_ptr_safe());
        try
        {
          extra_users.insert(user_from);
        }
        catch (...) {
          sql_print_warning("Failed to add %s in extra_users. "
                            "Binary log entry may miss some of the users.",
                            warn_user.c_ptr_safe());
        }
      }
      else
      {
        append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                    false);
        result= 1;
      }
      continue;
    }
    if (update_sctx_cache(thd->security_context(), acl_user,
                          user_from->alter_status.update_password_expired_column))
    {
      self= acl_user;
      password_expire_undo= !user_from->alter_status.update_password_expired_column;
    }
  }

  clear_and_init_db_cache();             // Clear locked hostname cache

  if (result && self)
    update_sctx_cache(thd->security_context(), self, password_expire_undo);

  if (result && !thd->is_error())
  {
    if (is_anonymous_user)
      my_error(ER_PASSWORD_EXPIRE_ANONYMOUS_USER, MYF(0));
    else
      my_error(ER_CANNOT_USER, MYF(0), "ALTER USER", wrong_users.c_ptr_safe());
  }

  result= log_and_commit_acl_ddl(thd, transactional_tables, &extra_users);

  DBUG_RETURN(result);
}
