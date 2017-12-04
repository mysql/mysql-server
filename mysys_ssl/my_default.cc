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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/****************************************************************************
 Add all options from files named "group".cnf from the default_directories
 before the command line arguments.
 On Windows defaults will also search in the Windows directory for a file
 called 'group'.ini
 As long as the program uses the last argument for conflicting
 options one only have to add a call to "load_defaults" to enable
 use of default values.
 pre- and end 'blank space' are removed from options and values. The
 following escape sequences are recognized in values:  \b \t \n \r \\

 The following arguments are handled automatically;  If used, they must be
 first argument on the command line!
 --no-defaults	; no options are read, except for the ones provided in the
                  login file.
 --defaults-file=full-path-to-default-file	; Only this file will be read.
 --defaults-extra-file=full-path-to-default-file ; Read this file before ~/
 --defaults-group-suffix  ; Also read groups with concat(group, suffix)
 --print-defaults	  ; Print the modified command line and exit
 --login-path=login-path-name ; Read options under login-path-name from
                                the login file.
****************************************************************************/

#include "../mysys/mysys_priv.h"
#include "my_default.h"
#include "my_default_priv.h"
#include "m_string.h"
#include "m_ctype.h"
#include <my_dir.h>
#include <my_aes.h>
#include "mysql/psi/mysql_file.h"
#ifdef _WIN32
#include <winbase.h>
#endif

#include "prealloced_array.h"

C_MODE_START
#ifdef HAVE_PSI_INTERFACE
extern PSI_file_key key_file_cnf;
#endif
PSI_memory_key key_memory_defaults;
C_MODE_END

/**
   arguments separator

   load_defaults() loads arguments from config file and put them
   before the arguments from command line, this separator is used to
   separate the arguments loaded from config file and arguments user
   provided on command line.

   Options with value loaded from config file are always in the form
   '--option=value', while for command line options, the value can be
   given as the next argument. Thus we used a separator so that
   handle_options() can distinguish them.

   Note: any other places that does not need to distinguish them
   should skip the separator.

   The content of arguments separator does not matter, one should only
   check the pointer, use "----args-separator----" here to ease debug
   if someone misused it.

   The args seprator will only be added when
   my_getopt_use_args_seprator is set to TRUE before calling
   load_defaults();

   See BUG#25192
*/
static const char *args_separator= "----args-separator----";
inline static void set_args_separator(char** arg)
{
  DBUG_ASSERT(my_getopt_use_args_separator);
  *arg= (char*)args_separator;
}

/*
  This flag indicates that the argument separator string
  (args_separator) should be added to the list of arguments,
  in order to separate arguments received from config file
  and command line.
*/
my_bool my_getopt_use_args_separator= FALSE;
my_bool my_getopt_is_args_separator(const char* arg)
{
  return (arg == args_separator);
}
const char *my_defaults_file=0;
const char *my_defaults_group_suffix=0;
const char *my_defaults_extra_file=0;

static const char *my_login_path= 0;

static char my_defaults_file_buffer[FN_REFLEN];
static char my_defaults_extra_file_buffer[FN_REFLEN];

static my_bool defaults_already_read= FALSE;

/* Which directories are searched for options (and in which order) */

#define MAX_DEFAULT_DIRS 6
#define DEFAULT_DIRS_SIZE (MAX_DEFAULT_DIRS + 1)  /* Terminate with NULL */
static const char **default_directories = NULL;

#ifdef _WIN32
static const char *f_extensions[]= { ".ini", ".cnf", 0 };
#define NEWLINE "\r\n"
#else
static const char *f_extensions[]= { ".cnf", 0 };
#define NEWLINE "\n"
#endif

extern "C" {
static int handle_default_option(void *in_ctx, const char *group_name,
                                 const char *option);
}

/*
   This structure defines the context that we pass to callback
   function 'handle_default_option' used in search_default_file
   to process each option. This context is used if search_default_file
   was called from load_defaults.
*/

typedef Prealloced_array<char*, 100> My_args;
struct handle_option_ctx
{
   MEM_ROOT *alloc;
   My_args *m_args;
   TYPELIB *group;
};

static int search_default_file(Process_option_func func, void *func_ctx,
                               const char *dir, const char *config_file,
                               my_bool is_login_file);
static int search_default_file_with_ext(Process_option_func func,
                                        void *func_ctx,
					const char *dir, const char *ext,
                                        const char *config_file,
                                        int recursion_level,
                                        my_bool is_login_file);
static my_bool mysql_file_getline(char *str, int size, MYSQL_FILE *file,
                                  my_bool is_login_file);

/**
  Create the list of default directories.

  @param alloc  MEM_ROOT where the list of directories is stored

  @details
  The directories searched, in order, are:
  - Windows:     GetSystemWindowsDirectory()
  - Windows:     GetWindowsDirectory()
  - Windows:     C:/
  - Windows:     Directory above where the executable is located
  - Unix:        /etc/
  - Unix:        /etc/mysql/
  - Unix:        --sysconfdir=<path> (compile-time option)
  - ALL:         getenv("MYSQL_HOME")
  - ALL:         --defaults-extra-file=<path> (run-time option)
  - Unix:        ~/

  On all systems, if a directory is already in the list, it will be moved
  to the end of the list.  This avoids reading defaults files multiple times,
  while ensuring the correct precedence.

  @retval NULL  Failure (out of memory, probably)
  @retval other Pointer to NULL-terminated array of default directories
*/

static const char **init_default_directories(MEM_ROOT *alloc);


static char *remove_end_comment(char *ptr);


