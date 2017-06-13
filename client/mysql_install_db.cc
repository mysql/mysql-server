/*
   Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

// C headers
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

// MySQL headers
#include "my_global.h"
#include "my_default.h"
#include "my_getopt.h"
#include "welcome_copyright_notice.h"
#include "mysql_version.h"
#include "auth_utils.h"
#include "path.h"
#include "logger.h"
#include "infix_ostream_it.h"
#include "my_dir.h"

// Additional C++ headers
#include <string>
#include <algorithm>
#include <locale>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <sstream>
#include <map>
#include <iomanip>

using namespace std;

#include "../scripts/sql_commands_system_tables.h"
#include "../scripts/sql_commands_system_data.h"
#include "../scripts/sql_commands_help_data.h"
#include "../scripts/sql_commands_sys_schema.h"

#define PROGRAM_NAME "mysql_install_db"
#define MYSQLD_EXECUTABLE "mysqld"
#if defined(HAVE_YASSL)
#define MYSQL_CERT_SETUP_EXECUTABLE "mysql_ssl_rsa_setup"
#endif /* HAVE_YASSL */
#define MAX_MYSQLD_ARGUMENTS 10
#define MAX_USER_NAME_LEN 32

char *opt_euid= 0;
char *opt_basedir= 0;
char *opt_datadir= 0;
char *opt_adminlogin= 0;
char *opt_loginpath= 0;
char default_loginpath[]= "client";
char *opt_sqlfile= 0;
char default_adminuser[]= "root";
char *opt_adminuser= 0;
char default_adminhost[]= "localhost";
char *opt_adminhost= 0;
char default_authplugin[]= "mysql_native_password";
char *opt_authplugin= 0;
char *opt_mysqldfile= 0;
#if defined (HAVE_YASSL)
char *opt_mysql_cert_setup_file= 0;
char default_mysql_cert_setup_file[]= MYSQL_CERT_SETUP_EXECUTABLE;
#endif
char *opt_randpwdfile= 0;
char default_randpwfile[]= ".mysql_secret";
char *opt_langpath= 0;
char *opt_lang= 0;
char default_lang[]= "en_US";
char *opt_defaults_file= 0;
char *opt_def_extra_file= 0;
char *opt_builddir= 0;
char *opt_srcdir= 0;
my_bool opt_no_defaults= FALSE;
my_bool opt_insecure= FALSE;
my_bool opt_verbose= FALSE;
my_bool opt_ssl= FALSE;
my_bool opt_skipsys= FALSE;

