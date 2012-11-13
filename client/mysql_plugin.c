/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <m_string.h>
#include <mysql.h>
#include <my_getopt.h>
#include <my_dir.h>
#include <my_global.h>
#include <stdio.h>
#include <string.h>


#define SHOW_VERSION "1.0.0"
#define PRINT_VERSION do { printf("%s  Ver %s Distrib %s\n",    \
                        my_progname, SHOW_VERSION, MYSQL_SERVER_VERSION);    \
                      } while(0)

/* Global variables. */
static uint my_end_arg= 0;
static uint opt_verbose=0;
static uint opt_no_defaults= 0;
static uint opt_print_defaults= 0;
static char *opt_datadir=0, *opt_basedir=0,
            *opt_plugin_dir=0, *opt_plugin_ini=0,
            *opt_mysqld=0, *opt_my_print_defaults=0;
static char bootstrap[FN_REFLEN];


/* plugin struct */
struct st_plugin
{
  const char *name;           /* plugin name */
  const char *so_name;        /* plugin so (library) name */
  const char *components[16]; /* components to load */
} plugin_data;


/* Options */
static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "The basedir for the server.",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd', "The datadir for the server.",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin-dir", 'p', "The plugin dir for the server.",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin-ini", 'i', "Read plugin information from configuration file "
   "specified instead of from <plugin-dir>/<plugin_name>.ini.",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-defaults", 'n', "Do not read values from configuration file.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"print-defaults", 'P', "Show default values from configuration file.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"mysqld", 'm', "Path to mysqld executable. Example: /sbin/temp1/mysql/bin",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"my-print-defaults", 'f', "Path to my_print_defaults executable. "
   "Example: /source/temp11/extra",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v',
    "More verbose output; you can use this multiple times to get even more "
    "verbose output.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
    NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


/* Methods */
static int process_options(int argc, char *argv[], char *operation);
static int check_access();
static int find_tool(const char *tool_name, char *tool_path);
static int find_plugin(char *tp_path);
static int build_bootstrap_file(char *operation, char *bootstrap);
static int dump_bootstrap_file(char *bootstrap_file);
static int bootstrap_server(char *server_path, char *bootstrap_file);


int main(int argc,char *argv[])
{
  int error= 0;
  char tp_path[FN_REFLEN];
  char server_path[FN_REFLEN];
  char operation[16];

  MY_INIT(argv[0]);
  plugin_data.name= 0; // initialize name
  
  /*
    The following operations comprise the method for enabling or disabling
    a plugin. We begin by processing the command options then check the
    directories specified for --datadir, --basedir, --plugin-dir, and
    --plugin-ini (if specified). If the directories are Ok, we then look
    for the mysqld executable and the plugin soname. Finally, we build a
    bootstrap command file for use in bootstraping the server.
    
    If any step fails, the method issues an error message and the tool exits.
    
      1) Parse, execute, and verify command options.
      2) Check access to directories.
      3) Look for mysqld executable.
      4) Look for the plugin.
      5) Build a bootstrap file with commands to enable or disable plugin.
      
  */
  if ((error= process_options(argc, argv, operation)) ||
      (error= check_access()) ||
      (error= find_tool("mysqld" FN_EXEEXT, server_path)) ||
      (error= find_plugin(tp_path)) ||
      (error= build_bootstrap_file(operation, bootstrap)))
    goto exit;
  
  /* Dump the bootstrap file if --verbose specified. */
  if (opt_verbose && ((error= dump_bootstrap_file(bootstrap))))
    goto exit;
  
  /* Start the server in bootstrap mode and execute bootstrap commands */
  error= bootstrap_server(server_path, bootstrap);

exit:
  /* Remove file */
  my_delete(bootstrap, MYF(0));
  if (opt_verbose && error == 0)
  {
    printf("# Operation succeeded.\n");
  }

  my_end(my_end_arg);
  exit(error ? 1 : 0);
  return 0;        /* No compiler warnings */
}


/**
  Get a temporary file name.

  @param[out]  filename   The file name of the temporary file
  @param[in]   ext        An extension for the file (optional)

  @retval int error = 1, success = 0
*/