/*
  Expand a file name so that the current working directory is added if
  the name is relative.

  RETURNS
   0   All OK
   2   Out of memory or path too long
   3   Not able to get working directory
 */

static int
fn_expand(const char *filename, char *result_buf)
{
  char dir[FN_REFLEN];
  const int flags= MY_UNPACK_FILENAME | MY_SAFE_PATH | MY_RELATIVE_PATH;
  DBUG_ENTER("fn_expand");
  DBUG_PRINT("enter", ("filename: %s, result_buf: 0x%lx",
                       filename, (unsigned long) result_buf));
  if (my_getwd(dir, sizeof(dir), MYF(0)))
    DBUG_RETURN(3);
  DBUG_PRINT("debug", ("dir: %s", dir));
  if (fn_format(result_buf, filename, dir, "", flags) == NULL)
    DBUG_RETURN(2);
  DBUG_PRINT("return", ("result: %s", result_buf));
  DBUG_RETURN(0);
}

/*
  Process config files in default directories.

  SYNOPSIS
  my_search_option_files()
  conf_file                   Basename for configuration file to search for.
                              If this is a path, then only this file is read.
  argc                        Pointer to argc of original program
  argv                        Pointer to argv of original program
  args_used                   Pointer to variable for storing the number of
                              arguments used.
  func                        Pointer to the function to process options
  func_ctx                    It's context. Usually it is the structure to
                              store additional options.
  default_directories         List of default directories.
  found_no_defaults           TRUE, if --no-defaults is specified.

  DESCRIPTION
    Process the default options from argc & argv
    Read through each found config file looks and calls 'func' to process
    each option. This function also reads options from login file.

  NOTES
    --defaults-group-suffix is only processed if we are called from
    load_defaults().


  RETURN
    0  ok
    1  given cinf_file doesn't exist
    2  out of memory
    3  Can't get current working directory

    The global variable 'my_defaults_group_suffix' is updated with value for
    --defaults_group_suffix
*/

int my_search_option_files(const char *conf_file, int *argc, char ***argv,
                           uint *args_used, Process_option_func func,
                           void *func_ctx, const char **default_directories,
                           my_bool is_login_file, my_bool found_no_defaults)
{
  const char **dirs, *forced_default_file, *forced_extra_defaults;
  int error= 0;
  DBUG_ENTER("my_search_option_files");

  /* Skip for login file. */
  if (! is_login_file)
  {
    /* Check if we want to force the use a specific default file */
    *args_used+= get_defaults_options(*argc - *args_used, *argv + *args_used,
                                      (char **) &forced_default_file,
                                      (char **) &forced_extra_defaults,
                                      (char **) &my_defaults_group_suffix,
                                      (char **) &my_login_path, found_no_defaults);

    if (! my_defaults_group_suffix)
      my_defaults_group_suffix= getenv("MYSQL_GROUP_SUFFIX");

    if (forced_extra_defaults && !defaults_already_read)
    {
      int error= fn_expand(forced_extra_defaults,
                           my_defaults_extra_file_buffer);
      if (error)
        DBUG_RETURN(error);

      my_defaults_extra_file= my_defaults_extra_file_buffer;
    }

    if (forced_default_file && !defaults_already_read)
    {
      int error= fn_expand(forced_default_file, my_defaults_file_buffer);
      if (error)
        DBUG_RETURN(error);
      my_defaults_file= my_defaults_file_buffer;
    }

    defaults_already_read= TRUE;

    /*
      We can only handle 'defaults-group-suffix' if we are called from
      load_defaults() as otherwise we can't know the type of 'func_ctx'
    */

    if (my_defaults_group_suffix && func == handle_default_option)
    {
      /* Handle --defaults-group-suffix= */
      uint i;
      const char **extra_groups;
      const size_t instance_len= strlen(my_defaults_group_suffix);
      struct handle_option_ctx *ctx= (struct handle_option_ctx*) func_ctx;
      char *ptr;
      TYPELIB *group= ctx->group;

      if (!(extra_groups=
            (const char**)alloc_root(ctx->alloc,
                                     (2*group->count+1)*sizeof(char*))))
        DBUG_RETURN(2);

      for (i= 0; i < group->count; i++)
      {
        size_t len;
        extra_groups[i]= group->type_names[i];  /** copy group */

        len= strlen(extra_groups[i]);
        if (!(ptr= (char *) alloc_root(ctx->alloc,
                                       (uint) (len + instance_len + 1))))
          DBUG_RETURN(2);

        extra_groups[i+group->count]= ptr;

        /** Construct new group */
        memcpy(ptr, extra_groups[i], len);
        memcpy(ptr+len, my_defaults_group_suffix, instance_len+1);
      }

      group->count*= 2;
      group->type_names= extra_groups;
      group->type_names[group->count]= 0;
    }
  }
  else if (my_login_path && func == handle_default_option)
  {
    /* Handle --login_path= */
    uint i;
    size_t len;
    const char **extra_groups;
    size_t instance_len= 0;
    struct handle_option_ctx *ctx= (struct handle_option_ctx*) func_ctx;
    char *ptr;
    TYPELIB *group= ctx->group;

    if (!(extra_groups= (const char**)alloc_root(ctx->alloc,
                                                 (group->count + 3)
                                                 * sizeof(char *))))
      DBUG_RETURN(2);

    for (i= 0; i < group->count; i++)
    {
      extra_groups[i]= group->type_names[i];    /** copy group */
    }

    extra_groups[i]= my_login_path;

    if (my_defaults_group_suffix && func == handle_default_option)
    {
      instance_len= strlen(my_defaults_group_suffix);
      len= strlen(extra_groups[i]);

      if (!(ptr= (char *) alloc_root(ctx->alloc,
                                     (uint) (len + instance_len + 1))))
        DBUG_RETURN(2);

      extra_groups[i + 1]= ptr;

      /** Construct new group */
      memcpy(ptr, extra_groups[i], len);
      memcpy(ptr+len, my_defaults_group_suffix, instance_len + 1);
      group->count += 1;
    }

    group->count += 1;
    group->type_names= extra_groups;
    group->type_names[group->count]= 0;
  }

  // If conf_file is an absolute path, we only read it
  if (dirname_length(conf_file))
  {
    if ((error= search_default_file(func, func_ctx, NullS, conf_file,
                                    is_login_file)) < 0)
    goto err;
  }
  // If my defaults file is set (from a previous run), we read it
  else if (my_defaults_file)
  {
    if ((error= search_default_file_with_ext(func, func_ctx, "", "",
                                             my_defaults_file, 0,
                                             is_login_file)) < 0)
      goto err;
    if (error > 0)
    {
      my_message_local(ERROR_LEVEL,
                       "Could not open required defaults file: %s",
                       my_defaults_file);
      goto err;
    }
  }
  else if (! found_no_defaults)
  {
    for (dirs= default_directories ; *dirs; dirs++)
    {
      if (**dirs)
      {
       if (search_default_file(func, func_ctx, *dirs, conf_file,
                               is_login_file) < 0)
        goto err;
      }
      else if (my_defaults_extra_file)
      {
        if ((error= search_default_file_with_ext(func, func_ctx, "", "",
                                                 my_defaults_extra_file, 0,
                                                 is_login_file)) < 0)
          goto err;				/* Fatal error */
        if (error > 0)
        {
          my_message_local(ERROR_LEVEL,
                           "Could not open required defaults file: %s",
                           my_defaults_extra_file);
          goto err;
        }
      }
    }
  }

  DBUG_RETURN(0);

err:
  my_message_local(ERROR_LEVEL,
                   "Fatal error in defaults handling. Program aborted!");
  DBUG_RETURN(1);
}