/**
  Connection options.
  @note first element must be 'help' and last element must be
  the end token: {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
*/
static struct my_option my_connection_options[]=
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG,
    NO_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "The effective user id used when executing the bootstrap "
    "sequence.", &opt_euid, &opt_euid, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0,
    0, 0, 0},
  {"builddir", 0, "For use with --srcdir and out-of-source builds. Set this to "
    "the location of the directory where the built files reside.",
    &opt_builddir, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"srcdir", 0, "For internal use. This option specifies the directory under"
    " which mysql_install_db looks for support files such as the error"
    " message file and the file for populating the help tables.", &opt_srcdir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 0, "The path to the MySQL installation directory.",
    &opt_basedir, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 0, "The path to the MySQL data directory.", &opt_datadir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"login-path", 0, "Set the credential category to use with the MySQL password"
  " store when setting default credentials. This option takes precedence over "
  "admin-user, admin-host options.",
    &opt_loginpath, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
    (longlong)&default_loginpath, 0, 0, 0, 0, 0},
   {"login-file", 0, "Use the MySQL password store at the specified location "
  " to set the default password. This option takes precedence over admin-user, "
  "admin-host options. Use the login-path option to change the default "
  "credential category (default is 'client').",
    &opt_adminlogin, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"extra-sql-file", 'f', "Optional SQL file to execute during bootstrap.",
    &opt_sqlfile, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"admin-user", 0, "Username part of the default admin account.",
    &opt_adminuser, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
    (longlong)&default_adminuser, 0, 0, 0, 0, 0},
  {"admin-host", 0, "Hostname part of the default admin account.",
    &opt_adminhost, 0, 0, GET_STR_ALLOC,REQUIRED_ARG,
    (longlong)&default_adminhost, 0, 0, 0, 0, 0},
  {"admin-auth-plugin", 0, "Plugin to use for the default admin account.",
    &opt_authplugin, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
    (longlong)&default_authplugin, 0, 0, 0, 0, 0},
  {"admin-require-ssl", 0, "Require SSL/TLS for the default admin account.",
   &opt_ssl, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"mysqld-file", 0, "Qualified path to the mysqld binary.", &opt_mysqldfile,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#if defined(HAVE_YASSL)
  {"ssl-setup-file", 0, "Qualified path to the mysql_ssl_setup binary", &opt_mysql_cert_setup_file,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"random-password-file", 0, "Specifies the qualified path to the "
     ".mysql_secret temporary password file.", &opt_randpwdfile,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"insecure", 0, "Disables random passwords for the default admin account.",
   &opt_insecure, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"verbose", 'v', "Be more verbose when running program.",
   &opt_verbose, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print program version and exit.",
   &opt_verbose, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"lc-messages-dir", 'l', "Specifies the path to the language files.",
   &opt_langpath, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lc-messages", 0, "Specifies the language to use.", &opt_lang,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, (longlong)&default_lang, 0, 0, 0, 0, 0},
  {"skip-sys-schema", 0, "Skip installation of the sys schema.",
   &opt_skipsys, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* End token */
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

Log info(cout,"NOTE");
Log error(cerr,"ERROR");
Log warning(cout, "WARNING");

/**
  Escapes quotes and backslash.
  @param str The string which needs to be quoted
  @note This is not a replacement for the mysql_real_escape_string() function
  as it only does what is necessary for this application.
  @return A quoted copy of the original string.
*/
string escape_string(string str)
{
  string esc("'\"\\");
  for(string::iterator it= esc.begin(); it != esc.end(); ++it)
  {
    string::size_type idx= 0;
    while ((idx= str.find(*it, idx)) != string::npos)
    {
      str.insert(idx, 1, '\\');
      idx +=2;
    }
  }
  return str;
}

/**

*/
struct Proxy_user
{
  Proxy_user(string opt_host, string opt_user) : host(opt_host), user(opt_user)
  {}

  string host;
  string user;
  void to_str(string *sql)
  {
    sql->clear();
    sql->append("INSERT INTO proxies_priv VALUES ('");
    sql->append(escape_string(host)).
      append("','");
    sql->append(escape_string(user)).
      append("','','',TRUE,'',now());\n");
  }

};

/**
  A trivial container for attributes associated with the creation of a MySQL
  user.
*/
struct Sql_user
{
  Sql_user(string opt_host,
           string opt_user,
           string opt_password,
           Access_privilege opt_priv,
           string opt_ssl_type,
           string opt_ssl_cipher,
           string opt_x509_issuer,
           string opt_x509_subject,
           int opt_max_questions,
           int opt_max_updates,
           int opt_max_connections,
           int opt_max_user_connections,
           string opt_plugin,
           string opt_authentication_string,
           bool opt_password_expired,
           int opt_password_lifetime) :
              host(opt_host),
              user(opt_user),
              password(opt_password),
              priv(opt_priv),
              ssl_type(opt_ssl_type),
              ssl_cipher(opt_ssl_cipher),
              x509_issuer(opt_x509_issuer),
              x509_subject(opt_x509_subject),
              max_questions(opt_max_questions),
              max_updates(opt_max_updates),
              max_connections(opt_max_connections),
              max_user_connections(opt_max_user_connections),
              plugin(opt_plugin),
              authentication_string(opt_authentication_string),
              password_expired(opt_password_expired),
              password_lifetime(opt_password_lifetime) {}

  string host;
  string user;
  string password;
  Access_privilege priv;
  string ssl_type;
  string ssl_cipher;
  string x509_issuer;
  string x509_subject;
  int max_questions;
  int max_updates;
  int max_connections;
  int max_user_connections;
  string plugin;
  string authentication_string;
  bool password_expired;
  int password_lifetime;

  void to_sql(string *cmdstr)
  {
    stringstream set_passcmd,ss, flush_priv;
    ss << "INSERT INTO mysql.user VALUES ("
       << "'" << escape_string(host) << "','" << escape_string(user) << "',";

    uint64_t acl= priv.to_int();
    for(int i= 0; i< NUM_ACLS; ++i)
    {
      if( (acl & (1L << i)) != 0 )
        ss << "'Y',";
      else
        ss << "'N',";
    }
    ss << "'" << escape_string(ssl_type) << "',"
       << "'" << escape_string(ssl_cipher) << "',"
       << "'" << escape_string(x509_issuer) << "',"
       << "'" << escape_string(x509_subject) << "',"
       << max_questions << ","
       << max_updates << ","
       << max_connections << ","
       << max_user_connections << ","
       << "'" << plugin << "',";
    ss << "'',";
    if (password_expired)
      ss << "'Y',";
    else
      ss << "'N',";
    ss << "now(), NULL, 'N');\n";

    flush_priv << "FLUSH PRIVILEGES;\n";

    if (password_expired)
    {
      set_passcmd << "ALTER USER '" << escape_string(user) << "'@'"
                  << escape_string(host) << "' IDENTIFIED BY '"
                  << escape_string(password) << "' PASSWORD EXPIRE;\n";
    }
    cmdstr->append(ss.str()).append(flush_priv.str()).append(set_passcmd.str());
  }

};

static const char *load_default_groups[]= { PROGRAM_NAME, 0 };

void print_version(const string &p)
{
  cout << p
       << " Ver " << MYSQL_SERVER_VERSION << ", for "
       << SYSTEM_TYPE << " on "
       << MACHINE_TYPE << "\n";
}

void usage(const string &p)
{
  print_version(p);
  cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") << endl
       << "MySQL Database Deployment Utility." << endl
       << "Usage: "
       << p
       << " [OPTIONS]" << endl;
  my_print_help(my_connection_options);
  cout << endl
   << "The following options may be given as the first argument:"
   << endl
   << "--print-defaults        Print the program argument list and exit."
   << endl
   << "--no-defaults           Don't read default options from any option file,"
   << endl
   << "                        except for login file."
   << endl
   << "--defaults-file=#       Only read default options from the given file #."
   << endl
   << "--defaults-extra-file=# Read this file after the global files are read."
   << endl;
  my_print_variables(my_connection_options);
}


extern "C" my_bool
my_arguments_get_one_option(int optid,
                            const struct my_option *opt MY_ATTRIBUTE((unused)),
                            char *argument)
{
  switch(optid)
  {
    case '?':
      usage(PROGRAM_NAME);
      exit(0);
    case 'V':
      print_version(PROGRAM_NAME);
      exit(0);
  }
  return 0;
}


/**
  The string class will break if constructed with a NULL pointer. This wrapper
 provides a systematic protection when importing char pointers.
*/
string create_string(char *ptr)
{
  if (ptr)
    return string(ptr);
  else
    return string("");
}

template <class InputIterator, class UnaryPredicate >
bool my_all_of(InputIterator first, InputIterator last, UnaryPredicate pred)
{
  while (first != last)
  {
    if (!pred(*first)) return false;
    ++first;
  }
  return true;
}

bool my_legal_username_chars(const char &c)
{
  return isalnum(c) || c == '_';
}

bool my_legal_hostname_chars(const char &c)
{
  return isalnum(c) || c == '_' || c == '.';
}

bool my_legal_plugin_chars(const char &c)
{
  return isalnum(c) || c == '_';
}

// defined in auth_utils.cc
extern const string g_allowed_pwd_chars;

bool my_legal_password(const char &c)
{
  return (get_allowed_pwd_chars().find(c) != string::npos);
}

/**
  Verify that the default admin account follows the recommendations and
  restrictions.
*/
bool assert_valid_root_account(const string &username, const string &host,
                                  const string &plugin, bool ssl)
{
  if( username.length() > MAX_USER_NAME_LEN || username.length() < 1)
  {
    error << "Username must be between 1 and "
          << MAX_USER_NAME_LEN
          << " characters in length."
          << endl;
    return false;
  }
  if (!my_all_of(username.begin(), username.end(), my_legal_username_chars) ||
      !my_all_of(host.begin(), host.end(), my_legal_hostname_chars))
  {
    error << "Recommended practice is to use only alpha-numericals in "
             "the user / host name."
          << endl;
    return false;
  }
  if (!my_all_of(plugin.begin(), plugin.end(), my_legal_plugin_chars))
  {
    error << "Only use alpha-numericals in the the plugin name."
          << endl;
    return false;
  }
  if (plugin != "mysql_native_password" && plugin != "sha256_password")
  {
    error << "Unsupported authentication plugin specified."
          << endl;
    return false;
  }
  return true;
}

bool assert_valid_datadir(const string &datadir, Path *target)
{
  if (datadir.length() == 0)
  {
    error << "The data directory needs to be specified."
          << endl;
    return false;
  }

  target->append(datadir);

  if (target->exists())
  {
    if (!target->empty())
    {
      error << "The data directory '"
            << datadir.c_str()
            << "' already exist and is not empty." << endl;
      return false;
    }
  }
  return true;
}

class File_exists
{
public:
  File_exists(const string *file, Path *qp) : m_file(file), m_qpath(qp)
  {}

  bool operator()(const Path &path)
  {
    Path tmp_path(path);
    tmp_path.filename(*m_file);
    if (tmp_path.exists())
    {
      m_qpath->path(path);
      m_qpath->filename(*m_file);
      return true;
    }
    return false;
  }

private:
  const string *m_file;
  Path *m_qpath;
};


/**
 Given a list of search paths; find the file.
 If search_paths=0 then the filename is considered to be a qualified path.
 If filename is empty then the qpath will be the first directory which
 is found.
 @param filename The file to look for
 @search_paths paths to search
 @qpath[out] The qualified path to the first found file

 @return true if a file is found, false if not.
*/

bool locate_file(const string &filename, vector<Path > *search_paths,
                  Path *qpath)
{
  if (search_paths == 0)
  {
    MY_STAT s;
    if (my_stat(filename.c_str(), &s, MYF(0)) == NULL)
      return false;
    qpath->qpath(filename);
  }
  else
  {
    vector<Path>::iterator qpath_it=
      find_if(search_paths->begin(), search_paths->end(),
              File_exists(&filename, qpath));
    if (qpath_it == search_paths->end())
      return false;
  }
  return true;
}

void add_standard_search_paths(vector<Path > *spaths)
{
  Path p;
  if (!p.path_getcwd())
    warning << "Can't determine current working directory." << endl;

  spaths->push_back(p);
  spaths->push_back(Path(p).append("/bin"));
  spaths->push_back(Path("/usr/bin"));
  spaths->push_back(Path("/usr/local/bin"));
  spaths->push_back(Path("/opt/mysql/bin"));
#ifdef INSTALL_SBINDIR
  spaths->push_back(Path(INSTALL_SBINDIR));
#endif
#ifdef INSTALL_BINDIR
  spaths->push_back(Path(INSTALL_BINDIR));
#endif

}

/**
 Attempts to locate the mysqld file.
 If opt_mysqldfile is specified then the this assumed to be a correct qualified
 path to the mysqld executable.
 If opt_basedir is specified then opt_basedir+"/bin" is assumed to be a
 candidate path for the mysqld executable.
 If opt_srcdir is set then opt_srcdir+"/bin" is assumed to be a
 candidate path for the mysqld executable.
 If opt_builddir is set then opt_builddir+"/sql" is assumed to be a
 candidate path for the mysqld executable.

 If the executable isn't found in any of these locations,
 attempt to search the local directory and "bin" and "sbin" subdirectories.
 Finally check "/usr/bin","/usr/sbin", "/usr/local/bin","/usr/local/sbin",
 "/opt/mysql/bin","/opt/mysql/sbin"

*/
bool assert_mysqld_exists(const string &opt_mysqldfile,
                            const string &opt_basedir,
                            const string &opt_builddir,
                            const string &opt_srcdir,
                            Path *qpath)
{
  vector<Path > spaths;
  if (opt_mysqldfile.length() > 0)
  {
    /* Use explicit option to file mysqld */
    if (!locate_file(opt_mysqldfile, 0, qpath))
    {
      error << "No such file: " << opt_mysqldfile << endl;
      return false;
    }
  }
  else
  {
    if (opt_basedir.length() > 0)
    {
      spaths.push_back(Path(opt_basedir).
        append("bin"));
      /* cater for RPM installs : mysqld in sbin */
      spaths.push_back(Path(opt_basedir).
        append("sbin"));
    }
    if (opt_builddir.length() > 0)
    {
      spaths.push_back(Path(opt_builddir).
        append("sql"));
    }

    add_standard_search_paths(&spaths);

    if (!locate_file(MYSQLD_EXECUTABLE, &spaths, qpath))
    {
      error << "Can't locate the server executable (mysqld)." << endl;
      info << "The following paths were searched: ";
      copy(spaths.begin(), spaths.end(),
           infix_ostream_iterator<Path >(info, ", "));
      info << endl;
      return false;
    }
  }
  return true;
}

#if defined(HAVE_YASSL)
/**
 Attempts to locate the mysql_ssl_rsa_setup file.
 If opt_mysql_cert_setup_file is specified then the this assumed to be a
 correct qualified path to the mysql_ssl_rsa_setup executable.
 If opt_basedir is specified then opt_basedir+"/bin" is assumed to be a
 candidate path for the mysql_ssl_rsa_setup executable.
 If opt_srcdir is set then opt_srcdir+"/bin" is assumed to be a
 candidate path for the mysql_ssl_rsa_setup executable.
 If opt_builddir is set then opt_builddir+"/client" is assumed to be a
 candidate path for the mysql_system_tables executable.

 If the executable isn't found in any of these locations,
 attempt to search the local directory and "bin" subdirectory.
 Finally check "/usr/bin","/usr/sbin", "/usr/local/bin","/usr/local/sbin",
 "/opt/mysql/bin","/opt/mysql/sbin"

*/
bool assert_cert_generator_exists(const string &opt_mysql_cert_setup_file,
                                  const string &opt_basedir,
                                  const string &opt_builddir,
                                  const string &opt_srcdir,
                                  Path *qpath)
{
  vector<Path > spaths;
  if (opt_mysql_cert_setup_file.length() > 0)
  {
    /* Use explicit option to file mysql_ssl_rsa_setup */
    if (!locate_file(opt_mysql_cert_setup_file, 0, qpath))
    {
      error << "No such file: " << opt_mysql_cert_setup_file << endl;
      return false;
    }
  }
  else
  {
    if (opt_basedir.length() > 0)
    {
      spaths.push_back(Path(opt_basedir).
        append("bin"));
    }
    if (opt_builddir.length() > 0)
    {
      spaths.push_back(Path(opt_builddir).
        append("client"));
    }

    add_standard_search_paths(&spaths);

    if (!locate_file(MYSQL_CERT_SETUP_EXECUTABLE, &spaths, qpath))
    {
      error << "Can't locate the server executable (mysql_ssl_rsa_setup)."
            << endl;
      info << "The following paths were searched: ";
      copy(spaths.begin(), spaths.end(),
           infix_ostream_iterator<Path >(info, ", "));
      info << endl;
      return false;
    }
  }
  return true;
}
#endif /* HAVE_YASSL */


bool assert_valid_language_directory(const string &opt_langpath,
                                     const string &opt_basedir,
                                     const string &opt_builddir,
                                     const string &opt_srcdir,
                                     Path *language_directory)
{
  vector<Path > search_paths;
  bool found_subdir= false;
  if (opt_langpath.length() > 0)
  {
    search_paths.push_back(opt_langpath);
  }
  else
  {
    if(opt_basedir.length() > 0)
    {
      Path ld(opt_basedir);
      ld.append("/share/english");
      search_paths.push_back(ld);

      /* cater for RPMs */
      Path ld2(opt_basedir);
      ld2.append("/share/mysql/english");
      search_paths.push_back(ld2);
    }
    if (opt_builddir.length() > 0)
    {
      Path ld(opt_builddir);
      ld.append("/sql/share/english");
      search_paths.push_back(ld);
    }
    if (opt_srcdir.length() > 0)
    {
      Path ld(opt_srcdir);
      ld.append("/sql/share/english");
      search_paths.push_back(ld);
    }
    search_paths.push_back(Path("/usr/share/mysql/english"));
    search_paths.push_back(Path("/opt/mysql/share/english"));
#ifdef INSTALL_MYSQLSHAREDIR
    search_paths.push_back(Path(INSTALL_MYSQLSHAREDIR).append("/english"));
#endif
    found_subdir= true;
  }

  if (!locate_file("", &search_paths, language_directory))
  {
    error << "Can't locate the language directory." << endl;
    info << "Attempted the following paths: ";
    copy(search_paths.begin(), search_paths.end(),
         infix_ostream_iterator<Path>(info, ", "));
    info << endl;
    return false;
  }
  if (found_subdir)
    language_directory->up();
  return true;
}

/**
  Parse the login.cnf file and extract the missing admin credentials.
  If any of adminuser or adminhost contains information, it won't be overwritten
  by new data. Password is always updated.

  @return Error
    @retval ALL_OK Reporting success
    @retval ERR_FILE File not found
    @retval ERR_ENCRYPTION Error while decrypting
    @retval ERR_SYNTAX Error while parsing
*/
int get_admin_credentials(const string &opt_adminlogin,
                          const string &login_path,
                          string *adminuser,
                          string *adminhost,
                          string *password)
{
  Path path;
  int ret= ERR_OTHER;
  if (!path.qpath(opt_adminlogin) || !path.exists())
    return ERR_FILE;

  ifstream fin(opt_adminlogin.c_str(), ifstream::binary);
  stringstream sout;
  if (decrypt_login_cnf_file(fin, sout) != ALL_OK)
    return ERR_ENCRYPTION;

  map<string, string > options;

  if ((ret= parse_cnf_file(sout, &options, login_path)) != ALL_OK)
    return ret;

  for( map<string, string >::iterator it= options.begin();
       it != options.end(); ++it)
  {
    if (it->first == "user")
      *adminuser= it->second;
    if (it->first == "host")
      *adminhost= it->second;
    if (it->first == "password")
      *password= it->second;
  }
  return ALL_OK;
}

void create_ssl_policy(string *ssl_type, string *ssl_cipher,
                         string *x509_issuer, string *x509_subject)
{
  /* TODO set up a specific SSL restriction on the default account */
  *ssl_type= "ANY";
  *ssl_cipher= "";
  *x509_issuer= "";
  *x509_subject= "";
}

#if defined(HAVE_YASSL)

class SSL_generator_writer
{
public:
  bool operator()(int fh MY_ATTRIBUTE((unused)))
  {
    return true;
  }
};

#endif /* HAVE_YASSL */

#define  READ_BUFFER_SIZE 2048
#define TIMEOUT_IN_SEC 30

class Process_reader
{
public:
  Process_reader(string *buffer) : m_buffer(buffer)
  {}
  bool operator()(int fh)
  {
    errno= 0;
    char ch[READ_BUFFER_SIZE];
    ssize_t n= 1;
    int select_ret= 0;
    fd_set rd, ex;
    struct timeval tm;
    tm.tv_sec = TIMEOUT_IN_SEC;
    tm.tv_usec = 0;

    int flags = fcntl(fh, F_GETFL, 0);
    fcntl(fh, F_SETFL, flags | O_NONBLOCK);
    FD_ZERO(&rd);
    FD_ZERO(&ex);
    FD_SET(fh, &rd);
    FD_SET(fh, &ex);
    errno= 0;
    /* Wait for something to read */
    if ((select_ret= select(fh + 1, &rd, NULL, &ex, &tm)) == 0)
    {
        /* if 30 s passed we attempt to read anyway */
        warning << "select() timed out." << endl;
    }
    /* Read any error reports from the child process */
    while((n= read(fh, ch, READ_BUFFER_SIZE)) > 0 && ch[0] != 0 && errno == 0)
    {
      m_buffer->append(ch, n);
    }

    return true;
  }

private:
  string *m_buffer;

};

struct Delimiter_parser
{
  Delimiter_parser() : m_delimiter(";") {}
  ~Delimiter_parser() {}

 bool operator()(std::string &line);
private:
  string m_agg;
  string m_delimiter;
};

bool Delimiter_parser::operator()(std::string &line)
{
  if (line.empty() || line.size() == m_delimiter.size())
    return false;
  const string delimiter_tok("delimiter ");

  string::size_type pos= line.find(delimiter_tok);
  string::size_type curr_del_pos= line.find(m_delimiter);

  if (pos != string::npos && curr_del_pos != string::npos)
  {
    /* replace old delimiter with new */
    m_delimiter= line.substr(pos+delimiter_tok.size(), curr_del_pos - (pos+delimiter_tok.size()));
    line.clear();
    return false;
  }

  if (curr_del_pos != string::npos)
  {
    line.erase(curr_del_pos, m_delimiter.length());
    line.append(";\n");
    line= m_agg.append(line);
    m_agg.clear();
  }
  else
  {
    m_agg.append(line.append(" "));
    line.clear();
    return false;
  }
  return true; /* line has delimiter */
}

struct Comment_extractor
{
  Comment_extractor() : m_in_comment(false) {}
  ~Comment_extractor() {}

 bool operator()(string &line);
private:
  bool m_in_comment;
};

bool Comment_extractor::operator ()(string& line)
{

  /* Are we in a multi comment? */
  if (m_in_comment)
  {
    string::size_type i= line.find("*/");
    if (i != string::npos)
    {
      m_in_comment= false;
      string::iterator b= line.begin();
      advance(b, i+2);
      line.erase(line.begin(), b);
      if (line.empty())
        return true;
    }
    else
    {
      /* We're still in a multi comment clear the line */
      line.clear();
    }
  }
  else
  {
    string::size_type i= line.find("--");
    if (i != string::npos)
    {
      string::iterator it= line.begin();
      advance(it, i);
      line.erase(it,line.end());
      return true;
    }
    i= line.find("/*");
    if (i != string::npos)
    {
      m_in_comment= true;
      string::iterator a= line.begin();
      advance(a, i);
      string::iterator b;
      string::size_type j= line.find("*/");
      if (j != string::npos)
      {
        b= line.begin();
        advance(b, j+2);
        m_in_comment= false;
      }
      else
        b= line.end();
      line.erase(a, b);
      if (line.empty())
        return true;
    }
  }

  return m_in_comment;
}

class Process_writer
{
public:
  Process_writer(Sql_user *user, const string &opt_sqlfile) : m_user(user),
    m_opt_sqlfile(opt_sqlfile) {}
  bool operator()(int fh)
  {
    errno= 0;
    info << "Creating system tables...";

    string create_db("CREATE DATABASE mysql;\n");
    string use_db("USE mysql;\n");
    // ssize_t write() may be declared with attribute warn_unused_result
    size_t w1= write(fh, create_db.c_str(), create_db.length());
    size_t w2= write(fh, use_db.c_str(), use_db.length());
    if (w1 != create_db.length() || w2 != use_db.length())
    {
      info << "failed." << endl;
      return false;
    }

    unsigned s= 0;
    s= sizeof(mysql_system_tables)/sizeof(*mysql_system_tables);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0 &&
        mysql_system_tables[i] != NULL; ++i)
    {
      n= write(fh, mysql_system_tables[i],
               strlen(mysql_system_tables[i]));
    }
    if (errno != 0)
    {
      info << "failed." << endl;
      return false;
    }
    else
      info << "done." << endl;

    info << "Filling system tables with data...";
    s= sizeof(mysql_system_data)/sizeof(*mysql_system_data);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0 &&
        mysql_system_data[i] != NULL; ++i)
    {
      n= write(fh, mysql_system_data[i],
               strlen(mysql_system_data[i]));
    }
    if (errno != 0)
    {
      info << "failed." << endl;
      return false;
    }
    else
      info << "done." << endl;

    info << "Filling help table with data...";
    s= sizeof(fill_help_tables)/sizeof(*fill_help_tables);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0 &&
        fill_help_tables[i] != NULL; ++i)
    {
      n= write(fh, fill_help_tables[i],
               strlen(fill_help_tables[i]));
    }
    if (errno != 0)
    {
      info << "failed." << endl;
      return false;
    }
    else
      info << "done." << endl;

    info << "Creating user for internal session service...";

    string create_session_serv_user(
      "INSERT IGNORE INTO mysql.user VALUES ('localhost','mysql.session',"
        "'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','Y','N',"
        "'N','N','N','N','N','N','N','N','N','N','N','N','','','','',0,0,0,0,"
        "'mysql_native_password','*THISISNOTAVALIDPASSWORDTHATCANBEUSEDHERE',"
        "'N',CURRENT_TIMESTAMP,NULL,'Y');\n");
    string select_table_priv(
      "INSERT IGNORE INTO mysql.tables_priv VALUES ('localhost', 'mysql',"
        " 'mysql.session', 'user', 'root@localhost', CURRENT_TIMESTAMP,"
        " 'Select', '');\n"
      );
    string select_db_priv(
      "INSERT IGNORE INTO mysql.db VALUES ('localhost', 'performance_schema',"
        " 'mysql.session','Y','N','N','N','N','N','N','N','N','N','N','N',"
        "'N','N','N','N','N','N','N');\n"
    );

    w1= write(fh, create_session_serv_user.c_str(),
              create_session_serv_user.length());
    w2= write(fh, select_table_priv.c_str(), select_table_priv.length());
    size_t w3= write(fh, select_db_priv.c_str(), select_db_priv.length());

    if (w1 != create_session_serv_user.length() ||
        w2 != select_table_priv.length() ||
        w3 != select_db_priv.length())
    {
      info << "failed." << endl;
      return false;
    }
    else
      info << "done." << endl;

    info << "Creating default user " << m_user->user << "@"
         << m_user->host
         << endl;
    string create_user_cmd;
    m_user->to_sql(&create_user_cmd);
    w1= write(fh, create_user_cmd.c_str(), create_user_cmd.length());
    if (w1 !=create_user_cmd.length() || errno != 0)
      return false;
    info << "Creating default proxy " << m_user->user << "@"
         << m_user->host
         << endl;
    Proxy_user proxy_user(m_user->host, m_user->user);
    string create_proxy_cmd;
    proxy_user.to_str(&create_proxy_cmd);
    w1= write(fh, create_proxy_cmd.c_str(), create_proxy_cmd.length());
    if (w1 != create_proxy_cmd.length() || errno != 0)
      return false;

    if (!opt_skipsys)
    {
      info << "Creating sys schema" << endl;
      s= sizeof(mysql_sys_schema)/sizeof(*mysql_sys_schema);
      for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0 &&
          mysql_sys_schema[i] != NULL; ++i)
      {
         n= write(fh, mysql_sys_schema[i],
                  strlen(mysql_sys_schema[i]));
      }
      if (errno != 0)
      {
        info << "failed." << endl;
        return false;
      }
      else
        info << "done." << endl;
    }

    /* Execute optional SQL from a file */
    if (m_opt_sqlfile.length() > 0)
    {
      Path extra_sql;
      extra_sql.qpath(m_opt_sqlfile);
      if (!extra_sql.exists())
      {
        warning << "No such file '" << extra_sql.to_str() << "' "
                << "(skipping)"
                << endl;
      } else
      {
        info << "Executing extra SQL commands from " << extra_sql.to_str()
             << endl;
        ifstream fin(extra_sql.to_str().c_str());
        string sql_command;
        string default_se_command("SET default_storage_engine=INNODB;\n");
        int n= write(fh, default_se_command.c_str(), default_se_command.length());
        Comment_extractor strip_comments;
        Delimiter_parser check_delimiters;
        while (!getline(fin, sql_command).eof() && errno != EPIPE &&
             n != 0)
        {
          bool is_comment= strip_comments(sql_command);
          if (!is_comment)
          {
            bool has_delimiter= check_delimiters(sql_command);
            if (!has_delimiter)
              continue;
            n= write(fh, sql_command.c_str(), sql_command.length());
          }
        }
        fin.close();
      }
    }
    return true;
  }
