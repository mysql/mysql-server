/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/



#include "client_priv.h"
#include "my_default.h"
#include "mysqld_error.h"
#include <welcome_copyright_notice.h> // ORACLE_WELCOME_COPYRIGHT_NOTICE

using namespace std;

static char **defaults_argv;
static char *opt_host= 0;
static char *opt_user= 0;
static uint opt_port= 0;
static uint opt_protocol= 0;
static char *opt_mysql_unix_port= 0;
static MYSQL mysql;
static char *password= 0;
static bool password_provided= FALSE;
#ifdef HAVE_SMEM
static char *shared_memory_base_name= 0;
#endif

#include "sslopt-vars.h"

static const char *load_default_groups[]= { "client", "mysql_secure_installation", 0 };

static struct my_option my_connection_options[]=
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", &opt_host,
   &opt_host, 0, GET_STR_ALLOC, REQUIRED_ARG,
   (longlong) "localhost", 0, 0, 0, 0, 0},
  {"password", 'p', "Password to connect to the server. If password is not "
   "given it's asked from the tty.", 0, 0, 0, GET_PASSWORD, OPT_ARG , 0, 0, 0,
   0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").", &opt_port,
   &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name,
   &shared_memory_base_name, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "Socket file to be used for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR_ALLOC, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#include "sslopt-longopts.h"
  {"user", 'u', "User for login if not current user.", &opt_user,
   &opt_user, 0, GET_STR_ALLOC, REQUIRED_ARG, (longlong) "root", 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void print_version(void)
{
  fprintf(stdout, "%s Ver %s, for %s on %s\n", my_progname,
	  MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
}

static void usage()
{
  print_version();
  fprintf(stdout, ORACLE_WELCOME_COPYRIGHT_NOTICE("2013"));
  fprintf(stdout, "MySQL Configuration Utility.");
  fprintf(stdout, "Usage: %s [OPTIONS]\n", my_progname);
  my_print_help(my_connection_options);
  my_print_variables(my_connection_options);
}

static void free_resources()
{
  if (opt_host)
    my_free(opt_host);
  if (opt_mysql_unix_port)
    my_free(opt_mysql_unix_port);
  if (opt_user)
    my_free(opt_user);
  if (password)
    my_free(password);
  mysql_close(&mysql);
  free_defaults(defaults_argv);
}
my_bool
my_arguments_get_one_option(int optid,
                            const struct my_option *opt __attribute__((unused)),
                            char *argument)
{
  switch(optid){
  case '?':
    usage();
    free_resources();
    exit(0);
  case 'p':
    if (argument)
    {
      char *start= argument;
      my_free(password);
      password= my_strdup(PSI_NOT_INSTRUMENTED,
			  argument, MYF(MY_FAE));
      while (*argument)
      {
	*argument++= 'x';               // Destroy argument
      }
      if (*start)
	start[1]= 0 ;
    }
    else
      password= get_tty_password(NullS);
    password_provided= TRUE;
    break;

#include <sslopt-case.h>
  case OPT_MYSQL_PROTOCOL:
#ifndef EMBEDDED_LIBRARY
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
				    opt->name);
#endif
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  }
  return 0;
}


/* Initialize options for the given connection handle. */
static void
init_connection_options(MYSQL *mysql)
{
  SSL_SET_OPTIONS(mysql);

  if (opt_protocol)
    mysql_options(mysql, MYSQL_OPT_PROTOCOL, (char*) &opt_protocol);

#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(mysql, MYSQL_SHARED_MEMORY_BASE_NAME, shared_memory_base_name);
#endif
}


/**
  Reads the response from stdin and returns the first character.

  @param    Optional message do be displayed.

  @return   First character of input string
*/
int get_response(const char *opt_message)
{
  int a= 0;
  int b= 0;
  int i= 0;
  if (opt_message)
    fprintf(stdout, "%s", opt_message);
  do
  {
    if (i == 1)
      b= a;
    a= getchar();
    i++;
  } while(a != '\n');
  return b;
}

/**
  Takes a mysql query and an optional message as arguments.
  It displays the message if provided one and then runs the query.
  If the query is run successfully, the success message is displayed.
  Else, the failure message along with the actual failure is displayed.
  If the server is not found running, the program is exited.

  @param1  query        The mysql query which is to be executed.
  @param2  opt_message  The optional message to be displayed.
*/
void execute_query_with_message(const char *query, const char *opt_message)
{
  if (opt_message)
    fprintf(stdout, "%s", opt_message);

  if (!mysql_query(&mysql, query))
    fprintf(stdout, " ... Success!\n");
  else if ((mysql_errno(&mysql) == ER_PROCACCESS_DENIED_ERROR) ||
           (mysql_errno(&mysql) == ER_TABLEACCESS_DENIED_ERROR) ||
           (mysql_errno(&mysql) == ER_COLUMNACCESS_DENIED_ERROR))
  {
    fprintf(stdout, "The user provided does not have enough permissions "
	            "to continue.\nmysql_secure_installation is exiting.\n");
    free_resources();
    exit(1);
  }
  else
    fprintf(stdout, " ... Failed! Error: %s\n", mysql_error(&mysql));

  if (mysql_errno(&mysql) == CR_SERVER_GONE_ERROR)
  {
    free_resources();
    exit(1);
  }
}

/**
  Takes a mysql query and the length of the query in bytes
  as the input. If the query fails on running, a message
  along with the failure details is displayed.

  @param1   query        The mysql query which is to be executed.
  @param2   length       Length of the query in bytes.

  return    FALSE in case of success
            TRUE  in case of failure
*/
bool execute_query(const char **query, unsigned int length)
{
  if (!mysql_real_query(&mysql, (const char *) *query, length))
    return FALSE;
  else if (mysql_errno(&mysql) == CR_SERVER_GONE_ERROR)
  {
    fprintf(stdout, " ... Failed! Error: %s\n", mysql_error(&mysql));
    free_resources();
    exit(1);
  }
  if ((mysql_errno(&mysql) == ER_PROCACCESS_DENIED_ERROR) ||
      (mysql_errno(&mysql) == ER_TABLEACCESS_DENIED_ERROR) ||
      (mysql_errno(&mysql) == ER_COLUMNACCESS_DENIED_ERROR))
  {
    fprintf(stdout, "The user provided does not have enough permissions "
	            "to continue.\nmysql_secure_installation is exiting.\n");
    free_resources();
    exit(1);
  }
  return TRUE;
}

/**
  Checks if the validate_password plugin is installed and returns TRUE if it is.
*/
bool validate_password_exists()
{
  MYSQL_ROW row;
  bool res= TRUE;
  const char *query= "SELECT NAME FROM mysql.plugin WHERE NAME "
                     "= \'validate_password\'";
  if (!execute_query(&query, strlen(query)))
    DBUG_PRINT("info", ("query success!"));
  MYSQL_RES *result= mysql_store_result(&mysql);
  row= mysql_fetch_row(result);
  if (!row)
    res= FALSE;

  mysql_free_result(result);
  return res;
}

/**
  Installs validate_password plugin and sets the password validation policy.

  @return   Returns 1 on successfully setting the plugin and 0 in case of
            of any error.
*/
int set_plugin()
{
  int reply;
  int plugin_set= 0;
  char *strength;
  bool option_read= FALSE;
  reply= get_response((const char *) "\n\nVALIDATE PASSWORD PLUGIN can be used "
                                     "to test passwords\nand improve security. "
				     "It checks the strength of password\nand "
				     "allows the users to set only those "
				     "passwords which are\nsecure enough. "
				     "Would you like to setup VALIDATE "
				     "PASSWORD plugin?\n\nPress y|Y for Yes, "
				     "any other key for No: ");
  if (reply == (int) 'y' || reply == (int) 'Y')
  {
#ifdef _WIN32
    const char *query_tmp;
    query_tmp= "INSTALL PLUGIN validate_password SONAME "
	       "'validate_password.dll'";
    if (!execute_query(&query_tmp, strlen(query_tmp)))
#else
    const char *query_tmp;
    query_tmp= "INSTALL PLUGIN validate_password SONAME "
	       "'validate_password.so'";
    if (!execute_query(&query_tmp, strlen(query_tmp)))
#endif
    {
      plugin_set= 1;
      while(!option_read)
      {
	reply= get_response((const char *) "\n\nThere are three levels of "
	                                   "password validation policy.\n\n"
					   "Please enter 0 for LOW, 1 for "
					   "MEDIUM and 2 for STRONG: ");
	switch (reply){
	case (int ) '0':
	  strength= (char *) "LOW";
	  option_read= TRUE;
	  break;
	case (int) '1':
	  strength= (char *) "MEDIUM";
	  option_read= TRUE;
	  break;
	case (int) '2':
	  strength= (char *) "STRONG";
	  option_read= TRUE;
	  break;
	default:
	  fprintf(stdout, "\nInvalid option provided.\n");
	}
      }
      char *query, *end;
      int tmp= sizeof("SET GLOBAL validate_password_policy = ") + 3;
      int strength_length= strlen(strength);
      /*
	query string needs memory which is atleast the length of initial part
	of query plus twice the size of variable being appended.
      */
      query= (char *)my_malloc(PSI_NOT_INSTRUMENTED,
	                       (strength_length * 2 + tmp) * sizeof(char),
	                       MYF(MY_WME));
      end= my_stpcpy(query, "SET GLOBAL validate_password_policy = ");
      *end++ = '\'';
      end+= mysql_real_escape_string(&mysql, end, strength, strength_length);
      *end++ = '\'';
      if (!execute_query((const char **) &query,(unsigned int) (end-query)))
	DBUG_PRINT("info", ("query success!"));
      my_free(query);
    }
    else
      fprintf(stdout, "\nVALIDATE PASSWORD PLUGIN is not available.\n"
	              "Proceeding with the further steps without the plugin.\n");
  }
  return(plugin_set);
}

/**
  Checks the password strength and displays it to the user.

  @param password_string    Password string whose strength
			    is to be estimated
*/
void estimate_password_strength(char *password_string)
{
  char *query, *end;
  int tmp= sizeof("SELECT validate_password_strength(") + 3;
  int password_length= strlen(password_string);
  /*
    query string needs memory which is atleast the length of initial part
    of query plus twice the size of variable being appended.
  */
  query= (char *)my_malloc(PSI_NOT_INSTRUMENTED,
                           (password_length * 2 + tmp) * sizeof(char),
                           MYF(MY_WME));
  end= my_stpcpy(query, "SELECT validate_password_strength(");
  *end++ = '\'';
  end+= mysql_real_escape_string(&mysql, end, password_string, password_length);
  *end++ = '\'';
  *end++ = ')';
  if (!execute_query((const char **) &query,(unsigned int) (end-query)))
  {
    MYSQL_RES *result= mysql_store_result(&mysql);
    MYSQL_ROW row= mysql_fetch_row(result);
    printf("\nStrength of the password: %s \n\n", row[0]);
    mysql_free_result(result);
  }
  my_free(query);
}


/**
  Sets the root password with the string provided during the flow
  of the method. It checks for the strength of the password before
  changing it and displays the same to the user. The user can decide
  if he wants to continue with the password, or provide a new one,
  depending on the strength displayed.

  @param    plugin_set   1 if validate_password plugin is set and
                         0 if it is not.
*/

static void set_root_password(int plugin_set)
{
  char *password1= 0, *password2= 0;
  int reply= 0;

  for(;;)
  {
    if (password1)
    {
      my_free(password1);
      password1= NULL;
    }
    if (password2)
    {
      my_free(password2);
      password2= NULL;
    }

    password1= get_tty_password("\nNew password: ");

    if (password1[0] == '\0')
    {
      fprintf(stdout, "Sorry, you can't use an empty password here.\n");
      continue;
    }

    password2= get_tty_password("\nRe-enter new password: ");

    if (strcmp(password1, password2))
    {
      fprintf(stdout, "Sorry, passwords do not match.\n");
      continue;
    }

    if (plugin_set == 1)
    {
      estimate_password_strength(password1);
      reply= get_response((const char *) "Do you wish to continue with the "
	                                 "password provided?(Press y|Y for "
					 "Yes, any other key for No) : ");
    }

    int pass_length= strlen(password1);

    if ((!plugin_set) || (reply == (int) 'y' || reply == (int) 'Y'))
    {
      char *query= NULL, *end;
      int tmp= sizeof("SET PASSWORD=PASSWORD(") + 3;
      /*
	query string needs memory which is atleast the length of initial part
	of query plus twice the size of variable being appended.
      */
      query= (char *)my_malloc(PSI_NOT_INSTRUMENTED,
	                       (pass_length*2 + tmp)*sizeof(char), MYF(MY_WME));
      end= my_stpcpy(query, "SET PASSWORD=PASSWORD(");
      *end++ = '\'';
      end+= mysql_real_escape_string(&mysql, end, password1, pass_length);
      *end++ = '\'';
      *end++ = ')';
      my_free(password1);
      my_free(password2);
      password1= NULL;
      password2= NULL;
      if (!execute_query((const char **)&query,(unsigned int) (end-query)))
      {
	my_free(query);
        break;
      }
      else
	fprintf(stdout, " ... Failed! Error: %s\n", mysql_error(&mysql));
    }
  }
}

/**
  Takes the root password as an input from the user and checks its validity
  by trying to connect to the server with it. The connection to the server
  is opened in this function.

  @return    Returns 1 if a password already exists and 0 if it doesn't.
*/
int get_root_password()
{
  int res;
  fprintf(stdout, "\n\n\n"
		  "NOTE: RUNNING ALL THE STEPS FOLLOWING THIS IS RECOMMENDED\n"
		  "FOR ALL MySQL SERVERS IN PRODUCTION USE!  PLEASE READ EACH\n"
		  "STEP CAREFULLY!\n\n\n\n\n");
  if (!password_provided)
  {
    fprintf(stdout, "In order to log into MySQL to secure it, we'll need the\n"
		    "current password for the root user. If you've just installed"
		    "\nMySQL, and you haven't set the root password yet, the \n"
		    "password will be blank, so you should just press enter here."
		    "\n\n");
    password= get_tty_password(NullS);
  }
  if (!mysql_real_connect(&mysql, opt_host, opt_user,
			  password, "", opt_port, opt_mysql_unix_port, 0))
  {
    if (mysql_errno(&mysql) == ER_MUST_CHANGE_PASSWORD_LOGIN)
    {
      bool can= TRUE;
      init_connection_options(&mysql);
      mysql_options(&mysql, MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS, &can);
      if (!mysql_real_connect(&mysql, opt_host, opt_user,
                              password, "", opt_port, opt_mysql_unix_port, 0))
      {
	fprintf(stdout, "Error: %s\n", mysql_error(&mysql));
	free_resources();
	exit(1);
      }
      fprintf(stdout, "\nThe existing password for the user account has "
	              "expired. Please set a new password.\n");
      set_root_password(0);
    }
    else
    {
      fprintf(stdout, "Error: %s\n", mysql_error(&mysql));
      free_resources();
      exit(1);
    }
  }
  fprintf(stdout, "\n\nOK, successfully used password, moving on...\n\n");
  res= (password[0] != '\0') ? 1 : 0;
  return(res);
}


/**
  Takes the user and the host from result set and drops those users.

  @param result    The result set from which rows are to be fetched.
*/
void drop_users(MYSQL_RES *result)
{
  MYSQL_ROW row;
  char *user_tmp, *host_tmp;
  while ((row= mysql_fetch_row(result)))
  {
    char *query, *end;
    int user_length, host_length;
    int tmp= sizeof("DROP USER ")+5;
    user_tmp= row[0];
    host_tmp= row[1];
    user_length= strlen(user_tmp);
    host_length= strlen(host_tmp);
    /*
      query string needs memory which is atleast the length of initial part
      of query plus twice the size of variable being appended.
    */
    query= (char *)my_malloc(PSI_NOT_INSTRUMENTED,
	                     ((user_length + host_length)*2 + tmp) *
	                     sizeof(char), MYF(MY_WME));
    end= my_stpcpy(query, "DROP USER ");
    *end++ = '\'';
    end+= mysql_real_escape_string(&mysql, end, user_tmp, user_length);
    *end++ = '\'';
    *end++ = '@';
    *end++ = '\'';
    end+= mysql_real_escape_string(&mysql, end, host_tmp, host_length);
    *end++ = '\'';
    if (!execute_query((const char **) &query, (unsigned int) (end-query)))
      DBUG_PRINT("info", ("query success!"));
    my_free(query);
  }
}

/**
  Removes all the anonymous users for better security.
*/
void remove_anonymous_users()
{
  int reply;
  reply= get_response((const char *) "By default, a MySQL installation has an "
				     "anonymous user,\nallowing anyone to log "
				     "into MySQL without having to have\na user "
				     "account created for them. This is intended "
				     "only for\ntesting, and to make the "
				     "installation go a bit smoother.\nYou should "
				     "remove them before moving into a production\n"
				     "environment.\n\nRemove anonymous users? "
				     "(Press y|Y for Yes, any other key for No) : ");

  if (reply == (int) 'y' || reply == (int) 'Y')
  {
    const char *query;
    query= "SELECT USER, HOST FROM mysql.user WHERE USER=''";
    if (!execute_query(&query, strlen(query)))
      DBUG_PRINT("info", ("query success!"));
    MYSQL_RES *result= mysql_store_result(&mysql);
    if (result)
      drop_users(result);
    mysql_free_result(result);
    fprintf(stdout, "\n\nSuccess.. Moving on..\n\n");
  }
  else
    fprintf(stdout, "\n ... skipping.\n\n");
}


/**
  Drops all the root users with a remote host.
*/
void remove_remote_root()
{
  int reply;
  reply= get_response((const char *) "\n\nNormally, root should only be "
                                     "allowed to connect from\n'localhost'. "
				     "This ensures that someone cannot guess at"
				     "\nthe root password from the network.\n\n"
				     "Disallow root login remotely? (Press y|Y "
				     "for Yes, any other key for No) : ");
  if (reply == (int) 'y' || reply == (int) 'Y')
  {
    const char *query;
    query= "SELECT USER, HOST FROM mysql.user WHERE USER='root' "
	   "AND HOST NOT IN ('localhost', '127.0.0.1', '::1')";
    if (!execute_query(&query, strlen(query)))
      DBUG_PRINT("info", ("query success!"));
    MYSQL_RES *result= mysql_store_result(&mysql);
    if (result)
      drop_users(result);
    mysql_free_result(result);
    fprintf(stdout, "Done.. Moving on..\n\n");
  }
  else
    fprintf(stdout, "\n ... skipping.\n");
}

/**
  Removes test database and deleteѕ the rows corresponding to them
  from mysql.db table.
*/
void remove_test_database()
{
  int reply;
  reply= get_response((const char *) "By default, MySQL comes with a database "
                                     "named 'test' that\nanyone can access. "
				     "This is also intended only for testing,\n"
				     "and should be removed before moving into "
				     "a production\nenvironment.\n\n\nRemove "
				     "test database and access to it? (Press "
				     "y|Y for Yes, any other key for No) : ");
  if (reply == (int) 'y' || reply == (int) 'Y')
  {
    execute_query_with_message((const char *) "DROP DATABASE test",
			       (const char *) " - Dropping test database...\n");

    execute_query_with_message((const char *) "DELETE FROM mysql.db WHERE "
	                                      "Db='test' OR Db='test\\_%'",
			       (const char *) " - Removing privileges on test "
			                      "database...\n");
  }
  else
    fprintf(stdout, "\n ... skipping.\n");
}

/**
  Refreshes the in-memory details through
  FLUSH PRIVILEGES.
*/
void reload_privilege_tables()
{
  int reply;
  reply= get_response((const char *) "Reloading the privilege tables will "
                                     "ensure that all changes\nmade so far "
				     "will take effect immediately.\n\nReload "
				     "privilege tables now? (Press y|Y for "
				     "Yes, any other key for No) : ");
  if (reply == (int) 'y' || reply == (int) 'Y')
  {
    execute_query_with_message((const char *) "FLUSH PRIVILEGES", NULL);
  }
  else
    fprintf(stdout, "\n ... skipping.\n");
}

int main(int argc,char *argv[])
{
  int reply;
  int rc;
  int hadpass, plugin_set= 0;

  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);
#ifdef __WIN__
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, &argc, &argv);
#endif

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my", load_default_groups, &argc, &argv))
  {
    free_defaults(argv);
    my_end(0);
    free_resources();
    exit(1);
  }
  defaults_argv= argv;
  my_getopt_use_args_separator= FALSE;

  if ((rc= handle_options(&argc, &argv, my_connection_options,
                          my_arguments_get_one_option)))
  {
    free_resources();
    exit(rc);
  }

  if (mysql_init(&mysql) == NULL)
  {
    printf("\nFailed to initate MySQL connection");
    free_resources();
    exit(1);
  }
  init_connection_options(&mysql);

  hadpass= get_root_password();

  if (!validate_password_exists())
    plugin_set= set_plugin();
  else
  {
    fprintf(stdout, "validate_password plugin is installed on the server.\n"
	            "The subsequent steps will run with the existing "
		    "configuration\nof the plugin.\n");
    plugin_set= 1;
  }
  //Set the root password
  fprintf(stdout, "\n\nSetting the root password ensures that nobody can log "
                  "into\nthe MySQL root user without the proper "
		  "authorisation.\n");

  if (!hadpass)
  {
    fprintf(stdout, "Please set the root password here.\n");
    set_root_password(plugin_set);
  }
  else
  {
    fprintf(stdout, "You already have a root password set.\n\n");

    if (plugin_set == 1)
      estimate_password_strength(password);

    reply= get_response((const char *) "Change the root password? (Press y|Y "
	                               "for Yes, any other key for No) : ");

    if (reply == (int) 'y' || reply == (int) 'Y')
      set_root_password(plugin_set);
    else
      fprintf(stdout, "\n ... skipping.\n\n");
  }

  //Remove anonymous users
  remove_anonymous_users();

  //Disallow remote root login
  remove_remote_root();

  //Remove test database
  remove_test_database();

  //Reload privilege tables
  reload_privilege_tables();

  fprintf(stdout, "All done! If you've completed all of the above steps, your\n"
                  "MySQL installation should now be secure.\n");
  free_resources();
return 0;
}
