/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

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
extern "C"
{
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pwd.h>
}

// MySQL headers
#include "my_global.h"
#include "my_default.h"
#include "my_getopt.h"
#include "welcome_copyright_notice.h"
#include "mysql_version.h"
#include "auth_utils.h"
#include "path.h"

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

using namespace std;

#include "sql_commands_system_tables.h"
#include "sql_commands_system_data.h"

#define PROGRAM_NAME "mysql_install_db"
#define MYSQLD_EXECUTABLE "mysqld"
#define MAX_MYSQLD_ARGUMENTS 10
#define MAX_USER_NAME_LEN 16

char *opt_euid= 0;
char *opt_basedir= 0;
char *opt_datadir= 0;
char *opt_adminlogin= 0;
char *opt_sqlfile= 0;
char default_adminuser[]= "root";
char *opt_adminuser= 0;
char default_adminhost[]= "localhost";
char *opt_adminhost= 0;
char default_authplugin[]= "mysql_native_password";
char *opt_authplugin= 0;
char *opt_mysqldfile= 0;
char *opt_randpwdfile= 0;
char default_randpwfile[]= ".mysql_secret";
char *opt_langpath= 0;
char *opt_lang= 0;
char default_lang[]= "en_US";
char *opt_defaults_file= 0;
char *opt_builddir= 0;
char *opt_srcdir= 0;
my_bool opt_defaults= TRUE;
my_bool opt_insecure= FALSE;
my_bool opt_verbose= FALSE;
my_bool opt_ssl= FALSE;

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
    " message file and the file for populating the help tables."
    "This option was added in MySQL 5.0.32.", &opt_srcdir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 0, "The path to the MySQL installation directory.",
    &opt_basedir, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 0, "The path to the MySQL data directory.", &opt_datadir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"login-path", 0, "Use the MySQL password store to set the default password",
    &opt_adminlogin, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"extra-sql-file", 'f', "Optional SQL file to execute during bootstrap",
    &opt_sqlfile, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"admin-user", 0, "Username part of the default admin account",
    &opt_adminuser, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
    (longlong)&default_adminuser, 0, 0, 0, 0, 0},
  {"admin-host", 0, "Hostname part of the default admin account",
    &opt_adminhost, 0, 0, GET_STR_ALLOC,REQUIRED_ARG,
    (longlong)&default_adminhost, 0, 0, 0, 0, 0},
  {"admin-auth-plugin", 0, "Plugin to use for the default admin account",
    &opt_authplugin, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
    (longlong)&default_authplugin, 0, 0, 0, 0, 0},
  {"admin-require-ssl", 0, "Default SSL/TLS restrictions for the default admin "
    "account", &opt_ssl, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"mysqld-file", 0, "Qualified path to the mysqld binary", &opt_mysqldfile,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"random-password-file", 0, "Specifies the qualified path to the "
     ".mysql_secret temporary password file", &opt_randpwdfile,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"insecure", 0, "Disables random passwords for the default admin account",
   &opt_insecure, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"verbose", 'v', "Be more verbose when running program",
   &opt_verbose, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"lc-messages-dir", 'l', "Specifies the path to the language files",
   &opt_langpath, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lc-messages", 0, "Specifies the language to use.", &opt_lang,
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, (longlong)&default_lang, 0, 0, 0, 0, 0},
  {"defaults", 0, "Do not read any option files. If program startup fails "
    "due to reading unknown options from an option file, --no-defaults can be "
    "used to prevent them from being read.",
    &opt_defaults, 0, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"defaults-file", 0, "Use only the given option file. If the file does not "
    "exist or is otherwise inaccessible, an error occurs. file_name is "
    "interpreted relative to the current directory if given as a relative "
    "path name rather than a full path name.", &opt_defaults_file,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  /* End token */
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


/**
  A trivial placeholder for inserting Time signatures into streams.
 @example cout << Datetime() << "[Info] Today was a sunny day" << endl;
*/
struct Datetime {};

ostream &operator<<(ostream &os, const Datetime &dt)
{
  const char format[]= "%F %X";
  time_t t(time(NULL));
  tm tm(*localtime(&t));
  std::locale loc(cout.getloc());
  ostringstream sout;
  const std::time_put<char> &tput =
          std::use_facet<std::time_put<char> >(loc);
  tput.put(sout.rdbuf(), sout, '\0', &tm, &format[0], &format[5]);
  os << sout.str() << " ";
  return os;
}

void escape_string(string *str)
{
  string esc("'\"\\");
  for(string::iterator it= esc.begin(); it != esc.end(); ++it)
  {
    string::size_type idx;
    while ((idx= str->find(*it, idx)) != string::npos)
    {
      str->insert(idx, 1, '\\');
      idx +=2;
    }
  }
}

/**
  A trivial container for attributes associated with the creation of a MySQL
  user.
*/
struct Sql_user
{
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
    stringstream set_oldpasscmd,ss;
    string esc_password(password);
    string esc_host(host);
    string esc_user(user);
    string esc_plugin(plugin);
    string esc_auth_str(authentication_string);
    string esc_ssl_type(ssl_type);
    string esc_ssl_cipher(ssl_cipher);
    string esc_x509_issuer(x509_issuer);
    string esc_x509_subject(x509_subject);
    escape_string(&esc_password);
    escape_string(&esc_user);
    escape_string(&esc_host);
    escape_string(&esc_plugin);
    escape_string(&esc_auth_str);
    escape_string(&esc_ssl_type);
    escape_string(&esc_ssl_cipher);
    escape_string(&esc_x509_issuer);
    escape_string(&esc_x509_subject);

    ss << "INSERT INTO mysql.user VALUES ("
       << "'" << esc_host << "','" << esc_user << "',";

    if (plugin == "mysql_native_password")
    {
      ss << "PASSWORD('" << esc_password << "'),";
    }
    else if (plugin == "sha256_password")
    {
      ss << "'',";
    }
    uint64_t acl= priv.to_int();
    for(int i= 0; i< 29; ++i)
    {
      if( (acl & (1L << i)) != 0 )
        ss << "'Y',";
      else
        ss << "'N',";
    }
    ss << "'" << esc_ssl_type << "',"
       << "'" << esc_ssl_cipher << "',"
       << "'" << esc_x509_issuer << "',"
       << "'" << esc_x509_subject << "',"
       << max_questions << ","
       << max_updates << ","
       << max_connections << ","
       << max_user_connections << ","
       << "'" << plugin << "',";
    if (plugin == "sha256_password")
    {
      set_oldpasscmd << "SET @@old_passwords= 2;\n";
      ss << "PASSWORD('" << esc_password << "'),";
    }
    else
    {
      ss << "'" << esc_auth_str << "',";
    }
    if (password_expired)
      ss << "'Y',";
    else
      ss << "'N',";
    ss << "now(), NULL);\n";

    cmdstr->append(set_oldpasscmd.str()).append(ss.str());
  }

};