private:
  Sql_user *m_user;
  string m_opt_sqlfile;
};

struct Reader_thd_command_st
{
  Process_reader *reader_functor;
  int read_hndl;
};

static void *reader_func_adaptor(void *f)
{
  Reader_thd_command_st *cmd= static_cast<Reader_thd_command_st*>(f);
  (*cmd->reader_functor)(cmd->read_hndl);
  return NULL;
}


template <typename Reader_func_t, typename Writer_func_t,
          typename Fwd_iterator >
bool process_execute(const string &exec, Fwd_iterator begin,
                       Fwd_iterator end, Reader_func_t reader,
                       Writer_func_t writer)
{
  pid_t child;
  bool retval= true;
  int read_pipe[2];
  int write_pipe[2];
  posix_spawn_file_actions_t spawn_action;
  char *execve_args[MAX_MYSQLD_ARGUMENTS];

  /*
    Disable any signal handler for broken pipes and check for EPIPE during
    IO instead.
  */
  signal(SIGPIPE, SIG_IGN);
  if (pipe(read_pipe) < 0)
  {
    return false;
  }
  if (pipe(write_pipe) < 0)
  {
    ::close(read_pipe[0]);
    ::close(read_pipe[1]);
    return false;
  }

  posix_spawn_file_actions_init(&spawn_action);
  /* target process std input (0) reads from our write_pipe */
  posix_spawn_file_actions_adddup2(&spawn_action, write_pipe[0], 0);
  /* target process shouldn't attempt to write to this pipe */
  posix_spawn_file_actions_addclose(&spawn_action, write_pipe[1]);

  /* target process output (1) is mapped to our read_pipe */
  posix_spawn_file_actions_adddup2(&spawn_action, read_pipe[1], 2);
  /* target process shouldn't attempt to read from this pipe */
  posix_spawn_file_actions_addclose(&spawn_action, read_pipe[0]);

  /*
    We need to copy the strings or spawn will fail
  */
  char *local_filename= strdup(exec.c_str());
  execve_args[0]= local_filename;
  int i= 1;
  for(Fwd_iterator it= begin;
      it!= end && i < MAX_MYSQLD_ARGUMENTS-1;)
  {
    execve_args[i]= strdup(const_cast<char *>((*it).c_str()));
    ++it;
    ++i;
  }
  execve_args[i]= 0;

  int ret= posix_spawnp(&child, (const char *)execve_args[0], &spawn_action,
                        NULL, execve_args, NULL);

  /* This end is for the target process to read from */
  ::close(write_pipe[0]);
  /* This end is for the target process to write to */
  ::close(read_pipe[1]);

  if (ret != 0)
  {
    /* always failure if we get here! */
    error << "Child process: " << exec <<
             " exited with return value " << ret << endl;
    exit(1);
  }
  else
  {
    pthread_t thd_id;
    Reader_thd_command_st cmd;
    cmd.read_hndl= read_pipe[0];
    cmd.reader_functor= &reader;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    pthread_create(&thd_id, NULL, reader_func_adaptor, &cmd);
    if (!writer(write_pipe[1]) || errno != 0)
    {
      error << "Child process: " << exec <<
               "terminated prematurely with errno= "
            << errno
            << endl;
      retval= false;
    }
    // join with read thread
    void *ret= NULL;
    pthread_cancel(thd_id); // break select()
    pthread_join(thd_id, &ret);
  }

  while(i > 0)
  {
    free(execve_args[i]);
    --i;
  }
  ::close(write_pipe[1]);
  ::close(read_pipe[0]);

  /* Wait for the child to die */
  int signal= 0;
  waitpid(child, &signal, 0);

  posix_spawn_file_actions_destroy(&spawn_action);
  free(local_filename);
  return retval;
}