/*
  The option handler for load_defaults.

  SYNOPSIS
    handle_deault_option()
    in_ctx                  Handler context. In this case it is a
                            handle_option_ctx structure.
    group_name              The name of the group the option belongs to.
    option                  The very option to be processed. It is already
                            prepared to be used in argv (has -- prefix). If it
                            is NULL, we are handling a new group (section).

  DESCRIPTION
    This handler checks whether a group is one of the listed and adds an option
    to the array if yes. Some other handler can record, for instance, all
    groups and their options, not knowing in advance the names and amount of
    groups.

  RETURN
    0 - ok
    1 - error occured
*/

static int handle_default_option(void *in_ctx, const char *group_name,
                                 const char *option)
{
  char *tmp;
  struct handle_option_ctx *ctx= (struct handle_option_ctx *) in_ctx;

  if (!option)
    return 0;

  if (find_type((char *)group_name, ctx->group, FIND_TYPE_NO_PREFIX))
  {
    if (!(tmp= (char *) alloc_root(ctx->alloc, strlen(option) + 1)))
      return 1;
    if (ctx->m_args->push_back(tmp))
      return 1;
    my_stpcpy(tmp, option);
  }

  return 0;
}


/*
  Gets options from the command line, however if --no-defaults
  option is used, --defaults-file & --defaults-extra-file options
  would be ignored.

  SYNOPSIS
    get_defaults_options()
    argc			Pointer to argc of original program
    argv			Pointer to argv of original program
    defaults                    --defaults-file option
    extra_defaults              --defaults-extra-file option
    group_suffix                --defaults-group-suffix option
    login_path                  --login-path option

  RETURN
    # Number of arguments used from *argv
      defaults and extra_defaults will be set to option of the appropriate
      items of argv array, or to NULL if there are no such options
*/