/**
  Helper function for gathering parameters under the Sql_user umbrella.
*/
void create_user(Sql_user *u,
                 const string &host,
                 const string &user,
                 const string &pass,
                 const Access_privilege &priv,
                 const string &ssl_type,
                 const string &ssl_cipher,
                 const string &x509_issuer,
                 const string &x509_subject,
                 int max_questions,
                 int max_updates,
                 int max_connections,
                 int max_user_connections,
                 const string &plugin,
                 const string &authentication_string,
                 bool password_expired,
                 int password_lifetime)
{
  u->host= host;
  u->user= user;
  u->password= pass;
  u->priv= priv;
  u->ssl_type= ssl_type;
  u->ssl_cipher= ssl_cipher;
  u->x509_issuer= x509_issuer;
  u->x509_subject= x509_subject;
  u->max_questions= max_questions;
  u->max_updates= max_updates;
  u->max_connections= max_connections;
  u->max_user_connections= max_user_connections;
  u->plugin= plugin;
  u->authentication_string= authentication_string;
  u->password_expired= password_expired;
  u->password_lifetime= password_lifetime;
}

static const char *load_default_groups[]= { "mysql_install_db", 0 };

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
  cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2014") << endl
       << "MySQL Database Deployment Utility." << endl
       << "Usage: "
       << p
       << "[OPTIONS]\n";
  my_print_help(my_connection_options);
  my_print_variables(my_connection_options);
}

