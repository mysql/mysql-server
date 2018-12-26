/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "sql_bootstrap.h"
#include "sql_initialize.h"
#include "my_rnd.h"
#include "m_ctype.h"
#include "mysqld.h"
#include <my_sys.h>
#include "sql_authentication.h"
#include "log.h"
#include "sql_class.h"
#include "sql_show.h"

#include "../scripts/sql_commands_system_tables.h"
#include "../scripts/sql_commands_system_data.h"
#include "../scripts/sql_commands_help_data.h"
#include "../scripts/sql_commands_sys_schema.h"

static const char *initialization_cmds[] =
{
  "CREATE DATABASE mysql;\n",
  "USE mysql;\n",
  NULL
};

#define INSERT_USER_CMD "CREATE USER root@localhost IDENTIFIED BY '%s' PASSWORD EXPIRE;\n"
#define INSERT_USER_CMD_INSECURE "CREATE USER root@localhost;\n"
#define GENERATED_PASSWORD_LENGTH 12

char insert_user_buffer[sizeof(INSERT_USER_CMD) + GENERATED_PASSWORD_LENGTH * 2];

my_bool opt_initialize_insecure= FALSE;

static const char *initialization_data[] =
{
  "FLUSH PRIVILEGES",
  insert_user_buffer,
  "GRANT ALL PRIVILEGES ON *.* TO root@localhost WITH GRANT OPTION;\n",
  "GRANT PROXY ON ''@'' TO 'root'@'localhost' WITH GRANT OPTION;\n",
  NULL
};

static const char *session_service_initialization_data[] =
{
  "CREATE USER 'mysql.session'@localhost IDENTIFIED "
    "WITH mysql_native_password AS '*THISISNOTAVALIDPASSWORDTHATCANBEUSEDHERE' "
    "ACCOUNT LOCK;\n",
  "REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'mysql.session'@localhost;\n",
  "GRANT SELECT ON mysql.user TO 'mysql.session'@localhost;\n",
  "GRANT SELECT ON performance_schema.* TO 'mysql.session'@localhost;\n",
  "GRANT SUPER ON *.* TO 'mysql.session'@localhost;\n",
  NULL
};

static const char** cmds[]=
{
  initialization_cmds,
  mysql_system_tables,
  initialization_data,
  mysql_system_data,
  fill_help_tables,
  session_service_initialization_data,
  mysql_sys_schema,
  NULL
};

/** keep in sync with the above array */
static const char *cmd_descs[]=
{
  "Creating the system database",
  "Creating the system tables",
  "Filling in the system tables, part 1",
  "Filling in the system tables, part 2",
  "Filling in the mysql.help table",
  "Creating user for internal session service",
  "Creating the sys schema",
  NULL
};


static void generate_password(char *password, int size)
{

#define UPCHARS "QWERTYUIOPASDFGHJKLZXCVBNM"
#define LOWCHARS "qwertyuiopasdfghjklzxcvbnm"
#define NUMCHARS "1234567890"
#define SYMCHARS ",.-+*;:_!#%&/()=?><"
#define rnd_of(x) x[((int) (my_rnd_ssl(&srnd) * 100)) % \
                    (sizeof(x) - 1)]

  static const char g_allowed_pwd_chars[]=
    LOWCHARS SYMCHARS UPCHARS NUMCHARS;
  static const char g_upper_case_chars[]= UPCHARS;
  static const char g_lower_case_chars[]= LOWCHARS;
  static const char g_numeric_chars[]= NUMCHARS;
  static const char g_special_chars[]= SYMCHARS;
  rand_struct srnd;
  char *ptr= password;
  bool had_upper= false, had_lower= false,
    had_numeric= false, had_special= false;

  for (; size > 0; --size)
  {
    char ch= rnd_of(g_allowed_pwd_chars);

    /*
      Ensure we have a password that conforms to the strong
      password validation plugin ploicy by re-drawing specially
      the last 4 chars if there's need.
    */
    if (size == 4 && !had_lower)
    {
      ch= rnd_of(g_lower_case_chars);
      had_lower= true;
    }
    else if (size == 3 && !had_numeric)
    {
      ch= rnd_of(g_numeric_chars);
      had_numeric= true;
    }
    else if (size == 2 && !had_special)
    {
      ch= rnd_of(g_special_chars);
      had_special= true;
    }
    else if (size == 1 && !had_upper)
    {
      ch= rnd_of(g_upper_case_chars);
      had_upper= true;
    }

    if (!had_upper && strchr(g_upper_case_chars, ch))
      had_upper= true;
    else if (!had_lower && strchr(g_lower_case_chars, ch))
      had_lower= true;
    else if (!had_numeric && strchr(g_numeric_chars, ch))
      had_numeric= true;
    else if (!had_special && strchr(g_special_chars, ch))
      had_special= true;

    *ptr++= ch;

  }
}


/* these globals don't need protection since it's single-threaded execution */
static int cmds_ofs=0, cmd_ofs= 0;
static File_command_iterator *init_file_iter= NULL;