int get_defaults_options(int argc, char **argv,
                         char **defaults,
                         char **extra_defaults,
                         char **group_suffix,
                         char **login_path,
                         my_bool found_no_defaults)
{
  int org_argc= argc, prev_argc= 0, default_option_count= 0;
  *defaults= *extra_defaults= *group_suffix= *login_path= 0;

  while (argc >= 2 && argc != prev_argc)
  {
    /* Skip program name or previously handled argument */
    argv++;
    prev_argc= argc;                            /* To check if we found */
    /* --no-defaults is always the first option. */
    if (is_prefix(*argv,"--no-defaults") && ! default_option_count)
    {
       argc--;
       default_option_count ++;
       continue;
    }
    if (!*defaults && is_prefix(*argv, "--defaults-file=") && ! found_no_defaults)
    {
      *defaults= *argv + sizeof("--defaults-file=")-1;
       argc--;
       default_option_count ++;
       continue;
    }
    if (!*extra_defaults && is_prefix(*argv, "--defaults-extra-file=")
        && ! found_no_defaults)
    {
      *extra_defaults= *argv + sizeof("--defaults-extra-file=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
    if (!*group_suffix && is_prefix(*argv, "--defaults-group-suffix="))
    {
      *group_suffix= *argv + sizeof("--defaults-group-suffix=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
    if (!*login_path && is_prefix(*argv, "--login-path="))
    {
      *login_path= *argv + sizeof("--login-path=")-1;
      argc--;
      default_option_count ++;
      continue;
    }
  }
  return org_argc - argc;
}

/*
  Wrapper around my_load_defaults() for interface compatibility.

  SYNOPSIS
    load_defaults()
    conf_file			Basename for configuration file to search for.
    				If this is a path, then only this file is read.
    groups			Which [group] entrys to read.
				Points to an null terminated array of pointers
    argc			Pointer to argc of original program
    argv			Pointer to argv of original program

  NOTES

    This function is NOT thread-safe as it uses a global pointer internally.
    See also notes for my_load_defaults().

  RETURN
    0 ok
    1 The given conf_file didn't exists
*/
int load_defaults(const char *conf_file, const char **groups,
                  int *argc, char ***argv)
{
  return my_load_defaults(conf_file, groups, argc, argv, &default_directories);
}

/** A global to turn off or on reading the mylogin file. On by default */
my_bool my_defaults_read_login_file= TRUE;
/*
  Read options from configurations files

  SYNOPSIS
    my_load_defaults()
    conf_file			Basename for configuration file to search for.
    				If this is a path, then only this file is read.
    groups			Which [group] entrys to read.
				Points to an null terminated array of pointers
    argc			Pointer to argc of original program
    argv			Pointer to argv of original program
    default_directories         Pointer to a location where a pointer to the list
                                of default directories will be stored

  IMPLEMENTATION

   Read options from configuration files and put them BEFORE the arguments
   that are already in argc and argv.  This way the calling program can
   easily command line options override options in configuration files

   NOTES
    In case of fatal error, the function will print a warning and do
    exit(1)
 
    To free used memory one should call free_defaults() with the argument
    that was put in *argv

   RETURN
     - If successful, 0 is returned. If 'default_directories' is not NULL,
     a pointer to the array of default directory paths is stored to a location
     it points to. That stored value must be passed to my_search_option_files()
     later.
     
     - 1 is returned if the given conf_file didn't exist. In this case, the
     value pointed to by default_directories is undefined.
*/


int my_load_defaults(const char *conf_file, const char **groups,
                  int *argc, char ***argv, const char ***default_directories)
{
  My_args my_args(key_memory_defaults);
  TYPELIB group;
  my_bool found_print_defaults= 0;
  uint args_used= 0;
  int error= 0;
  MEM_ROOT alloc;
  char *ptr,**res;
  struct handle_option_ctx ctx;
  const char **dirs;
  char my_login_file[FN_REFLEN];
  my_bool found_no_defaults= false;
  uint args_sep= my_getopt_use_args_separator ? 1 : 0;
  DBUG_ENTER("load_defaults");

  init_alloc_root(key_memory_defaults, &alloc,512,0);
  if ((dirs= init_default_directories(&alloc)) == NULL)
    goto err;
  /*
    Check if the user doesn't want any default option processing
    --no-defaults is always the first option
  */
  if (*argc >= 2 && !strcmp(argv[0][1], "--no-defaults"))
    found_no_defaults= TRUE;

  group.count=0;
  group.name= "defaults";
  group.type_names= groups;

  for (; *groups ; groups++)
    group.count++;

  ctx.alloc= &alloc;
  ctx.m_args= &my_args;
  ctx.group= &group;

  if ((error= my_search_option_files(conf_file, argc, argv,
                                     &args_used, handle_default_option,
                                     (void *) &ctx, dirs, false, found_no_defaults)))
  {
    free_root(&alloc,MYF(0));
    DBUG_RETURN(error);
  }

  if (my_defaults_read_login_file)
  {
    /* Read options from login group. */
    if (my_default_get_login_file(my_login_file, sizeof(my_login_file)) &&
      (error= my_search_option_files(my_login_file, argc, argv, &args_used,
                                     handle_default_option, (void *) &ctx,
                                     dirs, true, found_no_defaults)))
    {
      free_root(&alloc, MYF(0));
      DBUG_RETURN(error);
    }
  }
  /*
    Here error contains <> 0 only if we have a fully specified conf_file
    or a forced default file
  */
  if (!(ptr=(char*)
        alloc_root(&alloc,sizeof(alloc)+
                   (my_args.size() + *argc + 1 + args_sep) *sizeof(char*))))
    goto err;
  res= (char**) (ptr+sizeof(alloc));

  /* copy name + found arguments + command line arguments to new array */
  res[0]= argv[0][0];  /* Name MUST be set, even by embedded library */
  if (!my_args.empty())
    memcpy((res+1), &my_args[0], my_args.size() * sizeof(char*));
  /* Skip --defaults-xxx options */
  (*argc)-= args_used;
  (*argv)+= args_used;

  /*
    Check if we wan't to see the new argument list
    This options must always be the last of the default options
  */
  if (*argc >= 2 && !strcmp(argv[0][1],"--print-defaults"))
  {
    found_print_defaults=1;
    --*argc; ++*argv;				/* skip argument */
  }

  if (my_getopt_use_args_separator)
  {
    /* set arguments separator for arguments from config file and
       command line */
    set_args_separator(&res[my_args.size() + 1]);
  }

  if (*argc)
    memcpy((uchar*) (res + 1 + my_args.size() + args_sep),
           (char*) ((*argv)+1),
	   (*argc-1)*sizeof(char*));
  res[my_args.size() + *argc + args_sep]= 0;  /* last null */

  (*argc)+= my_args.size() + args_sep;
  *argv= res;
  *(MEM_ROOT*) ptr= alloc;			/* Save alloc root for free */

  if (default_directories)
    *default_directories= dirs;

  if (found_no_defaults)
    DBUG_RETURN(0);

  if (found_print_defaults)
  {
    int i;
    printf("%s would have been started with the following arguments:\n",
	   **argv);
    for (i=1 ; i < *argc ; i++)
      if (!my_getopt_is_args_separator((*argv)[i])) /* skip arguments separator */
      {
        if(strncmp((*argv)[i], "--password", 10) == 0)
          printf("%s ", "--password=*****");
        else
          printf("%s ", (*argv)[i]);
      }
    puts("");
    exit(0);
  }

  DBUG_RETURN(0);

 err:
  my_message_local(ERROR_LEVEL,
                   "Fatal error in defaults handling. Program aborted!");
  exit(1);
  return 0;					/* Keep compiler happy */
}


void free_defaults(char **argv)
{
  MEM_ROOT ptr;
  memcpy(&ptr, ((char *) argv) - sizeof(ptr), sizeof(ptr));
  free_root(&ptr,MYF(0));
}


static int search_default_file(Process_option_func opt_handler,
                               void *handler_ctx,
			       const char *dir,
                               const char *config_file,
                               my_bool is_login_file)
{
  char **ext;
  const char *empty_list[]= { "", 0 };
  my_bool have_ext= fn_ext(config_file)[0] != 0;
  const char **exts_to_use= have_ext ? empty_list : f_extensions;

  for (ext= (char**) exts_to_use; *ext; ext++)
  {
    int error;
    if ((error= search_default_file_with_ext(opt_handler, handler_ctx,
                                             dir, *ext,
                                             config_file, 0, is_login_file)) < 0)
      return error;
  }
  return 0;
}


/*
  Skip over keyword and get argument after keyword

  SYNOPSIS
   get_argument()
   keyword		Include directive keyword
   kwlen		Length of keyword
   ptr			Pointer to the keword in the line under process
   line			line number

  RETURN
   0	error
   #	Returns pointer to the argument after the keyword.
*/

static char *get_argument(const char *keyword, size_t kwlen,
                          char *ptr, char *name, uint line)
{
  char *end;

  /* Skip over "include / includedir keyword" and following whitespace */

  for (ptr+= kwlen - 1;
       my_isspace(&my_charset_latin1, ptr[0]);
       ptr++)
  {}

  /*
    Trim trailing whitespace from directory name
    The -1 below is for the newline added by fgets()
    Note that my_isspace() is true for \r and \n
  */
  for (end= ptr + strlen(ptr) - 1;
       my_isspace(&my_charset_latin1, *(end - 1));
       end--)
  {}
  end[0]= 0;                                    /* Cut off end space */

  /* Print error msg if there is nothing after !include* directive */
  if (end <= ptr)
  {
    my_message_local(ERROR_LEVEL,
                     "Wrong '!%s' directive in config file %s at line %d!",
                     keyword, name, line);
    return 0;
  }
  return ptr;
}


/*
  Open a configuration file (if exists) and read given options from it

  SYNOPSIS
    search_default_file_with_ext()
    opt_handler                 Option handler function. It is used to process
                                every separate option.
    handler_ctx                 Pointer to the structure to store actual 
                                parameters of the function.
    dir				directory to read
    ext				Extension for configuration file
    config_file                 Name of configuration file
    group			groups to read
    recursion_level             the level of recursion, got while processing
                                "!include" or "!includedir"
    is_login_file               TRUE, when login file is being processed.

  RETURN
    0   Success
    -1	Fatal error, abort
     1	File not found (Warning)
*/

static int search_default_file_with_ext(Process_option_func opt_handler,
                                        void *handler_ctx,
                                        const char *dir,
                                        const char *ext,
                                        const char *config_file,
                                        int recursion_level,
                                        my_bool is_login_file)
{
  char name[FN_REFLEN + 10], buff[4096], curr_gr[4096], *ptr, *end, **tmp_ext;
  char *value, option[4096+2], tmp[FN_REFLEN];
  static const char includedir_keyword[]= "includedir";
  static const char include_keyword[]= "include";
  const int max_recursion_level= 10;
  MYSQL_FILE *fp;
  uint line=0;
  my_bool found_group=0;
  uint i, rc;
  MY_DIR *search_dir;
  FILEINFO *search_file;

  if ((dir ? strlen(dir) : 0 )+strlen(config_file) >= FN_REFLEN-3)
    return 0;					/* Ignore wrong paths */
  if (dir)
  {
    end=convert_dirname(name, dir, NullS);
    if (dir[0] == FN_HOMELIB)		/* Add . to filenames in home */
      *end++='.';
    strxmov(end,config_file,ext,NullS);
  }
  else
  {
    my_stpcpy(name,config_file);
  }
  fn_format(name,name,"","",4);

  if ((rc= check_file_permissions(name, is_login_file)) < 2)
    return (int) rc;

  if (is_login_file)
  {
    if ( !(fp = mysql_file_fopen(key_file_cnf, name, (O_RDONLY | O_BINARY),
                                 MYF(0))))
      return 1;                                 /* Ignore wrong files. */
  }
  else
  {
    if ( !(fp = mysql_file_fopen(key_file_cnf, name, O_RDONLY, MYF(0))))
      return 1;                                 /* Ignore wrong files */
  }

  while (mysql_file_getline(buff, sizeof(buff) - 1, fp, is_login_file))
  {
    line++;
    /* Ignore comment and empty lines */
    for (ptr= buff; my_isspace(&my_charset_latin1, *ptr); ptr++)
    {}

    if (*ptr == '#' || *ptr == ';' || !*ptr)
      continue;

    /* Configuration File Directives */
    if (*ptr == '!')
    {
      if (recursion_level >= max_recursion_level)
      {
        for (end= ptr + strlen(ptr) - 1; 
             my_isspace(&my_charset_latin1, *(end - 1));
             end--)
        {}
        end[0]= 0;
        my_message_local(WARNING_LEVEL,
                         "skipping '%s' directive as maximum include"
                         "recursion level was reached in file %s at line %d!",
                         ptr, name, line);
        continue;
      }

      /* skip over `!' and following whitespace */
      for (++ptr; my_isspace(&my_charset_latin1, ptr[0]); ptr++)
      {}

      if ((!strncmp(ptr, includedir_keyword,
                    sizeof(includedir_keyword) - 1)) &&
          my_isspace(&my_charset_latin1, ptr[sizeof(includedir_keyword) - 1]))
      {
	if (!(ptr= get_argument(includedir_keyword,
                                sizeof(includedir_keyword),
                                ptr, name, line)))
	  goto err;

        if (!(search_dir= my_dir(ptr, MYF(MY_WME))))
          goto err;

        for (i= 0; i < search_dir->number_off_files; i++)
        {
          search_file= search_dir->dir_entry + i;
          ext= fn_ext(search_file->name);

          /* check extension */
          for (tmp_ext= (char**) f_extensions; *tmp_ext; tmp_ext++)
          {
            if (!strcmp(ext, *tmp_ext))
              break;
          }

          if (*tmp_ext)
          {
            fn_format(tmp, search_file->name, ptr, "",
                      MY_UNPACK_FILENAME | MY_SAFE_PATH);

            search_default_file_with_ext(opt_handler, handler_ctx, "", "", tmp,
                                         recursion_level + 1, is_login_file);
          }
        }

        my_dirend(search_dir);
      }
      else if ((!strncmp(ptr, include_keyword, sizeof(include_keyword) - 1)) &&
               my_isspace(&my_charset_latin1, ptr[sizeof(include_keyword)-1]))
      {
	if (!(ptr= get_argument(include_keyword,
                                sizeof(include_keyword), ptr,
                                name, line)))
	  goto err;

        search_default_file_with_ext(opt_handler, handler_ctx, "", "", ptr,
                                     recursion_level + 1, is_login_file);
      }

      continue;
    }

    if (*ptr == '[')				/* Group name */
    {
      found_group=1;
      if (!(end= strchr(++ptr,']')))
      {
        my_message_local(ERROR_LEVEL,
                         "Wrong group definition in config file %s at line %d!",
                         name,line);
        goto err;
      }
      /* Remove end space */
      for ( ; my_isspace(&my_charset_latin1, end[-1]); end --)
      {}

      end[0]=0;

      strmake(curr_gr, ptr, MY_MIN((size_t) (end-ptr)+1, sizeof(curr_gr)-1));

      /* signal that a new group is found */
      opt_handler(handler_ctx, curr_gr, NULL);

      continue;
    }
    if (!found_group)
    {
      my_message_local(ERROR_LEVEL,
                       "Found option without preceding group in config file"
                       " %s at line %d!", name,line);
      goto err;
    }


    end= remove_end_comment(ptr);
    if ((value= strchr(ptr, '=')))
      end= value;				/* Option without argument */
    for ( ; my_isspace(&my_charset_latin1, end[-1]) ; end--)
    {}

    if (!value)
    {
      strmake(my_stpcpy(option,"--"),ptr, (size_t) (end-ptr));
      if (opt_handler(handler_ctx, curr_gr, option))
        goto err;
    }
    else
    {
      /* Remove pre- and end space */
      char *value_end;
      for (value++ ; my_isspace(&my_charset_latin1, *value); value ++)
      {}

      value_end=strend(value);
      /*
	We don't have to test for value_end >= value as we know there is
	an '=' before
      */
      for ( ; my_isspace(&my_charset_latin1, value_end[-1]); value_end --)
      {}

      if (value_end < value)			/* Empty string */
	value_end=value;

      /* remove quotes around argument */
      if ((*value == '\"' || *value == '\'') && /* First char is quote */
          (value + 1 < value_end ) && /* String is longer than 1 */
          *value == value_end[-1] ) /* First char is equal to last char */
      {
	value++;
	value_end--;
      }
      ptr=my_stpnmov(my_stpcpy(option,"--"),ptr,(size_t) (end-ptr));
      *ptr++= '=';

      for ( ; value != value_end; value++)
      {
	if (*value == '\\' && value != value_end-1)
	{
	  switch(*++value) {
	  case 'n':
	    *ptr++='\n';
	    break;
	  case 't':
	    *ptr++= '\t';
	    break;
	  case 'r':
	    *ptr++ = '\r';
	    break;
	  case 'b':
	    *ptr++ = '\b';
	    break;
	  case 's':
	    *ptr++= ' ';			/* space */
	    break;
	  case '\"':
	    *ptr++= '\"';
	    break;
	  case '\'':
	    *ptr++= '\'';
	    break;
	  case '\\':
	    *ptr++= '\\';
	    break;
	  default:				/* Unknown; Keep '\' */
	    *ptr++= '\\';
	    *ptr++= *value;
	    break;
	  }
	}
	else
	  *ptr++= *value;
      }
      *ptr=0;
      if (opt_handler(handler_ctx, curr_gr, option))
        goto err;
    }
  }
  mysql_file_fclose(fp, MYF(0));
  return(0);

 err:
  mysql_file_fclose(fp, MYF(0));
  return -1;					/* Fatal error */
}


static char *remove_end_comment(char *ptr)
{
  char quote= 0;	/* we are inside quote marks */
  char escape= 0;	/* symbol is protected by escape chagacter */

  for (; *ptr; ptr++)
  {
    if ((*ptr == '\'' || *ptr == '\"') && !escape)
    {
      if (!quote)
	quote= *ptr;
      else if (quote == *ptr)
	quote= 0;
    }
    /* We are not inside a string */
    if (!quote && *ptr == '#')
    {
      *ptr= 0;
      return ptr;
    }
    escape= (quote && *ptr == '\\' && !escape);
  }
  return ptr;
}


/**
  Read one line from the specified file. In case
  of scrambled login file, the line read is first
  decrypted and then returned.

  @param str           [out]  Buffer to store the read text.
  @param size          [in]   At max, size-1 bytes to be read.
  @param file          [in]   Source file.
  @param is_login_file [in]   TRUE, when login file is being processed.

  @return 1               Success
          0               Error
*/

static my_bool mysql_file_getline(char *str, int size, MYSQL_FILE *file,
                                  my_bool is_login_file)
{
  uchar cipher[4096], len_buf[MAX_CIPHER_STORE_LEN];
  static unsigned char my_key[LOGIN_KEY_LEN];
  int length= 0, cipher_len= 0;

  if (is_login_file)
  {
    if (mysql_file_ftell(file, MYF(MY_WME)) == 0)
    {
      /* Move past unused bytes. */
      mysql_file_fseek(file, 4, SEEK_SET, MYF(MY_WME));
      if (mysql_file_fread(file, my_key, LOGIN_KEY_LEN,
                           MYF(MY_WME)) != LOGIN_KEY_LEN)
        return 0;
    }

    if (mysql_file_fread(file, len_buf, MAX_CIPHER_STORE_LEN,
                         MYF(MY_WME)) == MAX_CIPHER_STORE_LEN)
    {
      cipher_len= sint4korr(len_buf);
      if (cipher_len > size)
        return 0;
    }
    else
      return 0;

    mysql_file_fread(file, cipher, cipher_len, MYF(MY_WME));
    if ((length= my_aes_decrypt(cipher, cipher_len, (unsigned char *) str,
                                my_key, LOGIN_KEY_LEN, my_aes_128_ecb, NULL)) < 0)
    {
      /* Attempt to decrypt failed. */
      return 0;
    }
    str[length]= 0;
    return 1;
  }
  else
  {
    if (mysql_file_fgets(str, size, file))
      return 1;
    else
      return 0;
  }
}


void my_print_default_files(const char *conf_file)
{
  const char *empty_list[]= { "", 0 };
  my_bool have_ext= fn_ext(conf_file)[0] != 0;
  const char **exts_to_use= have_ext ? empty_list : f_extensions;
  char name[FN_REFLEN], **ext;

  puts("\nDefault options are read from the following files in the given order:");

  if (dirname_length(conf_file))
    fputs(conf_file,stdout);
  else
  {
    const char **dirs;
    MEM_ROOT alloc;
    init_alloc_root(key_memory_defaults, &alloc, 512, 0);

    if ((dirs= init_default_directories(&alloc)) == NULL)
    {
      fputs("Internal error initializing default directories list", stdout);
    }
    else
    {
      for ( ; *dirs; dirs++)
      {
        for (ext= (char**) exts_to_use; *ext; ext++)
        {
          const char *pos;
          char *end;
          if (**dirs)
            pos= *dirs;
          else if (my_defaults_extra_file)
            pos= my_defaults_extra_file;
          else
            continue;
          end= convert_dirname(name, pos, NullS);
          if (name[0] == FN_HOMELIB)	/* Add . to filenames in home */
            *end++= '.';

          if (my_defaults_extra_file == pos)
            end[(strlen(end)-1)] = ' ';
          else
            strxmov(end, conf_file, *ext , " ",  NullS);
          fputs(name, stdout);
        }
      }
    }

    free_root(&alloc, MYF(0));
  }
  puts("");
}

void print_defaults(const char *conf_file, const char **groups)
{
  const char **groups_save= groups;
  my_print_default_files(conf_file);

  fputs("The following groups are read:",stdout);
  for ( ; *groups ; groups++)
  {
    fputc(' ',stdout);
    fputs(*groups,stdout);
  }

  if (my_defaults_group_suffix)
  {
    groups= groups_save;
    for ( ; *groups ; groups++)
    {
      fputc(' ',stdout);
      fputs(*groups,stdout);
      fputs(my_defaults_group_suffix,stdout);
    }
  }
  puts("\nThe following options may be given as the first argument:\n\
--print-defaults        Print the program argument list and exit.\n\
--no-defaults           Don't read default options from any option file,\n\
                        except for login file.\n\
--defaults-file=#       Only read default options from the given file #.\n\
--defaults-extra-file=# Read this file after the global files are read.\n\
--defaults-group-suffix=#\n\
                        Also read groups with concat(group, suffix)\n\
--login-path=#          Read this path from the login file.");
}


static int add_directory(MEM_ROOT *alloc, const char *dir, const char **dirs)
{
  char buf[FN_REFLEN];
  size_t len;
  char *p;
  my_bool err MY_ATTRIBUTE((unused));

  len= normalize_dirname(buf, dir);
  if (!(p= strmake_root(alloc, buf, len)))
    return 1;  /* Failure */
  /* Should never fail if DEFAULT_DIRS_SIZE is correct size */
  err= array_append_string_unique(p, dirs, DEFAULT_DIRS_SIZE);
  DBUG_ASSERT(err == FALSE);

  return 0;
}


#ifdef _WIN32
/*
  This wrapper for GetSystemWindowsDirectory() will dynamically bind to the
  function if it is available, emulate it on NT4 Terminal Server by stripping
  the \SYSTEM32 from the end of the results of GetSystemDirectory(), or just
  return GetSystemDirectory().
 */

typedef UINT (WINAPI *GET_SYSTEM_WINDOWS_DIRECTORY)(LPSTR, UINT);

static size_t my_get_system_windows_directory(char *buffer, size_t size)
{
  size_t count;
  GET_SYSTEM_WINDOWS_DIRECTORY
    func_ptr= (GET_SYSTEM_WINDOWS_DIRECTORY)
              GetProcAddress(GetModuleHandle("kernel32.dll"),
                                             "GetSystemWindowsDirectoryA");

  if (func_ptr)
    return func_ptr(buffer, (uint) size);

  /*
    Windows NT 4.0 Terminal Server Edition:  
    To retrieve the shared Windows directory, call GetSystemDirectory and
    trim the "System32" element from the end of the returned path.
  */
  count= GetSystemDirectory(buffer, (uint) size);
  if (count > 8 && stricmp(buffer+(count-8), "\\System32") == 0)
  {
    count-= 8;
    buffer[count] = '\0';
  }
  return count;
}


static const char *my_get_module_parent(char *buf, size_t size)
{
  char *last= NULL;
  char *end;
  if (!GetModuleFileName(NULL, buf, (DWORD) size))
    return NULL;
  end= strend(buf);

  /*
    Look for the second-to-last \ in the filename, but hang on
    to a pointer after the last \ in case we're in the root of
    a drive.
  */
  for ( ; end > buf; end--)
  {
    if (*end == FN_LIBCHAR)
    {
      if (last)
      {
        /* Keep the last '\' as this works both with D:\ and a directory */
        end[1]= 0;
        break;
      }
      last= end;
    }
  }

  return buf;
}
#endif /* _WIN32 */


static const char **init_default_directories(MEM_ROOT *alloc)
{
  const char **dirs;
  char *env;
  int errors= 0;

  dirs= (const char **)alloc_root(alloc, DEFAULT_DIRS_SIZE * sizeof(char *));
  if (dirs == NULL)
    return NULL;
  memset(dirs, 0, DEFAULT_DIRS_SIZE * sizeof(char *));

#ifdef _WIN32

  {
    char fname_buffer[FN_REFLEN];
    if (my_get_system_windows_directory(fname_buffer, sizeof(fname_buffer)))
      errors += add_directory(alloc, fname_buffer, dirs);

    if (GetWindowsDirectory(fname_buffer, sizeof(fname_buffer)))
      errors += add_directory(alloc, fname_buffer, dirs);

    errors += add_directory(alloc, "C:/", dirs);

    if (my_get_module_parent(fname_buffer, sizeof(fname_buffer)) != NULL)
      errors += add_directory(alloc, fname_buffer, dirs);
  }

#else

  errors += add_directory(alloc, "/etc/", dirs);
  errors += add_directory(alloc, "/etc/mysql/", dirs);

#if defined(DEFAULT_SYSCONFDIR)
  if (DEFAULT_SYSCONFDIR[0])
    errors += add_directory(alloc, DEFAULT_SYSCONFDIR, dirs);
#endif /* DEFAULT_SYSCONFDIR */

#endif

  if ((env= getenv("MYSQL_HOME")))
    errors += add_directory(alloc, env, dirs);

  /* Placeholder for --defaults-extra-file=<path> */
  errors += add_directory(alloc, "", dirs);

#if !defined(_WIN32)
  errors += add_directory(alloc, "~/", dirs);
#endif

  return (errors > 0 ? NULL : dirs);
}

/**
  Place the login file name in the specified buffer.

  @param file_name     [out]  Buffer to hold login file name
  @param file_name_size [in]  Length of the buffer

  @return 1 - Success
          0 - Failure
*/

int my_default_get_login_file(char *file_name, size_t file_name_size)
{
  size_t rc;

  if (getenv("MYSQL_TEST_LOGIN_FILE"))
    rc= my_snprintf(file_name, file_name_size, "%s",
                    getenv("MYSQL_TEST_LOGIN_FILE"));
#ifdef _WIN32
  else if (getenv("APPDATA"))
    rc= my_snprintf(file_name, file_name_size, "%s\\MySQL\\.mylogin.cnf",
                    getenv("APPDATA"));
#else
  else if (getenv("HOME"))
    rc= my_snprintf(file_name, file_name_size, "%s/.mylogin.cnf",
                    getenv("HOME"));
#endif
  else
  {
    memset(file_name, 0, file_name_size);
    return 0;
  }
  /* Anything <= 0 will be treated as error. */
  if (rc <= 0)
    return 0;

  return 1;
}

/**
  Check file permissions of the option file.

  @param file_name     [in]   Name of the option file.
  @param is_login_file [in]   TRUE, when login file is being processed.

  @return  0 - Non-allowable file permissions.
           1 - Failed to stat.
           2 - Success.
*/
int check_file_permissions(const char *file_name, my_bool is_login_file)
{
#if !defined(_WIN32)
  MY_STAT stat_info;

  if (!my_stat(file_name,&stat_info,MYF(0)))
    return 1;
  /*
    Ignore .mylogin.cnf file if not exclusively readable/writable
    by current user.
  */
  if (is_login_file && (stat_info.st_mode & (S_IXUSR | S_IRWXG | S_IRWXO))
      && (stat_info.st_mode & S_IFMT) == S_IFREG)
  {
    my_message_local(WARNING_LEVEL, "%s should be readable/writable only by "
            "current user.", file_name);
    return 0;
  }
  /*
    Ignore world-writable regular files.
    This is mainly done to protect us to not read a file created by
    the mysqld server, but the check is still valid in most context.
  */
  else if ((stat_info.st_mode & S_IWOTH) &&
           (stat_info.st_mode & S_IFMT) == S_IFREG)

  {
    my_message_local(WARNING_LEVEL,
                     "World-writable config file '%s' is ignored.", file_name);
    return 0;
  }
#endif
  return 2;                                     /* Success */
}

