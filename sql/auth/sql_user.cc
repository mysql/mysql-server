/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "sql_parse.h"                  /* check_access */
#include "rpl_filter.h"                 /* rpl_filter */
#include "sql_base.h"                   /* MYSQL_LOCK_IGNORE_TIMEOUT */
#include "sql_table.h"                  /* open_ltable */
#include "password.h"                   /* my_make_scrambled_password */
#include "log_event.h"                  /* append_query_string */
#include "key.h"                        /* key_copy, key_cmp_if_same */
                                        /* key_restore */

#include "auth_internal.h"
#include "sql_auth_cache.h"
#include "sql_authentication.h"


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
               user->plugin.length) &&
        memcmp(user->plugin.str, old_password_plugin_name.str,
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
    else if (user->password.str)
    {
      str->append(STRING_WITH_LEN(" IDENTIFIED BY PASSWORD '"));
      if (user->uses_identified_by_password_clause)
      {
        str->append(user->password.str, user->password.length);
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
          my_make_scrambled_password_sha1(tmp, user->password.str,
                                          user->password.length);
          str->append(tmp);
        }
        else
        {
          /*
            Legacy password algorithm is just an obfuscation of a plain text
            so we're not going to write this.
            Same with old_passwords == 2 since the scrambled password will
            be binary anyway.
          */
          str->append("<secret>");
        }
        str->append("'");
      }
    }
  }
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS

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
                           char *new_password, uint new_password_len)
{
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return(1);
  }
  if (!thd->slave_thread &&
      (strcmp(thd->security_ctx->user, user) ||
       my_strcasecmp(system_charset_info, host,
                     thd->security_ctx->priv_host)))
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 0))
      return(1);
  }
  if (!thd->slave_thread && !thd->security_ctx->user[0])
  {
    my_message(ER_PASSWORD_ANONYMOUS_USER, ER(ER_PASSWORD_ANONYMOUS_USER),
               MYF(0));
    return(1);
  }

  return(0);
}


/**
  Change a password hash for a user.

  @param thd Thread handle
  @param host Hostname
  @param user User name
  @param new_password New password hash for host@user
 
  Note : it will also reset the change_password flag.
  This is safe to do unconditionally since the simple userless form
  SET PASSWORD = PASSWORD('text') will be the only allowed form when
  this flag is on. So we don't need to check user names here.


  @see set_var_password::update(THD *thd)

  @return Error code
   @retval 0 ok
   @retval 1 ERROR; In this case the error is sent to the client.
*/

bool change_password(THD *thd, const char *host, const char *user,
                     char *new_password)
{
  TABLE_LIST tables;
  TABLE *table;
  /* Buffer should be extended when password length is extended. */
  char buff[512];
  ulong query_length= 0;
  bool save_binlog_row_based;
  uchar user_key[MAX_KEY_LENGTH];
  char *plugin_temp= NULL;
  bool plugin_empty;
  uint new_password_len= (uint) strlen(new_password);
  bool result= 1;
  enum mysql_user_table_field password_field= MYSQL_USER_FIELD_PASSWORD;
  DBUG_ENTER("change_password");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'  new_password: '%s'",
                      host,user,new_password));
  DBUG_ASSERT(host != 0);                        // Ensured by parent

  if (check_change_password(thd, host, user, new_password, new_password_len))
    DBUG_RETURN(1);

  tables.init_one_table("mysql", 5, "user", 4, "user", TL_WRITE);

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.  It's ok to leave 'updating' set after tables_ok.
    */
    tables.updating= 1;
    /* Thanks to memset, tables.next==0 */
    if (!(thd->sp_runtime_ctx || rpl_filter->tables_ok(0, &tables)))
      DBUG_RETURN(0);
  }
