
#define DONT_DEFINE_VOID
#include <my_global.h>
#include <my_getopt.h>
#include <my_sys.h>
#include <m_string.h>

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include <accctrl.h>
#include <aclapi.h>


extern "C" const char mysql_bootstrap_sql[];

char default_os_user[] = "NT AUTHORITY\\Network Service";
static int create_db_instance();
static uint opt_verbose, opt_silent;
static char datadir_buffer[FN_REFLEN];
static char mysqld_path[FN_REFLEN];
static char *opt_datadir;
static char *opt_service;
static char *opt_password;
static int  opt_port;
static char *opt_socket;
static char *opt_os_user;
static char *opt_os_password;
static my_bool opt_default_user;
static my_bool opt_allow_remote_root_access;
static my_bool opt_skip_networking;


static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd', "Data directory of the new database",
  &opt_datadir, &opt_datadir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"service", 's', "Name of the Windows service",
  &opt_service, &opt_service, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p', "Root password",
  &opt_password, &opt_password, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "mysql port",
  &opt_port, &opt_port, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', 
  "named pipe name (if missing, it will be set the same as service)",
  &opt_socket, &opt_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-user", 'D', "Create default user",
  &opt_default_user, &opt_default_user, 0 , GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"allow-remote-root-access", 'R', 
  "Allows remote access from network for user root",
  &opt_allow_remote_root_access, &opt_allow_remote_root_access, 0 , GET_BOOL, 
  OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-networking", 'N', "Do not use TCP connections, use pipe instead",
  &opt_skip_networking, &opt_skip_networking, 0 , GET_BOOL, OPT_ARG, 0, 0, 0, 0,
  0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static my_bool
get_one_option(int optid, 
   const struct my_option *opt __attribute__ ((unused)),
   char *argument __attribute__ ((unused)))
{
  DBUG_ENTER("get_one_option");
  switch (optid) {
  case '?':
    my_print_help(my_long_options);
    exit(0);
    break;
  }
  DBUG_RETURN(0);
}


static void die(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");

  /* Print the error message */
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "FATAL ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  my_end(0);
  exit(1);
}


static void verbose(const char *fmt, ...)
{
  va_list args;

  if (opt_silent)
    return;

  /* Print the verbose message */
  va_start(args, fmt);
  if (fmt)
  {
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);
  }
  va_end(args);
}


int main(int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);
  char self_name[FN_REFLEN];
  char *p;

  GetModuleFileName(NULL, self_name, FN_REFLEN);
  strcpy(mysqld_path,self_name);
  p = strrchr(mysqld_path, FN_LIBCHAR);
  if(p)
  {
    strcpy(p, "\\mysqld.exe");
  }

  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(error);
  if(!opt_datadir)
  {
    my_print_help(my_long_options);
    die("parameter datadir is mandatory");
  }
  if(!opt_os_user)
  {
    opt_os_user= default_os_user;
    opt_os_password = NULL;
  }
  /* Workaround WiX bug (strip possible quote character at the end of path) */
  size_t len= strlen(opt_datadir);
  if(len > 0)
  {
    if(opt_datadir[len-1] == '"')
    {
      opt_datadir[len-1]= 0;
    }
  }
  GetFullPathName(opt_datadir, FN_REFLEN, datadir_buffer, NULL);
  opt_datadir = datadir_buffer;

  if (create_db_instance())
  {
    die("database creation failed");
  }

  printf("Creation of the database was successfull");
  return 0;
}



/**
  Convert slashes in paths into MySQL-compatible form 
*/
static void convert_slashes(char *s)
{
  for(size_t i=0; i< strlen(s); i++)
    if(s[i] == '\\')
      s[i] = '/';
}



/**
  Calculate basedir from mysqld.exe path
*/
static void get_basedir(char *basedir, int size, const char *mysqld_path)
{
  strcpy_s(basedir, size,  mysqld_path);
  convert_slashes(basedir);
  char *p = strrchr(basedir,'/');
  if(p)
  {
    *p = 0;
    p=strrchr(basedir, '/');
    if(p)
      *p=0;
  }
}



