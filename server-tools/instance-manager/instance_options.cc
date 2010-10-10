/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "instance_options.h"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

#include <signal.h>

#include "buffer.h"
#include "instance.h"
#include "log.h"
#include "options.h"
#include "parse_output.h"
#include "priv.h"


/* Create "mysqld ..." command in the buffer */

static inline bool create_mysqld_command(Buffer *buf,
                                        const LEX_STRING *mysqld_path,
                                        const LEX_STRING *option)
{
  int position= 0;

  if (buf->get_size()) /* malloc succeeded */
  {
#ifdef __WIN__
    buf->append(position++, "\"", 1);
#endif
    buf->append(position, mysqld_path->str, mysqld_path->length);
    position+= mysqld_path->length;
#ifdef __WIN__
    buf->append(position++, "\"", 1);
#endif
    /* here the '\0' character is copied from the option string */
    buf->append(position, option->str, option->length + 1);

    return buf->is_error() ? TRUE : FALSE;
  }
  return TRUE;
}

static inline bool is_path_separator(char ch)
{
#if defined(__WIN__) || defined(__NETWARE__)
  /* On windows and netware more delimiters are possible */
  return ch == FN_LIBCHAR || ch == FN_DEVCHAR || ch == '/';
#else
  return ch == FN_LIBCHAR;                      /* Unixes */
#endif
}


static char *find_last_path_separator(char *path, uint length)
{
  while (length)
  {
    if (is_path_separator(path[length]))
      return path + length;
    length--;
  }
  return NULL; /* No path separator found */
}



bool Instance_options::is_option_im_specific(const char *option_name)
{
  static const char *IM_SPECIFIC_OPTIONS[] =
  {
    "nonguarded",
    "mysqld-path",
    "shutdown-delay",
    NULL
  };

  for (int i= 0; IM_SPECIFIC_OPTIONS[i]; ++i)
  {
    if (!strcmp(option_name, IM_SPECIFIC_OPTIONS[i]))
      return TRUE;
  }

  return FALSE;
}


Instance_options::Instance_options()
  :mysqld_version(NULL), mysqld_socket(NULL), mysqld_datadir(NULL),
  mysqld_pid_file(NULL), 
  nonguarded(NULL),
  mysqld_port(NULL), 
  mysqld_port_val(0),
  shutdown_delay(NULL),
  shutdown_delay_val(0),
  filled_default_options(0)
{
  mysqld_path.str= NULL;
  mysqld_path.length= 0;

  mysqld_real_path.str= NULL;
  mysqld_real_path.length= 0;

  memset(logs, 0, sizeof(logs));
}


/*
  Get compiled-in value of default_option

  SYNOPSIS
    get_default_option()
    result            buffer to put found value
    result_len        buffer size
    option_name       the name of the option, prefixed with "--"

  DESCRIPTION

   Get compile-in value of requested option from server

  RETURN
    0 - ok
    1 - error occured
*/


int Instance_options::get_default_option(char *result, size_t result_len,
                                         const char *option_name)
{
  int rc= 1;
  LEX_STRING verbose_option=
    { C_STRING_WITH_LEN(" --no-defaults --verbose --help") };

  /* reserve space for the path + option + final '\0' */
  Buffer cmd(mysqld_path.length + verbose_option.length + 1);

  if (create_mysqld_command(&cmd, &mysqld_path, &verbose_option))
    goto err;

  /* +2 eats first "--" from the option string (E.g. "--datadir") */
  rc= parse_output_and_get_value((char*) cmd.buffer,
                                 option_name + 2, strlen(option_name + 2),
                                 result, result_len, GET_VALUE);
err:
  return rc;
}


/*
  Fill mysqld_version option (used at initialization stage)

  SYNOPSIS
    fill_instance_version()

  DESCRIPTION

  Get mysqld version string from "mysqld --version" output.

  RETURN
    FALSE - ok
    TRUE  - error occured
*/