my_bool
my_arguments_get_one_option(int optid,
                            const struct my_option *opt __attribute__((unused)),
                            char *argument)
{
  switch(optid)
  {
    case '?':
      usage(PROGRAM_NAME);
      exit(0);
  }
  return 0;
}


/**
  The string class will berak if constructed with a NULL pointer. This wrapper
 provides a systematic protection when importing char pointers.
*/
string create_string(char *ptr)
{
  if (ptr)
    return string(ptr);
  else
    return string("");
}

#ifndef all_of
template <class InputIterator, class UnaryPredicate >
  bool all_of (InputIterator first, InputIterator last, UnaryPredicate pred)
{
  while (first!=last) {
    if (!pred(*first)) return false;
    ++first;
  }
  return true;
}
#endif

bool my_legal_characters(const char &c)
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
  if( username.length() > MAX_USER_NAME_LEN && username.length() < 1)
  {
    cerr << Datetime()
         << "[ERROR] Username must be between 1 and "
         << MAX_USER_NAME_LEN
         << " characters in length."
         << endl;
    return false;
  }
  if (!all_of(username.begin(), username.end(), my_legal_characters) ||
      !all_of(host.begin(), host.end(), my_legal_characters))   
  {
    cerr << Datetime()
         << "[ERROR] Recommended practice is to use only alpha-numericals in "
            "the user / host name."
         << endl;
    return false;
  }
  if (!all_of(plugin.begin(), plugin.end(), my_legal_characters))
  {
    cerr << Datetime()
         << "[ERROR] Only use alpha-numericals in the the plugin name."
         << endl;
    return false;
  }
  if (plugin != "mysql_native_password" && plugin != "sha256_password")
  {
    cerr << Datetime()
         << "[ERROR] Unsupported authentication plugin specified."
         << endl;
    return false;
  }
  return true;
}

bool assert_valid_datadir(const string &datadir, Path *target)
{
  if (datadir.length() == 0)
  {
    cerr << Datetime()
         << "[ERROR] The data directory needs to be specified."
         << endl;
    return false;
  }

  target->append(datadir);

  if (target->exists())
  {
    if (!target->empty())
    {
      cerr << Datetime() << "[ERROR] The data directory '"<< datadir.c_str()
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

void add_standard_search_paths(vector<Path > *spaths, const string &path)
{
  Path p;
  if (!p.getcwd())
    cout << Datetime()
         << "[Warning] Can't determine current working directory." << endl;

  spaths->push_back(p);
  spaths->push_back(Path(p).append("/").append(path));
  spaths->push_back(Path("/usr/").append(path));
  spaths->push_back(Path("/usr/local/").append(path));
  spaths->push_back(Path("/opt/mysql/").append(path));
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
      cerr << Datetime()
           << "[ERROR] No such file: " << opt_mysqldfile << endl;
      return false;
    }
  }
  else
  {
    if (opt_basedir.length() > 0)
    {
      spaths.push_back(Path(opt_basedir).append("bin"));
    }
    if (opt_builddir.length() > 0)
    {
      spaths.push_back(Path(opt_builddir).append("sql"));
    }

    add_standard_search_paths(&spaths,"bin");

    if (!locate_file(MYSQLD_EXECUTABLE, &spaths, qpath))
    {
      cerr << Datetime()
           << "[ERROR] Can't locate the server executable (mysqld)." << endl;
      cout << Datetime()
           << "The following paths were searched:";
      copy(spaths.begin(), spaths.end(),
           ostream_iterator<Path >(cout, ", "));
      cout << endl;
      return false;
    }
  }
  return true;
}

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
    found_subdir= true;
  }

  if (!locate_file("", &search_paths, language_directory))
  {
    cerr << Datetime() << "[ERROR] Can't locate the language directory." << endl;
    cout << Datetime() << "Attempted the following paths: ";
    copy(search_paths.begin(), search_paths.end(),
         ostream_iterator<Path>(cout, ", "));
    cout << endl;
    return false;
  }
  if (found_subdir)
    language_directory->up();
  return true;
}