static int make_tempfile(char *filename, const char *ext)
{
  int fd= 0;

  if ((fd=create_temp_file(filename, NullS, ext, O_CREAT | O_WRONLY,
         MYF(MY_WME))) < 0)
  {
    fprintf(stderr, "ERROR: Cannot generate temporary file. Error code: %d.\n",
            fd);
    return 1;
  }
  my_close(fd, MYF(0));
  return 0;
}


/**
  Get the value of an option from a string read from my_print_defaults output.

  @param[in]  line   The line (string) read from the file
  @param[in]  item   The option to search for (e.g. --datadir)

  @returns NULL if not found, string containing value if found
*/

static char *get_value(char *line, const char *item)
{
  char *destination= 0;
  int item_len= (int)strlen(item);
  int line_len = (int)strlen(line);

  if ((strncasecmp(line, item, item_len) == 0))
  {
    int start= 0;
    char *s= 0;

    s = line + item_len + 1;
    destination= my_strndup(s, line_len - start, MYF(MY_FAE));
    destination[line_len - item_len - 2]= 0;
  }
  return destination;
}


/**
  Run a command in a shell.

  This function will attempt to execute the command specified by using the
  popen() method to open a shell and execute the command passed and store the
  output in a result file. If the --verbose option was specified, it will open
  the result file and print the contents to stdout.

  @param[in]  cmd   The command to execute.
  @param[in]  mode  The mode for popen() (e.g. "r", "w", "rw")

  @return int error code or 0 for success.
*/

static int run_command(char* cmd, const char *mode)
{
  char buf[512]= {0};
  FILE *res_file;
  int error;

  if (!(res_file= popen(cmd, mode)))
    return -1;

  if (opt_verbose)
  {
    while (fgets(buf, sizeof(buf), res_file))
    {
      fprintf(stdout, "%s", buf);
    }
  }
  error= pclose(res_file);
  return error;
}


#ifdef __WIN__
/**
  Check to see if there are spaces in a path.
  
  @param[in]  path  The Windows path to examine.

  @retval int spaces found = 1, no spaces = 0
*/
static int has_spaces(const char *path)
{
  if (strchr(path, ' ') != NULL)
    return 1;
  return 0;
}


/**
  Convert a Unix path to a Windows path.
 
  @param[in]  path  The Windows path to examine.

  @returns string containing path with / changed to \\
*/
static char *convert_path(const char *argument)
{
  /* Convert / to \\ to make Windows paths */
  char *winfilename= my_strdup(argument, MYF(MY_FAE));
  char *pos, *end;
  int length= strlen(argument);

  for (pos= winfilename, end= pos+length ; pos < end ; pos++)
  {
    if (*pos == '/')
    {
      *pos= '\\';
    }
  }
  return winfilename;
}


/**
  Add quotes if the path has spaces in it.

  @param[in]  path  The Windows path to examine.

  @returns string containing excaped quotes if spaces found in path
*/
static char *add_quotes(const char *path)
{
  char windows_cmd_friendly[FN_REFLEN];

  if (has_spaces(path))
    snprintf(windows_cmd_friendly, sizeof(windows_cmd_friendly),
             "\"%s\"", path);
  else
    snprintf(windows_cmd_friendly, sizeof(windows_cmd_friendly),
             "%s", path);
  return my_strdup(windows_cmd_friendly, MYF(MY_FAE));
}
#endif


/**
  Get the default values from the my.cnf file.

  This method gets the default values for the following parameters:

  --datadir
  --basedir
  --plugin-dir
  --plugin-ini

  These values are used if the user has not specified a value.

  @retval int error = 1, success = 0
*/