bool Instance_options::fill_instance_version()
{
  char result[MAX_VERSION_LENGTH];
  LEX_STRING version_option=
    { C_STRING_WITH_LEN(" --no-defaults --version") };
  Buffer cmd(mysqld_path.length + version_option.length + 1);

  if (create_mysqld_command(&cmd, &mysqld_path, &version_option))
  {
    log_error("Failed to get version of '%s': out of memory.",
              (const char *) mysqld_path.str);
    return TRUE;
  }

  bzero(result, MAX_VERSION_LENGTH);

  if (parse_output_and_get_value((char*) cmd.buffer, STRING_WITH_LEN("Ver"),
                                 result, MAX_VERSION_LENGTH, GET_LINE))
  {
    log_error("Failed to get version of '%s': unexpected output.",
              (const char *) mysqld_path.str);
    return TRUE;
  }

  DBUG_ASSERT(*result != '\0');

  {
    char *start;

    /* trim leading whitespaces */
    start= result;
    while (my_isspace(default_charset_info, *start))
      ++start;

    mysqld_version= strdup_root(&alloc, start);
  }

  return FALSE;
}


/*
  Fill mysqld_real_path

  SYNOPSIS
    fill_mysqld_real_path()

  DESCRIPTION

  Get the real path to mysqld from "mysqld --help" output.
  Will print the realpath of mysqld between "Usage: " and "[OPTIONS]"

  This is needed if the mysqld_path variable is pointing at a
  script(for example libtool) or a symlink.

  RETURN
    FALSE - ok
    TRUE  - error occured
*/

bool Instance_options::fill_mysqld_real_path()
{
  char result[FN_REFLEN];
  LEX_STRING help_option=
    { C_STRING_WITH_LEN(" --no-defaults --help") };
  Buffer cmd(mysqld_path.length + help_option.length);

  if (create_mysqld_command(&cmd, &mysqld_path, &help_option))
  {
    log_error("Failed to get real path of '%s': out of memory.",
              (const char *) mysqld_path.str);
    return TRUE;
  }

  bzero(result, FN_REFLEN);

  if (parse_output_and_get_value((char*) cmd.buffer,
                                 STRING_WITH_LEN("Usage: "),
                                 result, FN_REFLEN,
                                 GET_LINE))
  {
    log_error("Failed to get real path of '%s': unexpected output.",
              (const char *) mysqld_path.str);
    return TRUE;
  }

  DBUG_ASSERT(*result != '\0');

  {
    char* options_str;
    /* chop the path of at [OPTIONS] */
    if ((options_str= strstr(result, "[OPTIONS]")))
      *options_str= '\0';
    mysqld_real_path.str= strdup_root(&alloc, result);
    mysqld_real_path.length= strlen(mysqld_real_path.str);
  }

  return FALSE;
}


/*
  Fill various log options

  SYNOPSIS
    fill_log_options()

  DESCRIPTION

  Compute paths to enabled log files. If the path is not specified in the
  instance explicitly (I.e. log=/home/user/mysql.log), we try to guess the
  file name and placement.

  RETURN
    FALSE - ok
    TRUE  - error occured
*/