#endif
  if (!(table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(1);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  mysql_mutex_lock(&acl_cache->lock);
  ACL_USER *acl_user;
  if (!(acl_user= find_acl_user(host, user, TRUE)))
  {
    mysql_mutex_unlock(&acl_cache->lock);
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    goto end;
  }
  mysql_mutex_assert_owner(&acl_cache->lock);
  table->use_all_columns();
  DBUG_ASSERT(host != '\0');
  table->field[MYSQL_USER_FIELD_HOST]->store(host, strlen(host),
                                             system_charset_info);
  table->field[MYSQL_USER_FIELD_USER]->store(user, strlen(user),
                                             system_charset_info);

  key_copy((uchar *) user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  if (!table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT))
    plugin_temp= (table->s->fields > MYSQL_USER_FIELD_PLUGIN) ?
                 get_field(&global_acl_memory, table->field[MYSQL_USER_FIELD_PLUGIN]) : NULL;
  else
    DBUG_ASSERT(FALSE);

  plugin_empty= plugin_temp ? false: true;
  
  if (acl_user->plugin.length == 0)
  {
    acl_user->plugin.length= default_auth_plugin_name.length;
    acl_user->plugin.str= default_auth_plugin_name.str;
  }

  if (new_password_len == 0)
  {
    String *password_str= new (thd->mem_root) String(new_password,
                                                     thd->variables.
                                                     character_set_client);
    if (check_password_policy(password_str))
    {
      result= 1;
      mysql_mutex_unlock(&acl_cache->lock);
      goto end;
    }
  }
  
#if defined(HAVE_OPENSSL)
  /*
    update loaded acl entry:
    TODO Should password depend on @@old_variables here?
    - Probably not if the user exists and have a plugin set already.
  */
  if (my_strcasecmp(system_charset_info, acl_user->plugin.str,
                    sha256_password_plugin_name.str) == 0)
  {
    /*
     Accept empty passwords
    */
    if (new_password_len == 0)
      acl_user->auth_string= empty_lex_str;
    /*
     Check if password begins with correct magic number
    */
    else if (new_password[0] == '$' &&
             new_password[1] == '5' &&
             new_password[2] == '$')
    {
      password_field= MYSQL_USER_FIELD_AUTHENTICATION_STRING;
      if (new_password_len < CRYPT_MAX_PASSWORD_SIZE + 1)
      {
        /*
          Since we're changing the password for the user we need to reset the
          expiration flag.
        */
        if (!update_sctx_cache(thd->security_ctx, acl_user, false) &&
            thd->security_ctx->password_expired)
        {
          /* the current user is not the same as the user we operate on */
          my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
          result= 1;
          mysql_mutex_unlock(&acl_cache->lock);
          goto end;
        }

        acl_user->password_expired= false;
        /* copy string including \0 */
        acl_user->auth_string.str= (char *) memdup_root(&global_acl_memory,
                                                       new_password,
                                                       new_password_len + 1);
        acl_user->auth_string.length= new_password_len;
      }
    } else
    {
      /*
        Password format is unexpected. The user probably is using the wrong
        password algorithm with the PASSWORD() function.
      */
      my_error(ER_PASSWORD_FORMAT, MYF(0));
      result= 1;
      mysql_mutex_unlock(&acl_cache->lock);
      goto end;
    }
  }
  else
#endif
  if (my_strcasecmp(system_charset_info, acl_user->plugin.str,
                    native_password_plugin_name.str) == 0 ||
      my_strcasecmp(system_charset_info, acl_user->plugin.str,
                    old_password_plugin_name.str) == 0)
  {
    password_field= MYSQL_USER_FIELD_PASSWORD;
    
    /*
      Legacy code produced an error if the password hash didn't match the
      expectations.
    */
    if (new_password_len != 0)
    {
      if (plugin_empty)
      {
        if (new_password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH)
          acl_user->plugin= native_password_plugin_name;
        else if (new_password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
          acl_user->plugin= old_password_plugin_name;
        else
        {
          my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
          result= 1;
          mysql_mutex_unlock(&acl_cache->lock);
          goto end;
        }
      }
      else
      {
        if (my_strcasecmp(system_charset_info, acl_user->plugin.str,
                          native_password_plugin_name.str) == 0 &&
            new_password_len != SCRAMBLED_PASSWORD_CHAR_LENGTH)
        {
          my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
          result= 1;
          mysql_mutex_unlock(&acl_cache->lock);
          goto end;
        }
        else if (my_strcasecmp(system_charset_info, acl_user->plugin.str,
                               old_password_plugin_name.str) == 0 &&
                 new_password_len != SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
        {
          my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH_323);
          result= 1;
          mysql_mutex_unlock(&acl_cache->lock);
          goto end;
        }
      }
    }
    else if (plugin_empty)
      acl_user->plugin= native_password_plugin_name;

    /*
      Update loaded acl entry in memory.
      set_user_salt() stores a binary (compact) representation of the password
      in memory (acl_user->salt and salt_len).
      set_user_plugin() sets the appropriate plugin based on password length and
      if the length doesn't match a warning is issued.
    */
    if (set_user_salt(acl_user, new_password, new_password_len))
    {
      my_error(ER_PASSWORD_FORMAT, MYF(0));
      result= 1;
      mysql_mutex_unlock(&acl_cache->lock);
      goto end;  
    }
    if (!update_sctx_cache(thd->security_ctx, acl_user, false) &&
        thd->security_ctx->password_expired)
    {
      /* the current user is not the same as the user we operate on */
      my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
      result= 1;
      mysql_mutex_unlock(&acl_cache->lock);
      goto end;
    }
  }
  else
  {
     push_warning(thd, Sql_condition::SL_NOTE,
                  ER_SET_PASSWORD_AUTH_PLUGIN, ER(ER_SET_PASSWORD_AUTH_PLUGIN));
     /*
       An undefined password factory could very well mean that the password
       field is empty.
     */
     new_password_len= 0;
  }

  if (update_user_table(thd, table,
                        acl_user->host.get_host() ? acl_user->host.get_host() : "",
                        acl_user->user ? acl_user->user : "",
                        new_password, new_password_len, password_field, false, true))
  {
    mysql_mutex_unlock(&acl_cache->lock); /* purecov: deadcode */
    goto end;
  }

  acl_cache->clear(1);                          // Clear locked hostname cache
  mysql_mutex_unlock(&acl_cache->lock);
  result= 0;
  query_length= sprintf(buff, "SET PASSWORD FOR '%-.120s'@'%-.120s'='%-.120s'",
                        acl_user->user ? acl_user->user : "",
                        acl_user->host.get_host() ? acl_user->host.get_host() : "",
                        new_password);
  result= write_bin_log(thd, true, buff, query_length,
                        table->file->has_transactions());
end:
  result|= acl_trans_commit_and_close_tables(thd);

  if (!result)
    acl_notify_htons(thd, buff, query_length);

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(result);
}


/**
  Allocates a new buffer and calculates digested password hash based on plugin
  and old_passwords. The old buffer containing the clear text password is
  simply discarded as this memory belongs to the LEX will be freed when the
  session ends.
 
  @param THD the tread handler used for allocating memory
  @param user_record[in, out] The user record
 
  @return Failure state
  @retval 0 OK
  @retval 1 ERROR
*/

int digest_password(THD *thd, LEX_USER *user_record)
{
  /* Empty passwords stay empty */
  if (user_record->password.length == 0)
    return 0;

#if defined(HAVE_OPENSSL)
  /*
    Transform password into a password hash 
  */
  if (user_record->plugin.str == sha256_password_plugin_name.str)
  {
    char *buff=  (char *) thd->alloc(CRYPT_MAX_PASSWORD_SIZE+1);
    if (buff == NULL)
      return 1;

    my_make_scrambled_password(buff, user_record->password.str,
                               user_record->password.length);
    user_record->password.str= buff;
    user_record->password.length= strlen(buff)+1;
  }
  else
#endif
  if (user_record->plugin.str == native_password_plugin_name.str ||
      user_record->plugin.str == old_password_plugin_name.str)
  {
    if (thd->variables.old_passwords == 1)
    {
      char *buff= 
        (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH_323+1);
      if (buff == NULL)
        return 1;

      my_make_scrambled_password_323(buff, user_record->password.str,
                                     user_record->password.length);
      user_record->password.str= buff;
      user_record->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH_323;
    }
    else
    {
      char *buff= 
        (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH+1);
      if (buff == NULL)
        return 1;

      my_make_scrambled_password_sha1(buff, user_record->password.str,
                                      user_record->password.length);
      user_record->password.str= buff;
      user_record->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH;
    }
  } // end if native_password_plugin_name || old_password_plugin_name
  else
  {
    user_record->password.str= 0;
    user_record->password.length= 0;
  }
  return 0;
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
  uint idx;
  uint elements;
  const char *user;
  const char *host;
  ACL_USER *acl_user= NULL;
  ACL_DB *acl_db= NULL;
  ACL_PROXY_USER *acl_proxy_user= NULL;
  GRANT_NAME *grant_name= NULL;
  /*
    Dynamic array acl_grant_name used to store pointers to all
    GRANT_NAME objects
  */
  Dynamic_array<GRANT_NAME *> acl_grant_name;
  HASH *grant_name_hash= NULL;
  DBUG_ENTER("handle_grant_struct");
  DBUG_PRINT("info",("scan struct: %u  search: '%s'@'%s'",
                     struct_no, user_from->user.str, user_from->host.str));

  LINT_INIT(user);
  LINT_INIT(host);

  mysql_mutex_assert_owner(&acl_cache->lock);

  /* Get the number of elements in the in-memory structure. */
  switch (struct_no) {
  case USER_ACL:
    elements= acl_users.elements;
    break;
  case DB_ACL:
    elements= acl_dbs.elements;
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
    elements= acl_proxy_users.elements;
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
      acl_user= dynamic_element(&acl_users, idx, ACL_USER*);
      user= acl_user->user;
      host= acl_user->host.get_host();
    break;

    case DB_ACL:
      acl_db= dynamic_element(&acl_dbs, idx, ACL_DB*);
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
      acl_proxy_user= dynamic_element(&acl_proxy_users, idx, ACL_PROXY_USER*);
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
    DBUG_PRINT("loop",("scan struct: %u  index: %u  user: '%s'  host: '%s'",
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
        delete_dynamic_element(&acl_users, idx);
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
        delete_dynamic_element(&acl_dbs, idx);
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
        if (acl_grant_name.append(grant_name))
          DBUG_RETURN(-1);
        break;

      case PROXY_USERS_ACL:
        delete_dynamic_element(&acl_proxy_users, idx);
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
        if (acl_grant_name.append(grant_name))
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
    for (int i= 0; i < acl_grant_name.elements(); ++i)
    {
      grant_name= acl_grant_name.at(i);

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


/*
  Handle all privilege tables and in-memory privilege structures.

  SYNOPSIS
    handle_grant_data()
    tables                      The array with the four open tables.
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Go through all grant tables and in-memory grant structures and apply
    the requested operation.
    Delete from grant data if drop is true.
    Update in grant data if drop is false and user_to is not NULL.
    Search in grant data if drop is false and user_to is NULL.

  RETURN
    > 0         At least one element matched.
    0           OK, but no element matched.
    < 0         Error.
*/

static int handle_grant_data(TABLE_LIST *tables, bool drop,
                             LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int found;
  int ret;
  DBUG_ENTER("handle_grant_data");

  /* Handle user table. */
  if ((found= handle_grant_table(tables, 0, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle user array. */
    if (((ret= handle_grant_struct(USER_ACL, drop, user_from, user_to) > 0) &&
         ! result) || found)
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
  if ((found= handle_grant_table(tables, 1, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
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

  /* Handle stored routines table. */
  if ((found= handle_grant_table(tables, 4, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch in-memory array. */
    result= -1;
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

  /* Handle tables table. */
  if ((found= handle_grant_table(tables, 2, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch columns and in-memory array. */
    result= -1;
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

    /* Handle columns table. */
    if ((found= handle_grant_table(tables, 3, drop, user_from, user_to)) < 0)
    {
      /* Handle of table failed, don't touch the in-memory array. */
      result= -1;
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
  if (tables[5].table)
  {
    if ((found= handle_grant_table(tables, 5, drop, user_from, user_to)) < 0)
    {
      /* Handle of table failed, don't touch the in-memory array. */
      result= -1;
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

bool mysql_create_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_created= FALSE;
  bool save_binlog_row_based;
  bool transactional_tables;
  DBUG_ENTER("mysql_create_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* CREATE USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_name= user_list++))
  {
    /*
      If tmp_user_name.user.str is == NULL then
      user_name := tmp_user_name.
      Else user_name.user := sctx->user
      TODO and all else is turned to NULL !! Why?
    */
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }

    /*
      If no plugin is given, set a default plugin
    */
    if (user_name->plugin.length == 0 && user_name->uses_identified_with_clause)
    {
      user_name->plugin.str= default_auth_plugin_name.str;
      user_name->plugin.length= default_auth_plugin_name.length;
    }

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_name, NULL))
    {
      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                  false);
      result= TRUE;
      continue;
    }

    if (replace_user_table(thd, tables[0].table, user_name, 0, 0, 1, 0))
    {
      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0,
                  false);
      result= TRUE;
      continue;
    }

    some_users_created= TRUE;
  } // END while tmp_user_name= user_lists++

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "CREATE USER", wrong_users.c_ptr_safe());

  if (some_users_created)
  {
    if (!thd->rewritten_query.length())
      result|= write_bin_log(thd, false, thd->query(), thd->query_length(),
                             transactional_tables);
    else
      result|= write_bin_log(thd, false,
                             thd->rewritten_query.c_ptr_safe(),
                             thd->rewritten_query.length(),
                             transactional_tables);
  }

  mysql_rwlock_unlock(&LOCK_grant);

  result|= acl_trans_commit_and_close_tables(thd);

  if (some_users_created && !result)
    acl_notify_htons(thd, thd->query(), thd->query_length());

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/*
  Drop a list of users and all their privileges.

  SYNOPSIS
    mysql_drop_user()
    thd                         The current thread.
    list                        The users to drop.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_drop_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_deleted= FALSE;
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  bool save_binlog_row_based;
  bool transactional_tables;
  DBUG_ENTER("mysql_drop_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* DROP USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }  
    if (handle_grant_data(tables, 1, user_name, NULL) <= 0)
    {
      append_user(thd, &wrong_users, user_name, wrong_users.length() > 0, FALSE);
      result= TRUE;
      continue;
    }
    some_users_deleted= TRUE;
  }

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "DROP USER", wrong_users.c_ptr_safe());

  if (some_users_deleted)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length(),
                            transactional_tables);

  mysql_rwlock_unlock(&LOCK_grant);

  result|= acl_trans_commit_and_close_tables(thd);

  if (some_users_deleted && !result)
    acl_notify_htons(thd, thd->query(), thd->query_length());

  thd->variables.sql_mode= old_sql_mode;
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
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
  int result;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  LEX_USER *user_to, *tmp_user_to;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_renamed= FALSE;
  bool save_binlog_row_based;
  bool transactional_tables;
  DBUG_ENTER("mysql_rename_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* RENAME USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables, &transactional_tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_from= user_list++))
  {
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      result= TRUE;
      continue;
    }  
    tmp_user_to= user_list++;
    if (!(user_to= get_current_user(thd, tmp_user_to)))
    {
      result= TRUE;
      continue;
    }  
    DBUG_ASSERT(user_to != 0); /* Syntax enforces pairs of users. */

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_to, NULL) ||
        handle_grant_data(tables, 0, user_from, user_to) <= 0)
    {
      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0, FALSE);
      result= TRUE;
      continue;
    }
    some_users_renamed= TRUE;
  }
  
  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "RENAME USER", wrong_users.c_ptr_safe());
  
  if (some_users_renamed)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length(),
                            transactional_tables);

  mysql_rwlock_unlock(&LOCK_grant);

  result|= acl_trans_commit_and_close_tables(thd);

  if (some_users_renamed && !result)
    acl_notify_htons(thd, thd->query(), thd->query_length());

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/*
  Mark user's password as expired.

  SYNOPSIS
    mysql_user_password_expire()
    thd                         The current thread.
    list                        The user names.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_user_password_expire(THD *thd, List <LEX_USER> &list)
{
  bool result= false;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables;
  TABLE *table;
  bool some_passwords_expired= false;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_user_password_expire");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(true);
  }
  tables.init_one_table("mysql", 5, "user", 4, "user", TL_WRITE);

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.  It's ok to leave 'updating' set after tables_ok.
    */
    tables.updating= 1;
    /* Thanks to memset, tables.next==0 */
    if (!(thd->sp_runtime_ctx || rpl_filter->tables_ok(0, &tables)))
      DBUG_RETURN(false);
  }
#endif
  if (!(table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(true);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_from= user_list++))
  {
    ACL_USER *acl_user;
   
    /* add the defaults where needed */
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      result= true;
      append_user(thd, &wrong_users, tmp_user_from, wrong_users.length() > 0,
                  false);
      continue;
    }

    /* look up the user */
    if (!(acl_user= find_acl_user(user_from->host.str,
                                   user_from->user.str, TRUE)))
    {
      result= true;
      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                  false);
      continue;
    }

    /* Check if the user's authentication method supports expiration */
    if (!auth_plugin_supports_expiration(acl_user->plugin.str))
    {
      result= true;
      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                  false);
      continue;
    }


    /* update the mysql.user table */
    enum mysql_user_table_field password_field= MYSQL_USER_FIELD_PASSWORD;
    if (update_user_table(thd, table,
                          acl_user->host.get_host() ?
                          acl_user->host.get_host() : "",
                          acl_user->user ? acl_user->user : "",
                          NULL, 0, password_field, true, false))
    {
      result= true;
      append_user(thd, &wrong_users, user_from, wrong_users.length() > 0,
                  false);
      continue;
    }

    acl_user->password_expired= true;
    some_passwords_expired= true;
  }

  acl_cache->clear(1);                          // Clear locked hostname cache
  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "ALTER USER", wrong_users.c_ptr_safe());

  if (!result && some_passwords_expired)
  {
    const char *query= thd->rewritten_query.length() ?
      thd->rewritten_query.c_ptr_safe() : thd->query();
    const size_t query_length= thd->rewritten_query.length() ?
      thd->rewritten_query.length() : thd->query_length();
    result= (write_bin_log(thd, false, query, query_length,
                           table->file->has_transactions()) != 0);
  }

  mysql_rwlock_unlock(&LOCK_grant);

  result|= acl_trans_commit_and_close_tables(thd);

  if (some_passwords_expired && !result)
    acl_notify_htons(thd, thd->query(), thd->query_length());

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


#endif