static int get_default_values()
{
  char tool_path[FN_REFLEN];
  char defaults_cmd[FN_REFLEN];
  char defaults_file[FN_REFLEN];
  char line[FN_REFLEN];
  int error= 0;
  int ret= 0;
  FILE *file= 0;

  memset(tool_path, 0, FN_REFLEN);
  if ((error= find_tool("my_print_defaults" FN_EXEEXT, tool_path)))
    goto exit;
  else
  {
    if ((error= make_tempfile(defaults_file, "txt")))
      goto exit;

#ifdef __WIN__
    {
      char *format_str= 0;
  
      if (has_spaces(tool_path) || has_spaces(defaults_file))
        format_str = "\"%s mysqld > %s\"";
      else
        format_str = "%s mysqld > %s";
  
      snprintf(defaults_cmd, sizeof(defaults_cmd), format_str,
               add_quotes(tool_path), add_quotes(defaults_file));
      if (opt_verbose)
      {
        printf("# my_print_defaults found: %s\n", tool_path);
      }
    }
#else
    snprintf(defaults_cmd, sizeof(defaults_cmd),
             "%s mysqld > %s", tool_path, defaults_file);
#endif

    /* Execute the command */
    if (opt_verbose)
    {
      printf("# Command: %s\n", defaults_cmd);
    }
    error= run_command(defaults_cmd, "r");
    if (error)
    {
      fprintf(stderr, "ERROR: my_print_defaults failed. Error code: %d.\n",
              ret);
      goto exit;
    }
    /* Now open the file and read the defaults we want. */
    file= fopen(defaults_file, "r");
    while (fgets(line, FN_REFLEN, file) != NULL)
    {
      char *value= 0;

      if ((opt_datadir == 0) && ((value= get_value(line, "--datadir"))))
      {
        opt_datadir= my_strdup(value, MYF(MY_FAE));
      }
      if ((opt_basedir == 0) && ((value= get_value(line, "--basedir"))))
      {
        opt_basedir= my_strdup(value, MYF(MY_FAE));
      }
      if ((opt_plugin_dir == 0) && ((value= get_value(line, "--plugin_dir"))))
      {
        opt_plugin_dir= my_strdup(value, MYF(MY_FAE));
      }
      if ((opt_plugin_ini == 0) && ((value= get_value(line, "--plugin_ini"))))
      {
        opt_plugin_ini= my_strdup(value, MYF(MY_FAE));
      }
    }
  }
exit:
  if (file)
  {
    fclose(file);
    /* Remove file */
    my_delete(defaults_file, MYF(0));
  }
  return error;
}


/**
  Print usage.
*/

static void usage(void)
{
  PRINT_VERSION;
  puts("Copyright (c) 2011, Oracle and/or its affiliates. "
       "All rights reserved.\n");
  puts("Enable or disable plugins.");
  printf("\nUsage: %s [options] <plugin> ENABLE|DISABLE\n\nOptions:\n",
     my_progname);
  my_print_help(my_long_options);
  puts("\n");
}


/**
  Print the default values as read from the my.cnf file.

  This method displays the default values for the following parameters:

  --datadir
  --basedir
  --plugin-dir
  --plugin-ini

*/

static void print_default_values(void)
{
  printf("%s would have been started with the following arguments:\n",
         my_progname);
  get_default_values();
  if (opt_datadir)
  {
    printf("--datadir=%s ", opt_datadir);
  }
  if (opt_basedir)
  {
    printf("--basedir=%s ", opt_basedir);
  }
  if (opt_plugin_dir)
  {
    printf("--plugin_dir=%s ", opt_plugin_dir);
  }
  if (opt_plugin_ini)
  {
    printf("--plugin_ini=%s ", opt_plugin_ini);
  }
  if (opt_mysqld)
  {
    printf("--mysqld=%s ", opt_mysqld);
  }
  if (opt_my_print_defaults)
  {
    printf("--my_print_defaults=%s ", opt_my_print_defaults);
  }
  printf("\n");
}


/**
  Process the arguments and identify an option and store its value.

  @param[in]  optid      The single character shortcut for the argument.
  @param[in]  my_option  Structure of legal options.
  @param[in]  argument   The argument value to process.
*/

static my_bool
get_one_option(int optid,
               const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  switch(optid) {
  case 'n':
    opt_no_defaults++;
    break;
  case 'P':
    opt_print_defaults++;
    print_default_values();
    break;
  case 'v':
    opt_verbose++;
    break;
  case 'V':
    PRINT_VERSION;
    exit(0);
    break;
  case '?':
  case 'I':          /* Info */
    usage();
    exit(0);
  case 'd':
    opt_datadir= my_strdup(argument, MYF(MY_FAE));
    break;
  case 'b':
    opt_basedir= my_strdup(argument, MYF(MY_FAE));
    break;
  case 'p':
    opt_plugin_dir= my_strdup(argument, MYF(MY_FAE));
    break;
  case 'i':
    opt_plugin_ini= my_strdup(argument, MYF(MY_FAE));
    break;
  case 'm':
    opt_mysqld= my_strdup(argument, MYF(MY_FAE));
    break;
  case 'f':
    opt_my_print_defaults= my_strdup(argument, MYF(MY_FAE));
    break;
  }
  return 0;
}