bool Instance_options::fill_log_options()
{
  Buffer buff;
  enum { MAX_LOG_OPTION_LENGTH= 256 };
  char datadir[MAX_LOG_OPTION_LENGTH];
  char hostname[MAX_LOG_OPTION_LENGTH];
  struct log_files_st
  {
    const char *name;
    uint length;
    char **value;
    const char *default_suffix;
  } logs_st[]=
  {
    {"--log-error", 11, &(logs[IM_LOG_ERROR]), ".err"},
    {"--log", 5, &(logs[IM_LOG_GENERAL]), ".log"},
    {"--log-slow-queries", 18, &(logs[IM_LOG_SLOW]), "-slow.log"},
    {NULL, 0, NULL, NULL}
  };
  struct log_files_st *log_files;

  /* compute hostname and datadir for the instance */
  if (mysqld_datadir == NULL)
  {
    if (get_default_option(datadir, MAX_LOG_OPTION_LENGTH, "--datadir"))
      return TRUE;
  }
  else
  {
    /* below is safe, as --datadir always has a value */
    strmake(datadir, mysqld_datadir, MAX_LOG_OPTION_LENGTH - 1);
  }

  if (gethostname(hostname,sizeof(hostname)-1) < 0)
    strmov(hostname, "mysql");

  hostname[MAX_LOG_OPTION_LENGTH - 1]= 0; /* Safety */

  for (log_files= logs_st; log_files->name; log_files++)
  {
    for (int i=0; (argv[i] != 0); i++)
    {
      if (!strncmp(argv[i], log_files->name, log_files->length))
      {
        /*
          This is really log_files->name option if and only if it is followed
          by '=', '\0' or space character. This way we can distinguish such
          options as '--log' and '--log-bin'. This is checked in the following
          two statements.
        */
        if (argv[i][log_files->length] == '\0' ||
            my_isspace(default_charset_info, argv[i][log_files->length]))
        {
          char full_name[MAX_LOG_OPTION_LENGTH];

          fn_format(full_name, hostname, datadir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH);


          if ((MAX_LOG_OPTION_LENGTH - strlen(full_name)) <=
              strlen(log_files->default_suffix))
            return TRUE;

          strmov(full_name + strlen(full_name), log_files->default_suffix);

          /*
            If there were specified two identical logfiles options,
            we would loose some memory in MEM_ROOT here. However
            this situation is not typical.
          */
          *(log_files->value)= strdup_root(&alloc, full_name);
        }

        if (argv[i][log_files->length] == '=')
        {
          char full_name[MAX_LOG_OPTION_LENGTH];

          fn_format(full_name, argv[i] +log_files->length + 1,
                    datadir, "", MY_UNPACK_FILENAME | MY_SAFE_PATH);

          if (!(*(log_files->value)= strdup_root(&alloc, full_name)))
            return TRUE;
        }
      }
    }
  }

  return FALSE;
}


/*
  Get the full pid file name with path

  SYNOPSIS
    get_pid_filaname()
    result            buffer to sotre the pidfile value

  IMPLEMENTATION
    Get the data directory, then get the pid filename
    (which is always set for an instance), then load the
    full path with my_load_path(). It takes into account
    whether it is already an absolute path or it should be
    prefixed with the datadir and so on.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::get_pid_filename(char *result)
{
  char datadir[MAX_PATH_LEN];

  if (mysqld_datadir == NULL)
  {
    /* we might get an error here if we have wrong path to the mysqld binary */
    if (get_default_option(datadir, sizeof(datadir), "--datadir"))
      return 1;
  }
  else
    strxnmov(datadir, MAX_PATH_LEN - 1, mysqld_datadir, "/", NullS);

  /* get the full path to the pidfile */
  my_load_path(result, mysqld_pid_file, datadir);
  return 0;
}


int Instance_options::unlink_pidfile()
{
  return unlink(pid_file_with_path);
}


pid_t Instance_options::load_pid()
{
  FILE *pid_file_stream;

  /* get the pid */
  if ((pid_file_stream= my_fopen(pid_file_with_path,
                                O_RDONLY | O_BINARY, MYF(0))) != NULL)
  {
    pid_t pid;

    if (fscanf(pid_file_stream, "%i", &pid) != 1)
      pid= -1;
    my_fclose(pid_file_stream, MYF(0));
    return pid;
  }
  return 0;
}


