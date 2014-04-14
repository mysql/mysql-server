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

// C headers and MySQL headers
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <my_global.h>
#include <my_default.h>
#include <my_getopt.h>
#include <welcome_copyright_notice.h>
#include <mysql_version.h>
#include <my_dir.h>
#include <unistd.h>
#include <pwd.h>
#include <my_rnd.h>
#include "my_aes.h"

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
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_C '/'

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
char default_randpwfile[]= "/root/.mysql_secret";
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
    " message file and the file for populating the help tables. "
    "This option was added in MySQL 5.0.32.", &opt_srcdir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 0, "The path to the MySQL installation directory.", &opt_basedir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 0, "The path to the MySQL data directory.", &opt_datadir,
    0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0}, 
  {"login-path", 0, "Use the MySQL password store to set the default password",
    &opt_adminlogin, 0, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sql-file", 'f', "Optional SQL file to execute during bootstrap",
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
   0, 0, GET_STR_ALLOC, REQUIRED_ARG, (longlong)&default_randpwfile,
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

struct Datetime {};

ostream &operator<<(ostream &os, const Datetime &dt) 
{
  const char format[]= "%F %X";
  time_t t(time(NULL));	// current time
  tm tm(*localtime(&t));	
  std::locale loc(cout.getloc());	// current user locale
  ostringstream sout;
  const std::time_put<char> &tput =
          std::use_facet<std::time_put<char> >(loc);
  tput.put(sout.rdbuf(), sout, '\0', &tm, &format[0], &format[5]);
  os << sout.str() << " ";
  return os;
}

class Access_privilege
{
public:
  Access_privilege() : m_priv(0) {}
  Access_privilege(uint64_t privileges) : m_priv(privileges) {}
  Access_privilege(const Access_privilege &priv) : m_priv(priv.m_priv) {}
  bool has_select_ac()  { return (m_priv & (1L)) > 0; }
  bool has_insert_ac() { return (m_priv & (1L << 1)) > 0; }
  bool has_update_ac() { return (m_priv & (1L << 2)) > 0; }
  bool has_delete_ac() { return (m_priv & (1L << 3)) > 0; }
  bool has_create_ac() { return (m_priv & (1L << 4)) > 0; }
  bool has_drop_ac() { return (m_priv & (1L << 5)) > 0; }
  bool has_relead_ac() { return (m_priv & (1L << 6)) > 0; }
  bool has_shutdown_ac() { return (m_priv & (1L << 7)) > 0; }
  bool has_process_ac() { return (m_priv & (1L << 8)) > 0; }
  bool has_file_ac() { return (m_priv & (1L << 9)) > 0; }
  bool has_grant_ac() { return (m_priv & (1L << 10)) > 0; }
  bool has_references_ac() { return (m_priv & (1L << 11)) > 0; }
  bool has_index_ac() { return (m_priv & (1L << 12)) > 0; }
  bool has_alter_ac() { return (m_priv & (1L << 13)) > 0; }
  bool has_show_db_ac() { return (m_priv & (1L << 14)) > 0; }
  bool has_super_ac() { return (m_priv & (1L << 15)) > 0; }
  bool has_create_tmp_ac() { return (m_priv & (1L << 16)) > 0; }
  bool has_lock_tables_ac() { return (m_priv & (1L << 17)) > 0; }
  bool has_execute_ac() { return (m_priv & (1L << 18)) > 0; }
  bool has_repl_slave_ac() { return (m_priv & (1L << 19)) > 0; }
  bool has_repl_client_ac() { return (m_priv & (1L << 20)) > 0; }
  bool has_create_view_ac() { return (m_priv & (1L << 21)) > 0; }
  bool has_show_view_ac() { return (m_priv & (1L << 22)) > 0; }
  bool has_create_proc_ac() { return (m_priv & (1L << 23)) > 0; }
  bool has_alter_proc_ac() { return (m_priv & (1L << 24)) > 0; }
  bool has_create_user_ac() { return (m_priv & (1L << 25)) > 0; }
  bool has_event_ac() { return (m_priv & (1L << 26)) > 0; }
  bool has_trigger_ac() { return (m_priv & (1L << 27)) > 0; }
  bool has_create_tablespace_ac() { return (m_priv & (1L << 28)) > 0; }
  inline static uint64_t select_ac()  { return (1L); }
  inline uint64_t insert_ac() { return (1L << 1); }
  inline uint64_t update_ac() { return (1L << 2); }
  inline uint64_t delete_ac() { return (1L << 3); }
  inline static uint64_t create_ac() { return (1L << 4); }
  inline static uint64_t drop_ac() { return (1L << 5); }
  inline static uint64_t relead_ac() { return (1L << 6); }
  inline static uint64_t shutdown_ac() { return (1L << 7); }
  inline static uint64_t process_ac() { return (1L << 8); }
  inline static uint64_t file_ac() { return (1L << 9); }
  inline static uint64_t grant_ac() { return (1L << 10); }
  inline static uint64_t references_ac() { return (1L << 11); }
  inline static uint64_t index_ac() { return (1L << 12); }
  inline static uint64_t alter_ac() { return (1L << 13); }
  inline static uint64_t show_db_ac() { return (1L << 14); }
  inline static uint64_t super_ac() { return (1L << 15); }
  inline static uint64_t create_tmp_ac() { return (1L << 16); }
  inline static uint64_t lock_tables_ac() { return (1L << 17); }
  inline static uint64_t execute_ac() { return (1L << 18); }
  inline static uint64_t repl_slave_ac() { return (1L << 19); }
  inline static uint64_t repl_client_ac() { return (1L << 20); }
  inline static uint64_t create_view_ac() { return (1L << 21); }
  inline static uint64_t show_view_ac() { return (1L << 22); }
  inline static uint64_t create_proc_ac() { return (1L << 23); }
  inline static uint64_t alter_proc_ac() { return (1L << 24); }
  inline static uint64_t create_user_ac() { return (1L << 25); }
  inline static uint64_t event_ac() { return (1L << 26); }
  inline static uint64_t trigger_ac() { return (1L << 27); }
  inline static uint64_t create_tablespace_ac() { return (1L << 28); }
  inline static uint64_t acl_all() { return 0xfffffff; }
  uint64_t to_int() const { return m_priv; };
private:
  uint64_t m_priv;
};

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
    stringstream ss, oldpass;

    ss << "INSERT INTO mysql.user VALUES ("
       << "'" << host << "','" << user << "',";

    if (plugin == "mysql_native_password")
    {
      ss << "PASSWORD('" << password << "'),";
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
    ss << "'" << ssl_type << "',"
       << "'" << ssl_cipher << "',"
       << "'" << x509_issuer << "',"
       << "'" << x509_subject << "',"
       << max_questions << ","
       << max_updates << ","
       << max_connections << ","
       << max_user_connections << ","
       << "'" << plugin << "',";
    if (plugin == "sha256_password")
    {
      oldpass << "SET @@old_passwords= 2;\n";
      ss << "PASSWORD('" << password << "'),";
    }
    else
    {
      ss << "'" << authentication_string << "',";
    }
    if (password_expired)
      ss << "'Y',";
    else
      ss << "'N',";
    ss << "now(), NULL);\n";

    cmdstr->append(oldpass.str()).append(ss.str());
  }

};

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