/**
  Check to see if a file exists.

  @param[in]  filename  File to locate.

  @retval int file not found = 1, file found = 0
*/

static int file_exists(char * filename)
{
  MY_STAT stat_arg;

  if (!my_stat(filename, &stat_arg, MYF(0)))
  {
    return 0;
  }
  return 1;
}


/**
  Search a specific path and sub directory for a file name.

  @param[in]  base_path  Original path to use.
  @param[in]  tool_name  Name of the tool to locate.
  @param[in]  subdir     The sub directory to search.
  @param[out] tool_path  If tool found, return complete path.

  @retval int error = 1, success = 0
*/

static int search_dir(const char * base_path, const char *tool_name,
                      const char *subdir, char *tool_path)
{
  char new_path[FN_REFLEN];
  char source_path[FN_REFLEN];
#if __WIN__
  char win_abs_path[FN_REFLEN];
  char self_name[FN_REFLEN];
  const char *last_fn_libchar;
#endif

  strcpy(source_path, base_path);
  strcat(source_path, subdir);
  fn_format(new_path, tool_name, source_path, "", MY_UNPACK_FILENAME);
  if (file_exists(new_path))
  {
    strcpy(tool_path, new_path);
    return 1;
  }  

#if __WIN__
  /*
    On Windows above code will not be able to find the file since
    path names are not absolute and file_exists works only with 
    absolute path names. Try to get the absolute path of current 
    exe and see if the file exists relative to the path of exe.
  */
  if (GetModuleFileName(NULL, self_name, FN_REFLEN -1) == 0)
    strncpy(self_name, my_progname, FN_REFLEN - 1);
  self_name[FN_REFLEN - 1]= '\0';

  last_fn_libchar= strrchr(self_name, FN_LIBCHAR);
  if (NULL != last_fn_libchar)
  {
    strncpy(win_abs_path, self_name, last_fn_libchar - self_name + 1 );
    win_abs_path[(last_fn_libchar - self_name + 1)]= 0;
    strncat(win_abs_path, new_path, 
            sizeof(win_abs_path) - strlen(win_abs_path) - 1);
    if (file_exists(win_abs_path))
    {
      strcpy(tool_path, win_abs_path);
      return 1;
    }
  }
#endif
  return 0;
}


/**
  Search known common paths and sub directories for a file name.

  @param[in]  base_path  Original path to use.
  @param[in]  tool_name  Name of the tool to locate.
  @param[out] tool_path  If tool found, return complete path.

  @retval int error = 1, success = 0
*/

static int search_paths(const char *base_path, const char *tool_name,
                        char *tool_path)
{
  int i= 0;

  static const char *paths[]= {
    "", "/share/",  "/scripts/", "/bin/", "/sbin/", "/libexec/",
    "/mysql/", "/sql/",
  };
  for (i = 0 ; i < (int)array_elements(paths); i++)
  {
    if (search_dir(base_path, tool_name, paths[i], tool_path))
    {
      return 1;
    }
  }
  return 0;
}


/**
  Read the plugin ini file.

  This function attempts to read the plugin config file from the plugin_dir
  path saving the data in the the st_plugin structure. If the file is not
  found or the file cannot be read, an error is generated.

  @retval int error = 1, success = 0
*/