/**
  Allocate and initialize command line for mysqld --bootstrap.
 The resulting string is passed to popen, so it has a lot of quoting
 quoting around the full string plus quoting around parameters with spaces.
*/
static char *init_bootstrap_command_line(char *cmdline, size_t size)
{
  char basedir[MAX_PATH];
  get_basedir(basedir, sizeof(basedir), mysqld_path);

  my_snprintf(cmdline, size-1, 
    "\"\"%s\" --no-defaults --bootstrap"
    " \"--language=%s\\share\\english\""
    " --basedir=. --datadir=. --default-storage-engine=myisam"
    " --max_allowed_packet=9M --loose-skip-innodb --loose-skip-pbxt"
    " --net-buffer-length=16k\"", mysqld_path, basedir);
  return cmdline;
}


/**
  Create my.ini in  current directory (this is assumed to be
  data directory as well.
*/
static int create_myini()
{
  my_bool enable_named_pipe= FALSE;
  printf("Creating my.ini file\n");

  char path_buf[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, path_buf);

  /* Create ini file. */
  FILE *myini = fopen("my.ini","wt");
  if(!myini)
  {
    die("Cannot create my.ini in data directory");
  }
  /*
    Write out server settings. datadir and basedir are calculated,
    using path to mysqld.exe.
  */
  fprintf(myini, "[mysqld]\n");
  convert_slashes(path_buf);
  fprintf(myini, "datadir=%s\n", path_buf);
  if (opt_skip_networking)
  {
    fprintf(myini,"skip-networking\n");
    if(!opt_socket)
      opt_socket= opt_service;
  }
  enable_named_pipe= (my_bool) 
    ((opt_socket && opt_socket[0]) || opt_skip_networking);

  if(enable_named_pipe)
  {
    fprintf(myini,"enable-named-pipe\n");
  }

  if(opt_socket && opt_socket[0])
  {
    fprintf(myini, "socket=%s\n", opt_socket);
  }
  if (opt_port)
  {
    fprintf(myini,"port=%d\n", opt_port);
  }

#ifdef _WIN64
  /* PBXT does not work in 64 bit windows, sorry. */
  fprintf(myini, "loose-skip-pbxt\n");
#endif

  /* Write out client settings. */
  fprintf(myini, "[client]\n");
  if(opt_socket && opt_socket[0])
    fprintf(myini,"socket=%s\n",opt_socket);
  if(opt_skip_networking)
    fprintf(myini,"protocol=pipe\n");
  if(opt_port)
    fprintf(myini,"port=%d\n",opt_port);
  fclose(myini);
  return 0;
}


static const char update_root_passwd_part1[]=
  "UPDATE mysql.user SET Password = PASSWORD('";
static const char update_root_passwd_part2[]=
  "') where User='root';\n";
static const char remove_default_user_cmd[]= 
  "DELETE FROM mysql.user where User='';\n";
static const char allow_remote_root_access_cmd[]=
  "CREATE TEMPORARY TABLE tmp_user LIKE user;\n"
  "INSERT INTO tmp_user SELECT * from user where user='root' "
    " AND host='localhost';\n"
  "UPDATE tmp_user SET host='%';\n"
  "INSERT INTO user SELECT * FROM tmp_user;\n"
  "DROP TABLE tmp_user;\n";
static const char end_of_script[]="-- end.";