void Compiled_in_command_iterator::begin(void)
{
  cmds_ofs= cmd_ofs= 0;

  is_active= true;
  sql_print_information("%s", cmd_descs[cmds_ofs]);
  if (opt_initialize_insecure)
  {
    strcpy(insert_user_buffer, INSERT_USER_CMD_INSECURE);
    sql_print_warning("root@localhost is created with an empty password ! "
                      "Please consider switching off the --initialize-insecure option.");
  }
  else
  {
    char password[GENERATED_PASSWORD_LENGTH + 1];
    char escaped_password[GENERATED_PASSWORD_LENGTH * 2 + 1];
    ulong saved_verbosity= log_error_verbosity;

    generate_password(password, GENERATED_PASSWORD_LENGTH);
    password[GENERATED_PASSWORD_LENGTH]= 0;

    /*
      Temporarily bump verbosity to print the password.
      It's safe to do it since we're the sole process running.
    */
    log_error_verbosity= 3;
    sql_print_information(
      "A temporary password is generated for root@localhost: %s", password);
    log_error_verbosity= saved_verbosity;

    escape_string_for_mysql(&my_charset_bin,
                            escaped_password, sizeof(escaped_password),
                            password, GENERATED_PASSWORD_LENGTH);

    sprintf(insert_user_buffer, INSERT_USER_CMD, escaped_password);
  }
}


int Compiled_in_command_iterator::next(std::string &query, int *read_error,
                                       int *query_source)
{
  if (init_file_iter)
    return init_file_iter->next(query, read_error, query_source);

  *query_source= QUERY_SOURCE_COMPILED;
  while (cmds[cmds_ofs] != NULL && cmds[cmds_ofs][cmd_ofs] == NULL)
  {
    cmds_ofs++;
    if (cmds[cmds_ofs] != NULL)
      sql_print_information("%s", cmd_descs[cmds_ofs]);
    cmd_ofs= 0;
  }

  if (cmds[cmds_ofs] == NULL)
  {
    if (opt_init_file)
    {
      /* need to allow error reporting */
      THD *thd= current_thd;
      thd->get_stmt_da()->set_overwrite_status(true);
      init_file_iter= new File_command_iterator(opt_init_file);
      if (!init_file_iter->has_file())
      {
        sql_print_error("Failed to open the bootstrap file %s", opt_init_file);
        /* in case of error in open */
        delete init_file_iter;
        init_file_iter= NULL;
        return READ_BOOTSTRAP_ERROR;
      }
      init_file_iter->begin();
      return init_file_iter->next(query, read_error, query_source);
    }

    return READ_BOOTSTRAP_EOF;
  }

  query.assign(cmds[cmds_ofs][cmd_ofs++]);
  return READ_BOOTSTRAP_SUCCESS;
}

void Compiled_in_command_iterator::end(void)
{
  if (init_file_iter)
  {
    init_file_iter->end();
    delete init_file_iter;
    init_file_iter= NULL;
  }
  if (is_active)
  {
    sql_print_information("Bootstrapping complete");
    is_active= false;
  }
}


/**
  Create the data directory

  Creates the data directory when --initialize is specified.
  The directory is created when it does not exist.
  If it exists, is empty and the process can write into it
  no action is taken and the directory is accepted.
  Otherwise an error is thrown.
  "Empty" means no files other than the ones starting with "."
  or in the --ignore-db list.

  @param  data_home  the normalized path to the data directory
  @return status
  @retval true   failed to create. Error printed.
  @retval false  success
*/
bool initialize_create_data_directory(const char *data_home)
{
  MY_DIR *dir;
  int flags=
#ifdef _WIN32
    0
#else
    S_IRWXU | S_IRGRP | S_IXGRP
#endif
    ;

  if (NULL != (dir= my_dir(data_home, MYF(MY_DONT_SORT))))
  {
    bool no_files= true;
    char path[FN_REFLEN];
    File fd;

    /*
      Ignore files starting with . and in the --ignore-db list.
      This is exactly how find_files() in sql_show.cc operates.
    */
    for (uint i=0; i < dir->number_off_files; i++)
    {
      FILEINFO *file= dir->dir_entry + i;
      if (file->name[0] != '.' &&
          !is_in_ignore_db_dirs_list(file->name))
      {
        no_files= false;
        break;
      }
    }

    my_dirend(dir);

    if (!no_files)
    {
      sql_print_error("--initialize specified but the data directory"
                      " has files in it. Aborting.");
      return true;        /* purecov: inspected */
    }

    sql_print_information("--initialize specifed on an existing data directory.");

    if (NULL == fn_format(path, "is_writable", data_home, "",
      MY_UNPACK_FILENAME | MY_SAFE_PATH))
    {
      sql_print_error("--initialize specified but the data directory"
      " exists and the path is too long. Aborting.");
      return true;        /* purecov: inspected */

    }
    if (-1 != (fd= my_create(path, 0, flags, MYF(MY_WME))))
    {
      my_close(fd, MYF(MY_WME));
      my_delete(path, MYF(MY_WME));
    }
    else
    {
      sql_print_error("--initialize specified but the data directory"
      " exists and is not writable. Aborting.");
      return true;        /* purecov: inspected */
    }

    /* the data dir found is usable */
    return false;
  }

  sql_print_information("Creating the data directory %s", data_home);
  if (my_mkdir(data_home, flags, MYF(MY_WME)))
    return true;        /* purecov: inspected */

  return false;
}