static int load_plugin_data(char *plugin_name, char *config_file)
{
  FILE *file_ptr;
  char path[FN_REFLEN];
  char line[1024];
  char *reason= 0;
  char *res;
  int i= -1;

  if (opt_plugin_ini == 0)
  {
    fn_format(path, config_file, opt_plugin_dir, "", MYF(0));
    opt_plugin_ini= my_strdup(path, MYF(MY_FAE));
  }
  if (!file_exists(opt_plugin_ini))
  {
    reason= (char *)"File does not exist.";
    goto error;
  }

  file_ptr= fopen(opt_plugin_ini, "r");
  if (file_ptr == NULL)
  {
    reason= (char *)"Cannot open file.";
    goto error;
  }

  /* save name */
  plugin_data.name= my_strdup(plugin_name, MYF(MY_WME));

  /* Read plugin components */
  while (i < 16)
  {
    res= fgets(line, sizeof(line), file_ptr);
    /* strip /n */
    if (line[strlen(line)-1] == '\n')
    {
      line[strlen(line)-1]= '\0';
    }
    if (res == NULL)
    {
      if (i < 1)
      {
        reason= (char *)"Bad format in plugin configuration file.";
        fclose(file_ptr);
        goto error;        
      }
      break;
    }
    if ((line[0] == '#') || (line[0] == '\n')) // skip comment and blank lines
    {
      continue;
    }
    if (i == -1) // if first pass, read this line as so_name
    {
      /* Add proper file extension for soname */
      strcat(line, FN_SOEXT);
      /* save so_name */
      plugin_data.so_name= my_strdup(line, MYF(MY_WME|MY_ZEROFILL));
      i++;
    }
    else
    {
      if (strlen(line) > 0)
      {
        plugin_data.components[i]= my_strdup(line, MYF(MY_WME));
        i++;
      }
      else
      {
        plugin_data.components[i]= NULL;
      }
    }
  }
  
  fclose(file_ptr);
  return 0;

error:
  fprintf(stderr, "ERROR: Cannot read plugin config file %s. %s\n",
          plugin_name, reason);
  return 1;
}


/**
  Check the options for validity.

  This function checks the arguments for validity issuing the appropriate
  error message if arguments are missing or invalid. On success, @operation
  is set to either "ENABLE" or "DISABLE".

  @param[in]  argc       The number of arguments.
  @param[in]  argv       The arguments.
  @param[out] operation  The operation chosen (enable|disable)

  @retval int error = 1, success = 0
*/

static int check_options(int argc, char **argv, char *operation)
{
  int i= 0;                    // loop counter
  int num_found= 0;            // number of options found (shortcut loop)
  char config_file[FN_REFLEN]; // configuration file name
  char plugin_name[FN_REFLEN]; // plugin name
  
  /* Form prefix strings for the options. */
  const char *basedir_prefix = "--basedir=";
  int basedir_len= strlen(basedir_prefix);
  const char *datadir_prefix = "--datadir=";
  int datadir_len= strlen(datadir_prefix);
  const char *plugin_dir_prefix = "--plugin_dir=";
  int plugin_dir_len= strlen(plugin_dir_prefix);

  strcpy(plugin_name, "");
  for (i = 0; i < argc && num_found < 5; i++)
  {

    if (!argv[i])
    {
      continue;
    }
    if ((strcasecmp(argv[i], "ENABLE") == 0) ||
        (strcasecmp(argv[i], "DISABLE") == 0))
    {
      strcpy(operation, argv[i]);
      num_found++;
    }
    else if ((strncasecmp(argv[i], basedir_prefix, basedir_len) == 0) &&
             !opt_basedir)
    {
      opt_basedir= my_strndup(argv[i]+basedir_len,
                              strlen(argv[i])-basedir_len, MYF(MY_FAE));
      num_found++;
    }
    else if ((strncasecmp(argv[i], datadir_prefix, datadir_len) == 0) &&
             !opt_datadir)
    {
      opt_datadir= my_strndup(argv[i]+datadir_len,
                              strlen(argv[i])-datadir_len, MYF(MY_FAE));
      num_found++;
    }
    else if ((strncasecmp(argv[i], plugin_dir_prefix, plugin_dir_len) == 0) &&
             !opt_plugin_dir)
    {
      opt_plugin_dir= my_strndup(argv[i]+plugin_dir_len,
                                 strlen(argv[i])-plugin_dir_len, MYF(MY_FAE));
      num_found++;
    }
    /* read the plugin config file and check for match against argument */
    else
    {
      strcpy(plugin_name, argv[i]);
      strcpy(config_file, argv[i]);
      strcat(config_file, ".ini");
    }
  }

  if (!opt_basedir)
  {
    fprintf(stderr, "ERROR: Missing --basedir option.\n");
    return 1;
  }

  if (!opt_datadir)
  {
    fprintf(stderr, "ERROR: Missing --datadir option.\n");
    return 1;
  }

  if (!opt_plugin_dir)
  {
    fprintf(stderr, "ERROR: Missing --plugin_dir option.\n");
    return 1;
  }
  /* If a plugin was specified, read the config file. */
  else if (strlen(plugin_name) > 0) 
  {
    if (load_plugin_data(plugin_name, config_file))
    {
      return 1;
    }
    if (strcasecmp(plugin_data.name, plugin_name) != 0)
    {
      fprintf(stderr, "ERROR: plugin name requested does not match config "
              "file data.\n");
      return 1;
    }
  }
  else
  {
    fprintf(stderr, "ERROR: No plugin specified.\n");
    return 1;
  }

  if ((strlen(operation) == 0))
  {
    fprintf(stderr, "ERROR: missing operation. Please specify either "
            "'<plugin> ENABLE' or '<plugin> DISABLE'.\n");
    return 1;
  }

  return 0;
}