/**
 Parse the login.cnf file and extract the admin credentials
 @return Error
   @retval 0
   @retval ERR_FILE File not found
   @retval ERR_ENCRYPTION Error while decrypting 
   @retval ERR_SYNTAX Error while parsing
*/
int get_admin_credentials(const string &opt_adminlogin,
                             string *adminuser,
                             string *adminhost,
                             string *password)
{
  Path path;
  path.qpath(opt_adminlogin);
  if (!path.exists())
    return ERR_FILE;

  ifstream fin(opt_adminlogin.c_str(), ifstream::binary);
  stringstream sout;
  if (decrypt_login_cnf_file(fin, sout) != ALL_OK)
    return ERR_ENCRYPTION;

  map<string, string > options;
  if (parse_cnf_file(sout, &options, "client") != ALL_OK)
     return ERR_SYNTAX;

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
  return 0;
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

class Process_reader
{
public:
  Process_reader(string *buffer) : m_buffer(buffer) {}
  void operator()(int fh)
  {
    stringstream ss;
    int ch= 0;
    size_t n= 1;
    while(errno == 0 && n != 0)
    {
      n= read(fh,&ch,1);
      if (n > 0)
        ss << (char)ch;
    }
    m_buffer->append(ss.str());
  }

private:
  string *m_buffer;

};

class Process_writer
{
public:
  Process_writer(Sql_user *user, const string &opt_sqlfile) : m_user(user),
    m_opt_sqlfile(opt_sqlfile) {}
  void operator()(int fh)
  {
    cout << Datetime()
         << "Creating system tables..." << flush;
    unsigned s= 0;
    s= sizeof(mysql_system_tables)/sizeof(*mysql_system_tables);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0; ++i)
    {
      n= write(fh, mysql_system_tables[i].c_str(),
            mysql_system_tables[i].length());
    }   
    if (errno != 0)
    {
      cout << "failed." << endl;
      cerr << Datetime()
            << "[ERROR] Errno= " << errno << endl;
      return;
    }
    else
      cout << "done." << endl;
    cout << Datetime()
         << "Filling system tables with data..." << flush;
    s= sizeof(mysql_system_data)/sizeof(*mysql_system_data);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0; ++i)
    {
      n= write(fh, mysql_system_data[i].c_str(),
            mysql_system_data[i].length());
    }
    if (errno != 0)
    {
      cout << "failed." << endl;
      cerr << Datetime()
            << "[ERROR] Errno= " << errno << endl;
      return;
    }
    else
    {
      cout << "done." << endl;
    }
    cout << Datetime()
         << "Creating default user " << m_user->user << "@"
         << m_user->host
         << endl;
    string create_user_cmd;
    m_user->to_sql(&create_user_cmd);
    write(fh, create_user_cmd.c_str(), create_user_cmd.length());
    if (errno != 0)
    {
      cerr << Datetime()
           << "[ERROR] Failed to create user. Errno = "
           << errno << endl << flush;
      return;
    }

    /* Execute optional SQL from a file */
    if (m_opt_sqlfile.length() > 0)
    {
      Path extra_sql;
      extra_sql.qpath(m_opt_sqlfile);
      if (!extra_sql.exists())
      {
        cerr << Datetime()
             << "[ERROR] No such file " << extra_sql.to_str() << endl;
        return;
      }
      cout << Datetime()
       << "Executing extra SQL commands from " << extra_sql.to_str()
       << endl;
      ifstream fin(extra_sql.to_str().c_str());
      string sql_command;
      for (int n= 0; !getline(fin, sql_command).eof() && errno != EPIPE &&
           n != 0; )
      {
        n= write(fh, sql_command.c_str(), sql_command.length());
      }
      fin.close();
    }
  }
private:
  Sql_user *m_user;
  string m_opt_sqlfile;
};

template <typename Reader_func_t, typename Writer_func_t,
          typename Fwd_iterator >