/*
  A helper class for handling file paths. The class can handle the memory 
  on its own or it can wrap an external string.
*/
class Path
{
public:
  Path(string *path) : m_ptr(path), m_fptr(&m_filename)
  {
    trim();
  }

  Path(string *path, string *filename) : m_ptr(path), m_fptr(filename)
  {
    trim();
  }

  Path(void) { m_ptr= &m_path; m_fptr= &m_filename; trim(); }

  Path(const string &s) { path(s); m_fptr= &m_filename; }

  Path(const Path &p) { m_path= p.m_path; m_filename= p.m_filename; m_ptr= &m_path; m_fptr= &m_filename; }

  bool getcwd(void)
  {
    char path[512];
    if (::getcwd(path, 512) == 0)
       return false;
    m_ptr->clear();
    m_ptr->append(path);
    trim();
    return true;
  }

  void trim()
  {
    if (m_ptr->length() <= 1)
      return;
    string::iterator it= m_ptr->end();
    --it;

    if ((*it) == PATH_SEPARATOR_C)
      m_ptr->erase(it);
  }

  Path &append(const string &path)
  {
    if (m_ptr->length() > 1 && path[0] != PATH_SEPARATOR_C)
      m_ptr->append(PATH_SEPARATOR);
    m_ptr->append(path);
    trim();
    return *this;
  }