/**
  Parse, execute, and verify command options.
  
  This method handles all of the option processing including the optional
  features for displaying data (--print-defaults, --help ,etc.) that do not
  result in an attempt to ENABLE or DISABLE of a plugin.
  
  @param[in]   arc        Count of arguments
  @param[in]   argv       Array of arguments
  @param[out]  operation  Operation (ENABLE or DISABLE)
  
  @retval int error = 1, success = 0, exit program = -1
*/

static int process_options(int argc, char *argv[], char *operation)
{
  int error= 0;
  int i= 0;
  
  /* Parse and execute command-line options */
  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    goto exit;

  /* If the print defaults option used, exit. */
  if (opt_print_defaults)
  {
    error= -1;
    goto exit;
  }

  /* Add a trailing directory separator if not present */
  if (opt_basedir)
  {
    i= (int)strlength(opt_basedir);
    if (opt_basedir[i-1] != FN_LIBCHAR || opt_basedir[i-1] != FN_LIBCHAR2)
    {
      char buff[FN_REFLEN];
      
      strncpy(buff, opt_basedir, sizeof(buff) - 1);
#ifdef __WIN__
      strncat(buff, "/", sizeof(buff) - strlen(buff) - 1);
#else
      strncat(buff, FN_DIRSEP, sizeof(buff) - strlen(buff) - 1);
#endif
      buff[sizeof(buff) - 1]= 0;
      my_delete(opt_basedir, MYF(0));
      opt_basedir= my_strdup(buff, MYF(MY_FAE));
    }
  }
  
  /*
    If the user did not specify the option to skip loading defaults from a
    config file and the required options are not present or there was an error
    generated when the defaults were read from the file, exit.
  */
  if (!opt_no_defaults && ((error= get_default_values())))
  {
    error= -1;
    goto exit;
  }

  /*
   Check to ensure required options are present and validate the operation.
   Note: this method also validates the plugin specified by attempting to
   read a configuration file named <plugin_name>.ini from the --plugin-dir
   or --plugin-ini location if the --plugin-ini option presented.
  */
  strcpy(operation, "");
  if ((error = check_options(argc, argv, operation)))
  {
    goto exit;
  }

  if (opt_verbose)
  {
    printf("#    basedir = %s\n", opt_basedir);
    printf("# plugin_dir = %s\n", opt_plugin_dir);
    printf("#    datadir = %s\n", opt_datadir);
    printf("# plugin_ini = %s\n", opt_plugin_ini);
  }

exit:
  return error;
}


/**
  Check access
  
  This method checks to ensure all of the directories (opt_basedir,
  opt_plugin_dir, opt_datadir, and opt_plugin_ini) are accessible by
  the user.
  
  @retval int error = 1, success = 0
*/