bool Instance_options::complete_initialization()
{
  int arg_idx;
  const char *tmp;
  char *end;
  char bin_name_firstchar;

  if (!mysqld_path.str)
  {
    /*
      Need to copy the path to allocated memory, as convert_dirname() might
      need to change it
    */
    mysqld_path.str= strdup_root(&alloc, Options::Main::default_mysqld_path);
    if (!mysqld_path.str)
      return TRUE;
  }

  mysqld_path.length= strlen(mysqld_path.str);

  /*
    If we found path with no slashes (end == NULL), we should not call
    convert_dirname() at all. As we have got relative path to the binary.
    That is, user supposes that mysqld resides in the same dir as
    mysqlmanager.
  */
  if ((end= find_last_path_separator(mysqld_path.str, mysqld_path.length)))
  {
    bin_name_firstchar= end[1];

    /*
      Below we will conver the path to mysqld in the case, it was given
      in a format of another OS (e.g. uses '/' instead of '\' etc).
      Here we strip the path to get rid of the binary name ("mysqld"),
      we do it by removing first letter of the binary name (e.g. 'm'
      in "mysqld"). Later we put it back.
    */
    end[1]= 0;

    /* convert dirname to the format of current OS */
    convert_dirname((char*)mysqld_path.str, mysqld_path.str, NullS);

    /* put back the first character of the binary name*/
    end[1]= bin_name_firstchar;
  }

  if (mysqld_port)
    mysqld_port_val= atoi(mysqld_port);

  if (shutdown_delay)
    shutdown_delay_val= atoi(shutdown_delay);

  if (!(tmp= strdup_root(&alloc, "--no-defaults")))
    return TRUE;

  if (!mysqld_pid_file)
  {
    char pidfilename[MAX_PATH_LEN];
    char hostname[MAX_PATH_LEN];

    /*
      If we created only one istance [mysqld], because no config. files were
      found, we would like to model mysqld pid file values.
    */

    if (!gethostname(hostname, sizeof(hostname) - 1))
    {
      if (Instance::is_mysqld_compatible_name(&instance_name))
        strxnmov(pidfilename, MAX_PATH_LEN - 1, hostname, ".pid", NullS);
      else
        strxnmov(pidfilename, MAX_PATH_LEN - 1, instance_name.str, "-",
                 hostname, ".pid", NullS);
    }
    else
    {
      if (Instance::is_mysqld_compatible_name(&instance_name))
        strxnmov(pidfilename, MAX_PATH_LEN - 1, "mysql", ".pid", NullS);
      else
        strxnmov(pidfilename, MAX_PATH_LEN - 1, instance_name.str, ".pid",
                 NullS);
    }

    Named_value option((char *) "pid-file", pidfilename);

    set_option(&option);
  }

  if (get_pid_filename(pid_file_with_path))
    return TRUE;

  /* we need to reserve space for the final zero + possible default options */
  if (!(argv= (char**)
        alloc_root(&alloc, (get_num_options() + 1
                            + MAX_NUMBER_OF_DEFAULT_OPTIONS) * sizeof(char*))))
    return TRUE;
  filled_default_options= 0;

  /* the path must be first in the argv */
  if (add_to_argv(mysqld_path.str))
    return TRUE;

  if (add_to_argv(tmp))
    return TRUE;

  arg_idx= filled_default_options;
  for (int opt_idx= 0; opt_idx < get_num_options(); ++opt_idx)
  {
    char option_str[MAX_OPTION_STR_LEN];
    Named_value option= get_option(opt_idx);

    if (is_option_im_specific(option.get_name()))
      continue;

    char *ptr= strxnmov(option_str, MAX_OPTION_LEN + 3, "--", option.get_name(),
                        NullS);

    if (option.get_value()[0])
      strxnmov(ptr, MAX_OPTION_LEN + 2, "=", option.get_value(), NullS);

    argv[arg_idx++]= strdup_root(&alloc, option_str);
  }

  argv[arg_idx]= 0;

  if (fill_log_options() || fill_mysqld_real_path() || fill_instance_version())
    return TRUE;

  return FALSE;
}