bool process_execute(const string &exec, Fwd_iterator begin,
                       Fwd_iterator end, Reader_func_t reader,
                       Writer_func_t writer)
{
  int child;
  int read_pipe[2];
  int write_pipe[2];
  /*
    Disable any signal handler for broken pipes and check for EPIPE during
    IO instead.
  */
  signal(SIGPIPE, SIG_IGN);
  if (pipe(read_pipe) < 0) {
    return false;
  }
  if (pipe(write_pipe) < 0) {
    ::close(read_pipe[0]);
    ::close(read_pipe[1]);
    return false;
  }

  child= vfork();
  if (child == 0)
  {
    /*
      We need to copy the strings or execve will fail with errno EFAULT
    */
    char *execve_args[MAX_MYSQLD_ARGUMENTS];
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

    /* Child */
    if (dup2(read_pipe[0], STDIN_FILENO) == -1 ||
        dup2(write_pipe[1], STDOUT_FILENO) == -1 ||
        dup2(write_pipe[1], STDERR_FILENO) == -1)
    {
      return false;
    }
    ::close(read_pipe[0]);
    ::close(read_pipe[1]);
    ::close(write_pipe[0]);
    ::close(write_pipe[1]);
    execve(local_filename, execve_args, 0);
    /* always failure if we get here! */
    cerr << Datetime() << "Child process exited with errno= " << errno << endl;
    exit(1);
  }
  else if (child > 0)
  {
    /* Parent thread */
    ::close(read_pipe[0]);
    ::close(write_pipe[1]);
    writer(read_pipe[1]);
    if (errno == EPIPE)
    {
      cerr << Datetime() << "[ERROR] The child process terminated prematurely."
           << endl;
      ::close(read_pipe[1]);
      ::close(write_pipe[0]);
      return false;
    }
    ::close(read_pipe[1]);
    reader(write_pipe[0]);
    ::close(write_pipe[0]);
  }
  else
  {
    /* Failure */
    ::close(read_pipe[0]);
    ::close(read_pipe[1]);
    ::close(write_pipe[0]);
    ::close(write_pipe[1]);
  }
  return true;
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
  fout.open(pwdfile.to_str().c_str());
  if (!fout.is_open())
    return ERR_FILE;

  fout << "# Password set for user '"
       << adminuser << "@" << adminhost << "' at "
       << Datetime() << "\n"
       << password << "\n";
  fout.close();
  cout << "done." << endl << flush;
  return ALL_OK;
}