static int check_access()
{
  int error= 0;
  
  if ((error= my_access(opt_basedir, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access basedir at '%s'.\n",
            opt_basedir);
    goto exit;
  }
  if ((error= my_access(opt_plugin_dir, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access plugin_dir at '%s'.\n",
            opt_plugin_dir);
    goto exit;
  }
  if ((error= my_access(opt_datadir, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access datadir at '%s'.\n",
            opt_datadir);
    goto exit;
  }
  if (opt_plugin_ini && (error= my_access(opt_plugin_ini, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access plugin config file at '%s'.\n",
            opt_plugin_ini);
    goto exit;
  }
  if (opt_mysqld && (error= my_access(opt_mysqld, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access mysqld path '%s'.\n",
            opt_mysqld);
    goto exit;
  }
  if (opt_my_print_defaults && (error= my_access(opt_my_print_defaults, F_OK)))
  {
    fprintf(stderr, "ERROR: Cannot access my-print-defaults path '%s'.\n",
            opt_my_print_defaults);
    goto exit;
  }

exit:
  return error;
}


/**
  Locate the tool and form tool path.

  @param[in]  tool_name  Name of the tool to locate.
  @param[out] tool_path  If tool found, return complete path.

  @retval int error = 1, success = 0
*/

static int find_tool(const char *tool_name, char *tool_path)
{
  int i= 0;

  const char *paths[]= {
    opt_mysqld, opt_basedir, opt_my_print_defaults, "", 
    "/usr", "/usr/local/mysql", "/usr/sbin", "/usr/share", 
    "/extra", "/extra/debug", "/../../extra/debug", 
    "/release/", "/extra/release", "/../../extra/release",
    "/bin", "/usr/bin", "/mysql/bin"
  };
  for (i= 0; i < (int)array_elements(paths); i++)
  {
    if (paths[i] && (search_paths(paths[i], tool_name, tool_path)))
      goto found;
  }
  fprintf(stderr, "WARNING: Cannot find %s.\n", tool_name);
  return 1;
found:
  if (opt_verbose)
    printf("# Found tool '%s' as '%s'.\n", tool_name, tool_path);
  return 0;
}


/**
  Find the plugin library.

  This function attempts to use the @c plugin_dir option passed on the
  command line to locate the plugin.

  @param[out] tp_path   The actual path to plugin with FN_SOEXT applied.

  @retval int error = 1, success = 0
*/

static int find_plugin(char *tp_path)
{
  /* Check for existance of plugin */
  fn_format(tp_path, plugin_data.so_name, opt_plugin_dir, "", MYF(0));
  if (!file_exists(tp_path))
  {
    fprintf(stderr, "ERROR: The plugin library is missing or in a different"
            " location.\n");
    return 1;
  }
  else if (opt_verbose)
  {
    printf("# Found plugin '%s' as '%s'\n", plugin_data.name, tp_path);
  }
  return 0;
}


/**
  Build the boostrap file.
  
  Create a new file and populate it with SQL commands to ENABLE or DISABLE
  the plugin via REPLACE and DELETE operations on the mysql.plugin table.

  param[in]  operation  The type of operation (ENABLE or DISABLE)
  param[out] bootstrap  A FILE* pointer
  
  @retval int error = 1, success = 0
*/

static int build_bootstrap_file(char *operation, char *bootstrap)
{
  int error= 0;
  FILE *file= 0;
  
  /*
    Perform plugin operation : ENABLE or DISABLE

    The following creates a temporary bootstrap file and populates it with
    the appropriate SQL commands for the operation. For ENABLE, REPLACE
    statements are created. For DISABLE, DELETE statements are created. The
    values for these statements are derived from the plugin_data read from the
    <plugin_name>.ini configuration file. Once the file is built, a call to
    mysqld is made in read only, bootstrap modes to read the SQL statements
    and execute them.
    
    Note: Replace was used so that if a user loads a newer version of a
          library with a different library name, the new library name is
          used for symbols that match. 
  */
  if ((error= make_tempfile(bootstrap, "sql")))
  {
    /* Fail if we cannot create a temporary file for the bootstrap commands. */
    fprintf(stderr, "ERROR: Cannot create bootstrap file.\n");
    goto exit;
  }
  if ((file= fopen(bootstrap, "w+")) == NULL)
  {
    fprintf(stderr, "ERROR: Cannot open bootstrap file for writing.\n");
    error= 1;
    goto exit;
  }
  if (strcasecmp(operation, "enable") == 0)
  {
    int i= 0;
    fprintf(file, "REPLACE INTO mysql.plugin VALUES ");
    for (i= 0; i < (int)array_elements(plugin_data.components); i++)
    {
      /* stop when we read the end of the symbol list - marked with NULL */
      if (plugin_data.components[i] == NULL)
      {
        break;
      }
      if (i > 0)
      {
        fprintf(file, ", ");
      }
      fprintf(file, "('%s','%s')",
              plugin_data.components[i], plugin_data.so_name);
    }
    fprintf(file, ";\n");
    if (opt_verbose)
    {
      printf("# Enabling %s...\n", plugin_data.name);
    }
  }
  else
  {
    fprintf(file,
            "DELETE FROM mysql.plugin WHERE dl = '%s';", plugin_data.so_name);
    if (opt_verbose)
    {
      printf("# Disabling %s...\n", plugin_data.name);
    }
  }
  
exit:
  fclose(file);
  return error;
}


/**
  Dump bootstrap file.
  
  Read the contents of the bootstrap file and print it out.
  
  @param[in]  bootstrap_file  Name of bootstrap file to read
  
  @retval int error = 1, success = 0
*/

static int dump_bootstrap_file(char *bootstrap_file)
{
  char *ret= 0;
  int error= 0;
  char query_str[512];
  FILE *file= 0;

  if ((file= fopen(bootstrap_file, "r")) == NULL)
  {
    fprintf(stderr, "ERROR: Cannot open bootstrap file for reading.\n");
    error= 1;
    goto exit;
  }
  ret= fgets(query_str, 512, file);
  if (ret == 0)
  {
    fprintf(stderr, "ERROR: Cannot read bootstrap file.\n");
    error= 1;
    goto exit;
  }
  printf("# Query: %s\n", query_str);

exit:
  if (file)
  {
    fclose(file);
  }
  return error;
}


/**
  Bootstrap the server
  
  Create a command line sequence to launch mysqld in bootstrap mode. This
  will allow mysqld to launch a minimal server instance to read and
  execute SQL commands from a file piped in (the boostrap file). We use
  the --no-defaults option to skip reading values from the config file.

  The bootstrap mode skips loading of plugins and many other subsystems.
  This allows the mysql_plugin tool to insert the correct rows into the
  mysql.plugin table (for ENABLE) or delete the rows (for DISABLE). Once
  the server is launched in normal mode, the plugin will be loaded
  (for ENABLE) or not loaded (for DISABLE). In this way, we avoid the
  (sometimes) complicated LOAD PLUGIN commands.

  @param[in]  server_path     Path to server executable
  @param[in]  bootstrap_file  Name of bootstrap file to read

  @retval int error = 1, success = 0
*/

static int bootstrap_server(char *server_path, char *bootstrap_file)
{
  char bootstrap_cmd[FN_REFLEN];
  int error= 0;

#ifdef __WIN__
  char *format_str= 0;
  const char *verbose_str= NULL;
   
  
  if (opt_verbose)
    verbose_str= "--console";
  else
    verbose_str= "";
  if (has_spaces(opt_datadir) || has_spaces(opt_basedir) ||
      has_spaces(bootstrap_file))
    format_str= "\"%s %s --bootstrap --datadir=%s --basedir=%s < %s\"";
  else 
    format_str= "%s %s --bootstrap --datadir=%s --basedir=%s < %s";

  snprintf(bootstrap_cmd, sizeof(bootstrap_cmd), format_str,
           add_quotes(convert_path(server_path)), verbose_str,
           add_quotes(opt_datadir), add_quotes(opt_basedir),
           add_quotes(bootstrap_file));
#else
  snprintf(bootstrap_cmd, sizeof(bootstrap_cmd),
           "%s --no-defaults --bootstrap --datadir=%s --basedir=%s"
           " < %s", server_path, opt_datadir, opt_basedir, bootstrap_file);
#endif

  /* Execute the command */
  if (opt_verbose)
  {
    printf("# Command: %s\n", bootstrap_cmd);
  }
  error= run_command(bootstrap_cmd, "r");
  if (error)
    fprintf(stderr,
            "ERROR: Unexpected result from bootstrap. Error code: %d.\n",
            error);
  
  return error;
}