/* Register service. Assume my.ini is in datadir */
static int register_service()
{
  char buf[3*MAX_PATH +32]; /* path to mysqld.exe, to my.ini, service name */
  SC_HANDLE sc_manager, sc_service;

  size_t datadir_len= strlen(opt_datadir);
  const char *backslash_after_datadir="\\";

  if (datadir_len && opt_datadir[datadir_len-1] =='\\')
    backslash_after_datadir="";

  verbose("Registering service");
  my_snprintf(buf, sizeof(buf)-1,
    "\"%s\" \"--defaults-file=%s%smy.ini\" \"%s\"" ,  mysqld_path, opt_datadir, 
    backslash_after_datadir, opt_service);

  /* Get a handle to the SCM database. */ 
  sc_manager= OpenSCManager( 
    NULL,
    NULL,
    SC_MANAGER_ALL_ACCESS);

  if (!sc_manager) 
  {
    die("OpenSCManager failed (%d)\n", GetLastError());
  }

  /* Create the service. */
  sc_service = CreateServiceA(sc_manager, opt_service,  opt_service,
    SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, 
    SERVICE_ERROR_NORMAL, buf, NULL, NULL, NULL, opt_os_user, opt_os_password);

  if (!sc_service) 
  {
    CloseServiceHandle(sc_manager);
    die("CreateService failed (%d)", GetLastError());
  }

  SERVICE_DESCRIPTION sd = { "MariaDB database server" };
  ChangeServiceConfig2(sc_service, SERVICE_CONFIG_DESCRIPTION, &sd);
  CloseServiceHandle(sc_service); 
  CloseServiceHandle(sc_manager);
  return 0;
}


static void clean_directory(const char *dir)
{
  char dir2[MAX_PATH+2];
  size_t len = strlen(dir);

  strcpy_s(dir2, MAX_PATH+2, dir);
  dir2[len+1] = 0;
  
  SHFILEOPSTRUCT fileop;
  fileop.hwnd= NULL;    /* no status display */
  fileop.wFunc= FO_DELETE;  /* delete operation */
  fileop.pFrom= dir2;  /* source file name as double null terminated string */
  fileop.pTo= NULL;    /* no destination needed */
  fileop.fFlags= FOF_NOCONFIRMATION|FOF_SILENT;  /* do not prompt the user */


  fileop.fAnyOperationsAborted= FALSE;
  fileop.lpszProgressTitle= NULL;
  fileop.hNameMappings= NULL;

  SHFileOperation(&fileop);
}

/*
  Define directory permission to have inheritable all access for a user
  (defined as username or group string or as SID)
*/
static int set_directory_permissions(const char *dir, const char *os_user)
{

   struct{
        TOKEN_USER tokenUser;
        BYTE buffer[SECURITY_MAX_SID_SIZE];
   } tokenInfoBuffer;

  HANDLE hDir = CreateFile(dir,READ_CONTROL|WRITE_DAC,0,NULL,OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if(hDir == INVALID_HANDLE_VALUE) 
    return -1;  
  ACL* pOldDACL;
  SECURITY_DESCRIPTOR* pSD = NULL; 
  EXPLICIT_ACCESS ea={0};
  GetSecurityInfo(hDir, SE_FILE_OBJECT , DACL_SECURITY_INFORMATION,NULL, NULL,
    &pOldDACL, NULL, (void**)&pSD); 
  PSID pSid = NULL; 
  if(os_user)
  {
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.ptstrName = (LPSTR)os_user;
  }
  else
  {
    HANDLE token;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY, &token))
    {

      DWORD length=(DWORD) sizeof(tokenInfoBuffer);
      if (GetTokenInformation(token, TokenUser, &tokenInfoBuffer, 
        length, &length))
      {
        pSid= tokenInfoBuffer.tokenUser.User.Sid;
      }
    }
    if(!pSid)
      return 0;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = (LPTSTR)pSid;
  }
  ea.grfAccessMode = GRANT_ACCESS;
  ea.grfAccessPermissions = GENERIC_ALL; 
  ea.grfInheritance = CONTAINER_INHERIT_ACE|OBJECT_INHERIT_ACE; 
  ea.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN; 
  ACL* pNewDACL = 0; 
  DWORD err = SetEntriesInAcl(1,&ea,pOldDACL,&pNewDACL); 
  if(pNewDACL)
  {
    SetSecurityInfo(hDir,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL, NULL,
      pNewDACL, NULL);
  }
  LocalFree(pNewDACL); 
  LocalFree(pSD);
  LocalFree(pOldDACL); 
  CloseHandle(hDir); 
  return 0;
}