int main(int argc,char *argv[])
{
  /*
    In order to use the mysys library and the program option library
    we need to call the MY_INIT() macro.
  */
  MY_INIT(argv[0]);

#ifdef __WIN__
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, &argc, &argv);
#endif

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my", load_default_groups, &argc, &argv))
    return 1;

  int rc= 0;
  if ((rc= handle_options(&argc, &argv, my_connection_options,
                             my_arguments_get_one_option)))
  {
    cerr << Datetime() << "[ERROR] Unrecognized options" << endl;
    return 1;
  }

  string adminuser(create_string(opt_adminuser));
  string adminhost(create_string(opt_adminhost));
  string authplugin(create_string(opt_authplugin));
  string password;
  string basedir(create_string(opt_basedir));
  string srcdir(create_string(opt_srcdir));
  string builddir(create_string(opt_builddir));

  ofstream fout;
  if (opt_verbose != 1)
  {
    fout.open("/dev/null"); // TODO work on windows
    cout.rdbuf(fout.rdbuf()); // redirect 'cout' to a 'fout'
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
    cout << Datetime() << "Reading the login config file "
     << opt_adminlogin
     << " for default account credentials."
     << endl;
    int ret= get_admin_credentials(create_string(opt_adminlogin),
                                   &adminuser,
                                   &adminhost,
                                   &password);
    switch(ret)
    {
      case ERR_FILE:
        cerr << Datetime() << "[Error] Can't read the login config file: "
             << opt_adminlogin << endl;
        return 1;
      case ERR_ENCRYPTION:
        cerr << Datetime()
             << "[Error] Failed to decrypt the login config file: "
             << opt_adminlogin << endl;
        return 1;
      case ERR_SYNTAX:
      default:
        cerr << Datetime() << "[Error] Failed to parse the login config file: "
             << opt_adminlogin << endl;
        return 1;
    }
  }

  if (!assert_valid_root_account(create_string(opt_adminuser),
                                 create_string(opt_adminhost),
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

  if (data_directory.exists())
  {
    cout << Datetime() << "Using existing directory "
         << data_directory
         << endl;
  }
  else
  {
    cout << Datetime() << "Creating data directory "
         << data_directory << endl;
    umask(0);
    if (my_mkdir(data_directory.to_str().c_str(), 0770, MYF(0)) != 0)
    {
      cerr << Datetime()
           << "[ERROR] Failed to create the data directory '"
           << data_directory << "'" << endl;
      return 1;
    }
  }

  if (opt_euid && geteuid() == 0)
  {
    struct passwd *pwd;
    cout << Datetime() << "Setting file ownership to " << opt_euid
         << endl;
    pwd= getpwnam(opt_euid);   /* Try getting UID for username */
    if (pwd == NULL)
    {
      cerr << Datetime() << "Failed to verify user id '" << opt_euid
           << "'. Does it exist?" << endl;
      return 1;
    }
    if (chown(data_directory.to_str().c_str(), pwd->pw_uid, pwd->pw_gid) != 0)
    {
      cerr << Datetime() << "Failed to set file ownership for "
           << data_directory.to_str()
           << " to (" << pwd->pw_uid << ", " << pwd->pw_gid << ")"
           << endl;
    }
  }
  else
    opt_euid= 0;

  vector<string> command_line;
  if (opt_defaults == FALSE)
    command_line.push_back(string("--no-defaults"));
  command_line.push_back(string("--bootstrap"));
  command_line.push_back(string("--datadir=").append(data_directory.to_str()));
  command_line.push_back(string("--lc-messages-dir=").append(language_directory.to_str()));
  command_line.push_back(string("--lc-messages=").append(create_string(opt_lang)));
  if (basedir.length() > 0)
  command_line.push_back(string("--basedir=").append(basedir));
  if (opt_euid)
    command_line.push_back(string(" --user=").append(create_string(opt_euid)));

  // DEBUG
  //mysqld_exec.append("\"").insert(0, "gnome-terminal -e \"gdb --args ");

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
    cout << Datetime()
         << "Generating random password to "
         << randpwdfile << "..." << flush;
    generate_password(&password,12);
    if (generate_password_file(randpwdfile, adminuser, adminhost,
                               password) != ALL_OK)
    {
      cerr << Datetime()
           << "[ERROR] Can't create password file "
           << randpwdfile
           << endl;
    }
  }

  string ssl_type;
  string ssl_cipher;
  string x509_issuer;
  string x509_subject;
  if (opt_ssl == true)
    create_ssl_policy(&ssl_type, &ssl_cipher, &x509_issuer, &x509_subject);
  cout << Datetime()
       << "Executing " << mysqld_exec.to_str() << " ";
  copy(command_line.begin(), command_line.end(),
       ostream_iterator<Path>(cout, " "));
  cout << endl << flush;
  Sql_user user;
  create_user(&user,
              adminhost,
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
              true,
              0);
  string output;
  bool success= process_execute(mysqld_exec.to_str(),
                  command_line.begin(),
                  command_line.end(),
                  Process_reader(&output),
                  Process_writer(&user,create_string(opt_sqlfile)));
  if (!success)
  {
    cerr << Datetime()
         << "[ERROR] Failed to execute " << mysqld_exec.to_str() << " ";
    copy(command_line.begin(), command_line.end(),
         ostream_iterator<Path>(cerr, " "));
    cerr << endl;
    return 1;
  }
  else if (output.find("ERROR") != string::npos)
  {
    cerr << Datetime()
         << "[ERROR] The bootstrap log isn't empty:"
         << endl
         << output
         << endl;
  }
  else if (output.size() > 0)
  {
    cout << Datetime()
         << "[Warning] The bootstrap log isn't empty:"
         << endl
         << output
         << endl;
  }
  else
  {
    cout << Datetime()
         << "Success!"
         << endl;
  }
  return 0;
}