int generate_password_file(Path &pwdfile, const string &adminuser,
                           const string &adminhost,
                           const string &password)
{

  /*
    The format of the password file is
    ['#'][bytes]['\n']['password bytes']['\n']|[EOF])
  */
  ofstream fout;
  mode_t old_mask= umask(~(S_IRWXU));
  fout.open(pwdfile.to_str().c_str());
  if (!fout.is_open())
  {
    umask(old_mask);
    return ERR_FILE;
  }

  fout << "# Password set for user '"
       << adminuser << "@" << adminhost << "' at "
       << Datetime() << "\n"
       << password << "\n";
  fout.close();
  info << "done." << endl;
  umask(old_mask);
  return ALL_OK;
}

int connection_options_sorter(const void *a, const void *b)
{
  return strcmp(static_cast<const my_option*>(a)->name,
                static_cast<const my_option *>(b)->name);
}

static int is_prefix(const char *s, const char *t)
{
  while (*t)
    if (*s++ != *t++) return 0;
  return 1;
}

static int real_get_defaults_options(int argc, char **argv,
                                     my_bool *no_defaults,
                                     char **defaults,
                                     char **extra_defaults,
                                     char **group_suffix,
                                     char **login_path)
{
  char **argv_it= argv;
  int org_argc= argc, prev_argc= 0, default_option_count= 0;

  while (argc >= 2 && argc != prev_argc)
  {
    /* Skip program name or previously handled argument */
    argv_it++;
    prev_argc= argc;                            /* To check if we found */
    /* --no-defaults is always the first option. */
    if (is_prefix(*argv_it,"--no-defaults") && ! default_option_count)
    {
       argc--;
       default_option_count ++;
       *no_defaults= TRUE;
       continue;
    }
    if (!*defaults && is_prefix(*argv_it, "--defaults-file="))
    {
      *defaults= *argv_it + sizeof("--defaults-file=")-1;
       argc--;
       default_option_count ++;
       continue;
    }
    if (!*extra_defaults && is_prefix(*argv_it, "--defaults-extra-file="))
    {
      *extra_defaults= *argv_it + sizeof("--defaults-extra-file=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
    if (!*group_suffix && is_prefix(*argv_it, "--defaults-group-suffix="))
    {
      *group_suffix= *argv_it + sizeof("--defaults-group-suffix=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
    if (!*login_path && is_prefix(*argv_it, "--login-path="))
    {
      *login_path= *argv_it + sizeof("--login-path=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
  }
  return org_argc - argc;
}


class Resource_releaser
{
  char **m_argv;

public:
  explicit Resource_releaser(char **argv)
    : m_argv(argv) { }

  ~Resource_releaser()
  {
    free_defaults(m_argv);
    my_cleanup_options(my_connection_options);
  }
};


int main(int argc,char *argv[])
{
  /*
    In order to use the mysys library and the program option library
    we need to call the MY_INIT() macro.
  */
  MY_INIT(argv[0]);

  char *dummy= 0; // ignore group suffix when transferring to mysqld
  /* Remember the defaults argument so we later can pass these to mysqld */
  int default_opt_used= real_get_defaults_options(argc, argv,
                                           &opt_no_defaults,
                                           &opt_defaults_file,
                                           &opt_def_extra_file,
                                           &dummy,
                                           &opt_loginpath);
#ifdef __WIN__
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, &argc, &argv);
#endif

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my", load_default_groups, &argc, &argv))
    return 1;

  // Remember to call free_defaults() and my_cleanup_options()
  Resource_releaser resource_releaser(argv);

  // Assert that the help messages are in sorted order
  // except that --help must be the first element and 0 must indicate the end.
  my_qsort(my_connection_options + 1,
           sizeof(my_connection_options)/sizeof(my_option) - 2,
           sizeof(my_option),
           connection_options_sorter);

  int rc= 0;
  if ((rc= handle_options(&argc, &argv, my_connection_options,
                             my_arguments_get_one_option)))
  {
    error << "Unrecognized options" << endl;
    return 1;
  }

  warning << "mysql_install_db is deprecated. ";
  warning << "Please consider switching to mysqld --initialize" << endl;

  bool expire_password= !opt_insecure;
  string adminuser(create_string(opt_adminuser));
  string adminhost(create_string(opt_adminhost));
  string authplugin(create_string(opt_authplugin));
  string password;
  string basedir(create_string(opt_basedir));
  string srcdir(create_string(opt_srcdir));
  string builddir(create_string(opt_builddir));

  if (opt_verbose != 1)
  {
    info.enabled(false);
  }

  if (default_opt_used > 0)
  {
    if (opt_defaults_file != 0)
    {
      info << "Using default values from " << opt_defaults_file << endl;
    }
    else
    if (!opt_no_defaults)
    {
      info << "Using default values from my.cnf" << endl;
    }
    if (opt_def_extra_file != 0)
    {
      info << "Using additional default values from " << endl;
    }
  }

  /*
   1. Verify all option parameters
   2. Create missing directories
   3. Compose mysqld start string
   4. Execute mysqld
   5. Exit
  */

  if (opt_adminlogin)
  {
    info << "Reading the login config file "
         << opt_adminlogin
         << " for default account credentials using login-path = "
         << opt_loginpath
         << endl;
    int ret= get_admin_credentials(create_string(opt_adminlogin),
                                   create_string(opt_loginpath),
                                   &adminuser,
                                   &adminhost,
                                   &password);
    switch(ret)
    {
      case ALL_OK: expire_password= false;
        if (password.length() == 0 && !opt_insecure)
        {
          error << "Password is specified as empty! You need to use the "
                   "--insecure option" << endl;
          return 1;
        }
      break;
      case ERR_FILE:
        error << "Can't read the login config file: "
              << opt_adminlogin << endl;
        return 1;
      case ERR_ENCRYPTION:
        error << "Failed to decrypt the login config file: "
              << opt_adminlogin << endl;
        return 1;
      case ERR_NO_SUCH_CATEGORY:
        error << "Failed to locate login-path '"
              << opt_loginpath << "' "
              << "in the login config file '"
              << opt_adminlogin << "' " << endl;
        return 1;
      case ERR_SYNTAX:
      default:
        error << "Failed to parse the login config file: "
              << opt_adminlogin << endl;
        return 1;

    }
  }

  if (!assert_valid_root_account(adminuser,
                                 adminhost,
                                 create_string(opt_authplugin),
                                 opt_ssl))
  {
    /* Subroutine reported error */
    return 1;
  }


  Path data_directory;
  if (!assert_valid_datadir(create_string(opt_datadir), &data_directory))
  {
    /* Subroutine reported error */
    return 1;
  }

  Path language_directory;
  if (!assert_valid_language_directory(create_string(opt_langpath),
                                       basedir,
                                       builddir,
                                       srcdir,
                                       &language_directory))
  {
    /* Subroutine reported error */
    return 1;
  }

  Path mysqld_exec;
  if( !assert_mysqld_exists(create_string(opt_mysqldfile),
                            basedir,
                            builddir,
                            srcdir,
                            &mysqld_exec))
  {
    /* Subroutine reported error */
    return 1;
  }

#if defined(HAVE_YASSL)
  Path mysql_cert_setup;
  if( !opt_insecure &&
      !assert_cert_generator_exists(create_string(opt_mysql_cert_setup_file),
                                    basedir,
                                    builddir,
                                    srcdir,
                                    &mysql_cert_setup))
  {
    /* Subroutine reported error */
    return 1;
  }
#endif /* HAVE_YASSL */

  if (opt_def_extra_file)
  {
    Path def_extra_file;
    def_extra_file.qpath(opt_def_extra_file);
    if (!def_extra_file.exists())
    {
      warning << "Can't open extra defaults file '"
              << opt_def_extra_file
              << "' (skipping)" << endl;
      opt_def_extra_file= NULL;
    }
  }

  if (opt_defaults_file)
  {
    opt_no_defaults= FALSE;
    Path defaults_file;
    defaults_file.qpath(opt_defaults_file);
    if (!defaults_file.exists())
    {
      error << "Can't open defaults file '"
            << defaults_file
            << "'" << endl;
      opt_defaults_file= NULL;
      return 1;
    }
  }

  if (data_directory.exists())
  {
    info << "Using existing directory "
         << data_directory
         << endl;
  }
  else
  {
    info << "Creating data directory "
         << data_directory << endl;
    mode_t old_mask= umask(0);
    if (my_mkdir(data_directory.to_str().c_str(),
                 S_IRWXU | S_IRGRP | S_IXGRP, MYF(MY_WME)))
    {
      error << "Failed to create the data directory '"
            << data_directory << "'" << endl;
      umask(old_mask);
      return 1;
    }
    umask(old_mask);
  }

  /* Generate a random password is no password was found previously */
  if (password.length() == 0 && !opt_insecure)
  {
    Path randpwdfile;
    if (opt_randpwdfile != 0)
    {
      randpwdfile.qpath(opt_randpwdfile);
    }
    else
    {
      randpwdfile.get_homedir();
      randpwdfile.filename(default_randpwfile);
    }
    info << "Generating random password to "
         << randpwdfile << "...";
    generate_password(&password,12);
    if (generate_password_file(randpwdfile, adminuser, adminhost,
                               password) != ALL_OK)
    {
      error << "Can't create password file "
            << randpwdfile
            << endl;
      return 1;
    }
  }

  if (opt_euid && geteuid() == 0)
  {
    struct passwd *pwd;
    info << "Setting file ownership to " << opt_euid
         << endl;
    pwd= getpwnam(opt_euid);   /* Try getting UID for username */
    if (pwd == NULL)
    {
      error << "Failed to verify user id '" << opt_euid
            << "'. Does it exist?" << endl;
      return 1;
    }
    if (chown(data_directory.to_str().c_str(), pwd->pw_uid, pwd->pw_gid) != 0)
    {
      error << "Failed to set file ownership for "
            << data_directory.to_str()
            << " to (" << pwd->pw_uid << ", " << pwd->pw_gid << ")"
            << endl;
    }
    if (setegid(pwd->pw_gid) != 0)
    {
      warning << "Failed to set effective group id to " << pwd->pw_gid
              << endl;
    }
    if (seteuid(pwd->pw_uid) != 0)
    {
      warning << "Failed to set effective user id to " << pwd->pw_uid
              << endl;
    }
  }
  else
    opt_euid= 0;
  vector<string> command_line;
  if (opt_no_defaults == TRUE && opt_defaults_file == NULL &&
      opt_def_extra_file == NULL)
    command_line.push_back(string("--no-defaults"));
  if (opt_defaults_file != NULL)
    command_line.push_back(string("--defaults-file=")
      .append(opt_defaults_file));
  if (opt_def_extra_file != NULL)
    command_line.push_back(string("--defaults-extra-file=")
      .append(opt_def_extra_file));
  command_line.push_back(string("--bootstrap"));
  command_line.push_back(string("--datadir=")
    .append(data_directory.to_str()));
  command_line.push_back(string("--lc-messages-dir=")
    .append(language_directory.to_str()));
  command_line.push_back(string("--lc-messages=")
    .append(create_string(opt_lang)));
  if (basedir.length() > 0)
  command_line.push_back(string("--basedir=")
    .append(basedir));

#if defined(HAVE_YASSL)
  vector<string> cert_setup_command_line;
  if (!opt_insecure)
  {
    if (opt_no_defaults == TRUE && opt_defaults_file == NULL &&
        opt_def_extra_file == NULL)
      cert_setup_command_line.push_back(string("--no-defaults"));
    cert_setup_command_line.push_back(string("--datadir=")
      .append(data_directory.to_str()));
    cert_setup_command_line.push_back(string("--suffix=")
      .append(MYSQL_SERVER_VERSION));
  }
#endif /* HAVE_YASSL */

  // DEBUG
  //mysqld_exec.append("\"").insert(0, "gnome-terminal -e \"gdb --args ");

  string ssl_type;
  string ssl_cipher;
  string x509_issuer;
  string x509_subject;
  if (opt_ssl == true)
    create_ssl_policy(&ssl_type, &ssl_cipher, &x509_issuer, &x509_subject);
  info << "Executing " << mysqld_exec.to_str() << " ";
  copy(command_line.begin(), command_line.end(),
       infix_ostream_iterator<Path>(info, " "));
  info << endl;
  Sql_user user(adminhost,
                adminuser,
                password,
                Access_privilege(Access_privilege::acl_all()),
                ssl_type, // ssl_type
                ssl_cipher, // ssl_cipher
                x509_issuer, // x509_issuer
                x509_subject, // x509_subject
                0, // max_questions
                0, // max updates
                0, // max connections
                0, // max user connections
                create_string(opt_authplugin),
                string(""),
                expire_password,
                0);
  string output;
  bool success= process_execute(mysqld_exec.to_str(),
                  command_line.begin(),
                  command_line.end(),
                  Process_reader(&output),
                  Process_writer(&user,create_string(opt_sqlfile)));
  if (!success)
  {
    error << "Failed to execute " << mysqld_exec.to_str() << " ";
    copy(command_line.begin(), command_line.end(),
         infix_ostream_iterator<Path>(error, " "));
    error << endl;
    cerr << "-- server log begin --" << endl;
    cerr << output << endl;
    cerr << "-- server log end --" << endl;
    return 1;
  }
  else if (output.find("ERROR") != string::npos)
  {
    error << "The bootstrap log isn't empty:"
          << endl
          << output
          << endl;
  }
  else if (output.size() > 0 &&
           output.find_first_not_of(" \t\n\r") != string::npos)
  {
    warning << "The bootstrap log isn't empty:"
            << endl
            << output
            << endl;
  }
  else
  {
    info << "Success!"
         << endl;
  }

#if defined(HAVE_YASSL)
  if (!opt_insecure)
  {
    string ssl_output;
    info << "Generating SSL Certificates" << endl;
    success= process_execute(mysql_cert_setup.to_str(),
                             cert_setup_command_line.begin(),
                             cert_setup_command_line.end(),
                             Process_reader(&ssl_output),
                             SSL_generator_writer());
    if (!success)
    {
      warning << "failed to execute " << mysql_cert_setup.to_str() << " ";
      copy(cert_setup_command_line.begin(), cert_setup_command_line.end(),
           infix_ostream_iterator<Path>(error, " "));
      warning << endl;
      warning << "SSL functionality may not work";
      warning << endl;
    }
    else if ((ssl_output.size() > 0))
    {
      info << "SSL certificate generation :"
           << endl
           << ssl_output
           << endl;
    }
  }

#endif /* HAVE_YASSL */

  return 0;
}