  void path(string *p)
  {
    m_ptr= p;
    trim();
  }

  void path(const string &p)
  {
    m_path.clear();
    m_path.append(p);
    m_ptr= &m_path;
    trim();
  }
  
  void filename(const string &f)
  {
    m_filename.clear();
    m_filename.append(f);
    m_fptr= &m_filename;
  }
  void filename(string *f) { m_fptr= f; }
  void path(const Path &p) { path(p.m_path); }
  void filename(const Path &p) { path(p.m_filename); }

  void qpath(const string &qp)
  {
    size_t idx= qp.rfind(PATH_SEPARATOR);
    if (idx == string::npos)
    {
      m_filename= qp;
      m_path.clear();
      m_ptr= &m_path;
      m_fptr= &m_filename;
      return;
    }
    filename(qp.substr(idx + 1, qp.size() - idx));
    path(qp.substr(0, idx));   
  }

  bool is_qualified_path()
  {
    return m_fptr->length() > 0;
  }

  bool exists() 
  {
    if (!is_qualified_path())
    {
      DIR *dir= opendir(m_ptr->c_str());
      if (dir == 0)
        return false;
      return true;
    }
    else
    {
      MY_STAT s;
      string qpath(*m_ptr);
      qpath.append(PATH_SEPARATOR).append(*m_fptr);
      if (my_stat(qpath.c_str(), &s, MYF(0)) == NULL)
        return false;
      return true;
    }
  }

  string to_str()
  {
    string qpath(*m_ptr);
    if (m_filename.length() != 0)
    {
      qpath.append(PATH_SEPARATOR);
      qpath.append(m_filename);
    }
    return qpath;
  }
  
  friend ostream &operator<<(ostream &op, const Path &p);
private:  
  string m_path;
  string *m_ptr;
  string *m_fptr;
  string m_filename;
};

ostream &operator<<(ostream &op, const Path &p)
{
  return op << *(p.m_ptr);
}

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

string create_string(char *ptr)
{
  if (ptr)
    return string(ptr);
  else
    return string("");
}

#ifndef all_of
template<class InputIterator, class UnaryPredicate>
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
  return isalnum(c);
}