/* 
  Give directory permissions for special service user NT SERVICE\servicename
  this user is available only on Win7 and later.
*/
void grant_directory_permissions_to_service()
{
  char service_user[MAX_PATH+ 12];
  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  if (info.dwMajorVersion >6 || 
    (info.dwMajorVersion== 6 && info.dwMinorVersion > 0)
    && opt_service)
  {
    my_snprintf(service_user,sizeof(service_user), "NT SERVICE\\%s", 
      opt_service);
    set_directory_permissions(opt_datadir, service_user);
  }
}

/* Create database instance (including registering as service etc) .*/
static int create_db_instance()
{
  int ret=0;
  char cwd[MAX_PATH];
  DWORD cwd_len= MAX_PATH;
  char cmdline[3*MAX_PATH];
  FILE *in;

  verbose("Running bootstrap");

  GetCurrentDirectory(cwd_len, cwd);
  CreateDirectory(opt_datadir, NULL); /*ignore error, it might already exist */

  if(!SetCurrentDirectory(opt_datadir))
  {
    die("Cannot set current directory to %s\n",opt_datadir);
    return -1;
  }

  CreateDirectory("mysql",NULL);
  CreateDirectory("test", NULL);

  set_directory_permissions(opt_datadir, NULL);
  set_directory_permissions(opt_datadir, default_os_user);

  /* Create mysqld --bootstrap process */
  init_bootstrap_command_line(cmdline, sizeof(cmdline));
  /* verbose("Executing %s", cmdline); */

  in= popen(cmdline, "wt");
  if(!in)
    goto end;

  if (fwrite("use mysql;\n",11,1, in) != 1)
  {
    verbose("ERROR: Cannot write to mysqld's stdin");
    ret = 1;
    goto end;
  }

  /* Write the bootstrap script to stdin. */
  if (fwrite(mysql_bootstrap_sql, strlen(mysql_bootstrap_sql), 1, in) != 1)
  {
    verbose("ERROR: Cannot write to mysqld's stdin");
    ret= 1;
    goto end;
  }

  
  /* Remove default user, if requested. */
  if(!opt_default_user)
  {
    verbose("Removing default user",remove_default_user_cmd);
    fputs(remove_default_user_cmd, in);
    fflush(in);
  }

  if(opt_allow_remote_root_access)
  {
     verbose("Allowing remote access for user root",remove_default_user_cmd);
     fputs(allow_remote_root_access_cmd,in);
     fflush(in);
  }

  /* Change root password if requested. */
  if (opt_password)
  {
    verbose("Changing root password",remove_default_user_cmd);
    fputs(update_root_passwd_part1, in);
    fputs(opt_password, in);
    fputs(update_root_passwd_part2, in);
    fflush(in);
  }


  /*
    On some reason, bootstrap chokes if last command sent via stdin ends with 
    newline, so we supply a dummy comment, that does not end with newline.
  */
  fputs(end_of_script, in);
  fflush(in);

  /* Check if bootstrap has completed successfully. */
  ret= pclose(in);
  if (ret)
  {
    verbose("mysqld returned an error in pclose");
    goto end;
  }

  /* Create my.ini file in data directory.*/
  ret= create_myini();
  if(ret)
    goto end;

  /* Register service if requested. */
  if(opt_service && opt_service[0])
  {
    ret= register_service();
    grant_directory_permissions_to_service();
    if(ret)
      goto end;
  }

end:
  if (ret)
  {
    SetCurrentDirectory(cwd);
    clean_directory(opt_datadir);
  }
  return ret;
}