bool Instance_options::set_option(Named_value *option)
{
  bool err_status;
  int idx= find_option(option->get_name());
  char *option_name_str;
  char *option_value_str;

  if (!(option_name_str= Named_value::alloc_str(option->get_name())))
    return TRUE;

  if (!(option_value_str= Named_value::alloc_str(option->get_value())))
  {
    Named_value::free_str(&option_name_str);
    return TRUE;
  }

  Named_value option_copy(option_name_str, option_value_str);

  if (idx < 0)
    err_status= options.add_element(&option_copy);
  else
    err_status= options.replace_element(idx, &option_copy);

  if (!err_status)
    update_var(option_copy.get_name(), option_copy.get_value());
  else
    option_copy.free();

  return err_status;
}


void Instance_options::unset_option(const char *option_name)
{
  int idx= find_option(option_name);

  if (idx < 0)
    return; /* the option has not been set. */

  options.remove_element(idx);

  update_var(option_name, NULL);
}


void Instance_options::update_var(const char *option_name,
                                  const char *option_value)
{
  struct options_st
  {
    const char *name;
    uint name_len;
    const char **var;
  } options_def[]=
  {
    {"socket",          6,  &mysqld_socket},
    {"port",            4,  &mysqld_port},
    {"datadir",         7,  &mysqld_datadir},
    {"pid-file",        8,  &mysqld_pid_file},
    {"nonguarded",      10, &nonguarded},
    {"mysqld-path",     11, (const char **) &mysqld_path.str},
    {"shutdown-delay",  14, &shutdown_delay},
    {NULL, 0, NULL}
  };

  for (options_st *opt= options_def; opt->name; ++opt)
  {
    if (!strncmp(opt->name, option_name, opt->name_len))
    {
      *(opt->var)= option_value;
      break;
    }
  }
}


int Instance_options::find_option(const char *option_name)
{
  for (int i= 0; i < get_num_options(); i++)
  {
    if (!strcmp(get_option(i).get_name(), option_name))
      return i;
  }

  return -1;
}


int Instance_options::add_to_argv(const char* option)
{
  DBUG_ASSERT(filled_default_options < MAX_NUMBER_OF_DEFAULT_OPTIONS);

  if (option)
    argv[filled_default_options++]= (char*) option;
  return 0;
}


/* function for debug purposes */
void Instance_options::print_argv()
{
  int i;

  printf("printing out an instance %s argv:\n",
         (const char *) instance_name.str);

  for (i=0; argv[i] != NULL; i++)
    printf("argv: %s\n", argv[i]);
}


/*
  We execute this function to initialize some options.

  RETURN
    FALSE - ok
    TRUE  - memory allocation error
*/

bool Instance_options::init(const LEX_STRING *instance_name_arg)
{
  instance_name.length= instance_name_arg->length;

  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);

  if (options.init())
    return TRUE;

  if (!(instance_name.str= strmake_root(&alloc, instance_name_arg->str,
                                        instance_name_arg->length)))
    return TRUE;

  return FALSE;
}


Instance_options::~Instance_options()
{
  free_root(&alloc, MYF(0));
}


uint Instance_options::get_shutdown_delay() const
{
  static const uint DEFAULT_SHUTDOWN_DELAY= 35;

  /*
    NOTE: it is important to check shutdown_delay here, but use
    shutdown_delay_val. The idea is that if the option is unset,
    shutdown_delay will be NULL, but shutdown_delay_val will not be reset.
  */

  return shutdown_delay ? shutdown_delay_val : DEFAULT_SHUTDOWN_DELAY;
}

int Instance_options::get_mysqld_port() const
{
  /*
    NOTE: it is important to check mysqld_port here, but use mysqld_port_val.
    The idea is that if the option is unset, mysqld_port will be NULL, but
    mysqld_port_val will not be reset.
  */

  return mysqld_port ? mysqld_port_val : 0;
}

