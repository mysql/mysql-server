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
char default_langpath[]= "/usr/share/mysql";
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
  {"euid", 'u', "The effective user id used when executing the bootstrap "
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
   &opt_langpath, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
   (longlong)&default_langpath, 0, 0, 0, 0, 0},
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
  os << sout.str() << " >> ";
  return os;
}

class Access_privilege
{
public:
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

void create_user(string *cmdstr,
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
  stringstream ss, oldpass;
  
  ss << "INSERT INTO mysql.user VALUES ("
     << "'" << host << "','" << user << "',";
  
  if (plugin == "mysql_native_password")
  {
    ss << "PASSWORD('" << pass << "'),";
  }
  else if (plugin == "sha256_password")
  {
    ss << "'',";
  }
  for( int i= 0; i< 29; ++i)
  {
    if( (priv.to_int() & (1L << i)) != 0 )
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
    ss << "PASSWORD('" << pass << "'),";
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
    if (is_qualified_path())
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
         << "Error: Username must be between 1 and 20 characters in length."
         << endl;
    return false;
  }
  
  if (!all_of(username.begin(), username.end(), my_legal_characters))
  {
    cerr << Datetime()
         << "Error: Recommended practice is to use only alpha-numericals in the"
            "user name."
         << endl;
    return false;
  }
  
  if (plugin != "mysql_native_password" && plugin != "sha256_password")
  {
    cerr << Datetime()
         << "Error: Unsupported authentication plugin specified."
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
         << "Error: The data directory needs to be specified."
         << endl;
    return false;
  }

  target->append(datadir);

  if (target->exists())
  {
    cerr << Datetime() << "Error: The data directory '"<< datadir.c_str()
         << "' already exist." << endl;
    return false;
  }
  return true;
}

bool assert_valid_language_dir(const string &opt_langpath, string *language_directory)
{
  if (opt_langpath.length() == 0)
  {
    cerr << Datetime() << "Error: The path to the language files needs to be "
                          "specified" << endl;
    return false;
  }
  DIR *dir= opendir(opt_langpath.c_str());
  if (dir == NULL)
  {
    cerr << Datetime() << "Error: "
         << opt_langpath.c_str()
         << " is not a directory."
         << endl;
    return false;
  }
  
  *language_directory= opt_langpath;
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
    if (!(s.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
    {
      cerr << Datetime()
           << "Error: Specified server binary is not executable."
           << endl;
      return false;
    }
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

bool assert_mysqld_exists(Path *qpath, vector<Path > *spaths)
{
  if (opt_mysqldfile != 0)
  {
    /* Use explicit option to file mysqld */
    if (!locate_file(opt_mysqldfile, 0, qpath))
    {
      cerr << Datetime()
           << "Error: No such file: " << opt_mysqldfile << endl;
      return false;
    }
  }
  else
  {
    /* Use implicit locations to find mysqld */
    if (opt_basedir != 0)
    {
      spaths->push_back(Path(create_string(opt_basedir)).append("bin"));
    }
    if (opt_basedir == 0 && opt_srcdir !=0 )
    {
      spaths->push_back(Path(create_string(opt_srcdir)).append("bin"));  
    }
    
    Path path;
    if (!path.getcwd())
      cout << Datetime()
           << "Warning: Can't determine current working directory." << endl;

    spaths->push_back(path);
    spaths->push_back(Path(path).append("/bin"));
    spaths->push_back(Path(path).append("/sbin"));
    
    //copy(search_paths.begin(), search_paths.end(),
    //     ostream_iterator<Path>(cout, "\n"));

    if (!locate_file("mysqld", spaths, qpath))
    {
      cerr << Datetime()
           << "Error: Can't locate the server executable (mysqld)." << endl;
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
    cerr << "Error: Missing client header" << endl;
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
    cerr << "Error: can't decrypt " << opt_adminlogin << endl;
    return false;
  }

  map<string, string> options;
  parse_cnf_file(sout, &options);

  cout << Datetime() << "Reading "
       << path.to_str()
       << " for default account credentials."
       << endl;
  for( map<string, string>::iterator it= options.begin(); it != options.end(); ++it)
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
    int ch= (int)(my_rnd_ssl(&srnd)*allowed_characters.size());
    ss << allowed_characters[ch];
    --size;
  }
  password->assign(ss.str());
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
    exit(1);

  int rc= 0;
  if ((rc= handle_options(&argc, &argv, my_connection_options,
                             my_arguments_get_one_option)))
  {
    cerr << Datetime() << "Error: Unrecognized options" << endl;
    exit(1);
  }

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

  /* 1.1 Verify admin account parameters */
  string adminuser(create_string(opt_adminuser));
  string adminhost(create_string(opt_adminhost));
  string authplugin(create_string(opt_authplugin));
  string password;

  if (opt_adminlogin &&
      !get_admin_credentials(create_string(opt_adminlogin),
                             &adminuser,
                             &adminhost,
                             &password))
  {
    cout << Datetime() << "Warning: ignoring login-path option: "
         << opt_adminlogin << endl;
  }

  if (!assert_valid_root_account(create_string(opt_adminuser),
                                 create_string(opt_adminhost), 
                                 create_string(opt_authplugin),
                                 opt_ssl))
    exit(1);

  /* 1.2 Verify datadir. Policy: If datadir exist: Don't proceed! */
  Path data_directory;
  if (!assert_valid_datadir(create_string(opt_datadir), &data_directory))
    exit(1);

  /*
    1.3 Verify language directory.
  */
  Path language_directory;
  if (opt_langpath)
  {
    language_directory.append(create_string(opt_langpath));
  }
  /*
    basedir, builddir and srcdir are mutually exclusive but if set they
    will override any opt_langpath default or set value.
  */
  if (opt_basedir)
  {
    language_directory.path(create_string(opt_basedir));
    language_directory.append("/share");
  }
  else if (opt_builddir)
  {
    language_directory.path(create_string(opt_builddir));
    language_directory.append("/share");
  }
  else if (opt_srcdir)
  {
    language_directory.path(create_string(opt_srcdir));
    language_directory.append("/sql/share");
  }

  if (!language_directory.exists())
  {
    cerr << Datetime()
         << "Error: No such directory " << language_directory << endl;
    exit(1);
  }

  /* 1.4 Verify accessibility to mysqld */
  Path mysqld_exec;
  vector<Path > search_paths;
  if( !assert_mysqld_exists(&mysqld_exec, &search_paths))
    exit(1);

  /* 2.1 Create the datadir. */
  cout << Datetime() << "Creating data directory " << opt_datadir << endl;
  umask(0);
  if (my_mkdir(opt_datadir, 0770, MYF(0)) != 0)
  {
    cerr << Datetime()
         << "Error: Failed to create the data directory '"
         << opt_datadir << "'";
    exit(1);
  }

  /* 3. Compose mysqld bootstrap string */
  string command_line;
  command_line.append(mysqld_exec.to_str());

  if (opt_defaults == FALSE)
    command_line.append(" --no-defaults");
  command_line.append(" --bootstrap --datadir=").append(data_directory.to_str());
  command_line.append(" --lc-messages-dir=").append(language_directory.to_str());
  command_line.append(" --lc-messages=").append(create_string(opt_lang));
  if (opt_euid && geteuid() == 0)
    command_line.append(" --user=").append(create_string(opt_euid));
  else if (opt_euid)
  {
    cout << Datetime() << "Warning: Can't change euid to " << opt_euid << endl;
  }

  // DEBUG
  //command_line.append("\"").insert(0, "gnome-terminal -e \"gdb --args ");

  
#include "sql_commands_system_tables.h"
#include "sql_commands_system_data.h"

  cout << Datetime()
       << "Executing " << command_line << endl;
  FILE *p;
  if ((p = popen(command_line.c_str(), "w")) == 0)
  {
    cerr << Datetime() << "Error: Can't execute "<< command_line << endl;
  }
  cout << Datetime()
       << "Creating system tables...";  
  unsigned s= sizeof(mysql_system_tables)/sizeof(*mysql_system_tables);
  for(unsigned i=0; i< s && !feof(p); ++i)
  {
    fwrite(mysql_system_tables[i].c_str(),
          mysql_system_tables[i].length(), 1, p);
  }
  cout << "done." << endl;
  cout << Datetime()
       << "Filling system tables with data...";
  s= sizeof(mysql_system_data)/sizeof(*mysql_system_data);
  for(unsigned i=0; i< s && !feof(p); ++i)
  {
    fwrite(mysql_system_data[i].c_str(),
          mysql_system_data[i].length(), 1, p);
  }
  cout << "done." << endl;
  // create default user
  cout << Datetime()
       << "Creating default user " << adminuser << "@" << adminhost
       << endl;
  string create_user_cmd;
  if (password.length() == 0 && !opt_insecure)
  {
    cout << Datetime()
         << "Generating random password to " << opt_randpwdfile << "...";
    generate_password(&password,12);
    /*
      The format of the password file is
      ['#'][bytes]['\n']['password bytes']['\n']|[EOF])
    */
    ofstream fout;
    fout.open(opt_randpwdfile);
    if (!fout.is_open())
    {
      cout << "failed." << endl;
      cerr << Datetime()
           << "Error: Files to open " << opt_randpwdfile << endl;
      exit(1);
    }
    fout << "# The random password set for user '"
         << adminuser << "' at "
         << Datetime() << "\n"
         << password << "\n";
    fout.close();
    cout << "done." << endl;
  }
  create_user(&create_user_cmd,
              adminhost,
              adminuser,
              password,
              Access_privilege(Access_privilege::acl_all()),
              string(""), // ssl_type
              string(""), // ssl_cipher
              string(""), // x509_issuer
              string(""), // x509_subject
              0, // max_questions
              0, // max updates
              0, // max connections
              0, // max user connections
              create_string(opt_authplugin),
              string(""),
              true,
              0);

  if (!feof(p))
    fwrite(create_user_cmd.c_str(), create_user_cmd.length(), 1, p);
  
  /* Execute optional SQL from a file */
  if (opt_sqlfile != 0)
  {
    Path extra_sql;
    extra_sql.qpath(create_string(opt_sqlfile));
    if (!extra_sql.exists())
    {
      cerr << Datetime()
           << "Error: No such file " << extra_sql.to_str() << endl;
      exit(1);
    }
    cout << Datetime()
     << "Executes extra SQL commands from " << extra_sql.to_str()
     << endl;
    ifstream fin(extra_sql.to_str().c_str());
    string sql_command;
    while (!getline(fin, sql_command).eof() && !feof(p))
    {
      fwrite(sql_command.c_str(), sql_command.length(), 1, p);
    }
  }
  fflush(p); 
  /* The return value of the child process is in the top 16 bits. */
  int return_code= pclose(p)/256;
  if (return_code != 0)
  {
    cerr << Datetime()
         << "Error: mysqld returned an error code: " << return_code << endl;
  }
  else
  {
    cout << Datetime()
         << "Exiting with return code " << return_code << endl;
  }
  cout.rdbuf(cout_sbuf);
  return return_code;
}