/**
  Verify that the default admin account follows the recommendations and
  restrictions.
*/
bool assert_valid_root_account(const string &username, const string &host, 
                               const string &plugin, bool ssl)
{
 
  if( username.length() > 16 && username.length() < 1)
  {
    cerr << Datetime()
         << "[ERROR] Username must be between 1 and 20 characters in length."
         << endl;
    return false;
  }
  
  if (!all_of(username.begin(), username.end(), my_legal_characters))
  {
    cerr << Datetime()
         << "[ERROR] Recommended practice is to use only alpha-numericals in the"
            "user name."
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
    cerr << Datetime() << "[ERROR] The data directory '"<< datadir.c_str()
         << "' already exist." << endl;
    return false;
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
    m_qpath->path(path);
    m_qpath->filename(*m_file);
    if (m_qpath->exists())
      return true;
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

bool locate_file(const string &filename, vector<Path> *search_paths,
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
    
    if (!locate_file("mysqld", &spaths, qpath))
    {
      cerr << Datetime()
           << "[ERROR] Can't locate the server executable (mysqld)." << endl;
      cout << Datetime()
           << "The following paths were searched:";
      copy(spaths.begin(), spaths.end(),
           ostream_iterator<Path>(cout, ", "));
      cout << endl;
      return false;
    }
  }
  return true;
}

void trim(string *s)
{
  stringstream trimmer;
  trimmer << *s;
  s->clear();
  trimmer >> *s;
}

bool parse_cnf_file(stringstream &sin, map<string, string> *options)
{
  string header;
  string option_name;
  string option_value;
  getline(sin, header);
  if (header != "[client]")
  {
    cerr << "[ERROR] Missing client header" << endl;
    return false;
  }
  while (!getline(sin, option_name, '=').eof())
  {
    trim(&option_name);
    getline(sin, option_value);
    trim(&option_value);
    if (option_name.length() > 0)
      options->insert(make_pair<string, string>(option_name, option_value));
  }
  return true;
}

bool decrypt_cnf_file(ifstream &fin, stringstream &sout)
{
  fin.seekg(4, fin.beg);
  char rkey[20];
  fin.read(rkey, 20);
  while(true)
  {
    uint32_t len;
    fin.read((char*)&len,4);
    if (len == 0 || fin.eof())
      break;
    char *cipher= new char[len];
    fin.read(cipher, len);
    char plain[1024];  
    int aes_length;
    aes_length= my_aes_decrypt((const unsigned char *) cipher, len,
                               (unsigned char *) plain,
                               (const unsigned char *) rkey,
                               20, my_aes_128_ecb, NULL);                             
    plain[aes_length]= 0;
    sout << plain;
  }
  return true;
}

bool get_admin_credentials(const string &opt_adminlogin,
                           string *adminuser,
                           string *adminhost,
                           string *password)
{
  Path path;
  path.qpath(opt_adminlogin);
  if (!path.exists())
    return true; // ignore 

  ifstream fin(opt_adminlogin.c_str(), ifstream::binary);
  stringstream sout;
  if (!decrypt_cnf_file(fin, sout))
  {
    cerr << "[ERROR] can't decrypt " << opt_adminlogin << endl;
    return false;
  }

  map<string, string> options;
  parse_cnf_file(sout, &options);

  cout << Datetime() << "Reading "
       << path.to_str()
       << " for default account credentials."
       << endl;
  for( map<string, string>::iterator it= options.begin();
       it != options.end(); ++it)
  {
    if (it->first == "user")
      *adminuser= it->second;
    if (it->first == "host")
      *adminhost= it->second;
    if (it->first == "password")
      *password= it->second;
  }
  return true;
}

void generate_password(string *password, int size)
{
  stringstream ss;
  rand_struct srnd;
  string allowed_characters("qwertyuiopasdfghjklzxcvbnm,.-1234567890+*"
                            "QWERTYUIOPASDFGHJKLZXCVBNM;:_!#%&/()=?><");
  while(size>0)
  {
    int ch= ((int)(my_rnd_ssl(&srnd)*100))%allowed_characters.size();
    ss << allowed_characters[ch];
    --size;
  }
  password->assign(ss.str());
}

bool assert_valid_language_directory(const string &opt_langpath,
                                         const string &opt_basedir,
                                         const string &opt_builddir,
                                         const string &opt_srcdir,
                                         Path *language_directory)
{
  vector<Path > search_paths;
  if (opt_langpath.length() > 0)
  {
    search_paths.push_back(opt_langpath);
  }
  else 
  {
    if(opt_basedir.length() > 0)
    {
      Path ld(opt_basedir);
      ld.append("/share");
      search_paths.push_back(ld);
    }
    if (opt_builddir.length() > 0)
    {
      Path ld(opt_builddir);
      ld.append("/share");
      search_paths.push_back(ld);
    }
    if (opt_srcdir.length() > 0)
    {
      Path ld(opt_srcdir);
      ld.append("/share");
      search_paths.push_back(ld);
    }
    search_paths.push_back(Path("/usr/share/mysql"));
    search_paths.push_back(Path("/opt/mysql/share"));
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
  return true;
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
  Process_writer(Sql_user *user, const string &opt_sqlfile) : m_user(user), m_opt_sqlfile(opt_sqlfile) {}
  void operator()(int fh)
  {
    /* This is the write thread */
    cout << Datetime()
         << "Creating system tables..." << flush;
    unsigned s= sizeof(mysql_system_tables)/sizeof(*mysql_system_tables);
    for(unsigned i=0, n= 1; i< s && errno != EPIPE && n != 0; ++i)
    {
      do {
        n= write(fh, mysql_system_tables[i].c_str(),
              mysql_system_tables[i].length());
      } while(errno == EAGAIN);
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
      do {
        n= write(fh, mysql_system_data[i].c_str(),
              mysql_system_data[i].length());
      } while(errno == EAGAIN);
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
    do {
      write(fh, create_user_cmd.c_str(), create_user_cmd.length());
    } while(errno == EAGAIN);
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

template< typename Reader_func_t, typename Writer_func_t, typename Fwd_iterator >
bool process_execute(const string &exec, Fwd_iterator begin,
                     Fwd_iterator end,
                       Reader_func_t reader,
                       Writer_func_t writer)
{
  int child;
  int read_pipe[2];
  int write_pipe[2];
  
  if (pipe(read_pipe) < 0) {
    return false;
  }
  if (pipe(write_pipe) < 0) {
    ::close(read_pipe[0]);
    ::close(read_pipe[1]);
    return false;
  }

  fcntl(read_pipe[0], F_SETFL, O_NONBLOCK);
  child= vfork();
  if (child == 0)
  {
    /*
      We need to copy the strings or execve will fail with errno EFAULT 
    */
    char *execve_args[10];
    char *local_filename= strdup(exec.c_str());
    execve_args[0]= local_filename;
    int i= 1;
    for(Fwd_iterator it= begin;
        it!= end && i<10;)
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
    exit(0);
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

  streambuf *cout_sbuf = std::cout.rdbuf(); // save original sbuf
  ofstream fout;
  if (opt_verbose != 1)
  {
    cout_sbuf = std::cout.rdbuf(); // save original sbuf
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

  if (opt_adminlogin &&
      !get_admin_credentials(create_string(opt_adminlogin),
                             &adminuser,
                             &adminhost,
                             &password))
  {
    cout << Datetime() << "[Warning] ignoring login-path option: "
         << opt_adminlogin << endl;
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

  cout << Datetime() << "Creating data directory "
       << data_directory.to_str() << endl;
  umask(0);
  if (my_mkdir(data_directory.to_str().c_str(), 0770, MYF(0)) != 0)
  {
    cerr << Datetime()
         << "[ERROR] Failed to create the data directory '"
         << data_directory.to_str() << "'";
    return 1;
  }

  vector<string> command_line;
  if (opt_defaults == FALSE)
    command_line.push_back(string("--no-defaults"));
  command_line.push_back(string("--bootstrap"));
  command_line.push_back(string("--datadir=").append(data_directory.to_str()));
  command_line.push_back(string("--lc-messages-dir=").append(language_directory.to_str()));
  command_line.push_back(string("--lc-messages=").append(create_string(opt_lang)));
  if (basedir.length() > 0)
  command_line.push_back(string("--basedir=").append(basedir));
  if (opt_euid && geteuid() == 0)
    command_line.push_back(string(" --user=").append(create_string(opt_euid)));
  else if (opt_euid)
  {
    cout << Datetime() << "[Warning] Can't change effective user id to "
         << opt_euid << endl;
  }
 
  // DEBUG
  //mysqld_exec.append("\"").insert(0, "gnome-terminal -e \"gdb --args ");

  /* Generate a random password is no password was found previously */
  if (password.length() == 0 && !opt_insecure)
  {
    cout << Datetime()
         << "Generating random password to "
         << opt_randpwdfile << "..." << flush;
    generate_password(&password,12);
    /*
      The format of the password file is
      ['#'][bytes]['\n']['password bytes']['\n']|[EOF])
    */
    ofstream fout;
    fout.open(opt_randpwdfile);
    if (!fout.is_open())
    {
      cout << "failed." << endl << flush;
      cerr << Datetime()
           << "[ERROR] Can't create password file " << opt_randpwdfile << endl;
      return 1;
    }
    fout << "# The random password set for user '"
         << adminuser << "' at "
         << Datetime() << "\n"
         << password << "\n";
    fout.close();
    cout << "done." << endl << flush;
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
  process_execute(mysqld_exec.to_str(),
                  command_line.begin(),
                  command_line.end(),
                  Process_reader(&output),
                  Process_writer(&user,create_string(opt_sqlfile)));
  if (output.find("ERROR") != string::npos)
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
  cout.rdbuf(cout_sbuf);
  return 0;
}
