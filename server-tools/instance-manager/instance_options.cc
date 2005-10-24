/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

#include "parse_output.h"
#include "buffer.h"

#include <my_sys.h>
#include <signal.h>
#include <m_string.h>

#ifdef __WIN__
#define NEWLINE_LEN 2
#else
#define NEWLINE_LEN 1
#endif


/* Create "mysqld ..." command in the buffer */

static inline int create_mysqld_command(Buffer *buf,
                                        const char *mysqld_path_str,
                                        uint mysqld_path_len,
                                        const char *option,
                                        uint option_len)
{
  int position= 0;

  if (buf->get_size()) /* malloc succeeded */
  {
#ifdef __WIN__
    buf->append(position, "\"", 1);
    position++;
#endif
    buf->append(position, mysqld_path_str, mysqld_path_len);
    position+= mysqld_path_len;
#ifdef __WIN__
    buf->append(position, "\"", 1);
    position++;
#endif
    /* here the '\0' character is copied from the option string */
    buf->append(position, option, option_len);

    return buf->is_error();
  }
  return 1;
}


/*
  Get compiled-in value of default_option

  SYNOPSYS
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
  char verbose_option[]= " --no-defaults --verbose --help";

  /* reserve space fot the path + option + final '\0' */
  Buffer cmd(mysqld_path_len + sizeof(verbose_option));

  if (create_mysqld_command(&cmd, mysqld_path, mysqld_path_len,
                            verbose_option, sizeof(verbose_option)))
    goto err;

  /* +2 eats first "--" from the option string (E.g. "--datadir") */
  rc= parse_output_and_get_value(cmd.buffer, option_name + 2,
                                 result, result_len, GET_VALUE);
err:
  return rc;
}


/*
  Fill mysqld_version option (used at initialization stage)

  SYNOPSYS
    fill_instance_version()

  DESCRIPTION

  Get mysqld version string from "mysqld --version" output.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::fill_instance_version()
{
  enum { MAX_VERSION_STRING_LENGTH= 160 };
  char result[MAX_VERSION_STRING_LENGTH];
  char version_option[]= " --no-defaults --version";
  int rc= 1;
  Buffer cmd(mysqld_path_len + sizeof(version_option));

  if (create_mysqld_command(&cmd, mysqld_path, mysqld_path_len,
                            version_option, sizeof(version_option)))
    goto err;

  bzero(result, MAX_VERSION_STRING_LENGTH);

  rc= parse_output_and_get_value(cmd.buffer, mysqld_path,
                                 result, MAX_VERSION_STRING_LENGTH,
                                 GET_LINE);

  if (*result != '\0')
  {
    /* chop the newline from the end of the version string */
    result[strlen(result) - NEWLINE_LEN]= '\0';
    mysqld_version= strdup_root(&alloc, result);
  }
err:
  return rc;
}


/*
  Fill various log options

  SYNOPSYS
    fill_log_options()

  DESCRIPTION

  Compute paths to enabled log files. If the path is not specified in the
  instance explicitly (I.e. log=/home/user/mysql.log), we try to guess the
  file name and placement.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::fill_log_options()
{
  Buffer buff;
  uint position= 0;
  char **tmp_argv= argv;
  enum { MAX_LOG_OPTION_LENGTH= 256 };
  char datadir[MAX_LOG_OPTION_LENGTH];
  char hostname[MAX_LOG_OPTION_LENGTH];
  uint hostname_length;
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
      goto err;
  }
  else
  {
    /* below is safe, as --datadir always has a value */
    strmake(datadir,
            strchr(mysqld_datadir, '=') + 1, MAX_LOG_OPTION_LENGTH - 1);
  }

  if (gethostname(hostname,sizeof(hostname)-1) < 0)
    strmov(hostname, "mysql");

  hostname[MAX_LOG_OPTION_LENGTH - 1]= 0; /* Safety */
  hostname_length= strlen(hostname);


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
            goto err;

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
            goto err;
        }
      }
    }
  }

  return 0;
err:
  return 1;
}


/*
  Get the full pid file name with path

  SYNOPSYS
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
  const char *pid_file= mysqld_pid_file;
  char datadir[MAX_PATH_LEN];

  if (mysqld_datadir == NULL)
  {
    /* we might get an error here if we have wrong path to the mysqld binary */
    if (get_default_option(datadir, sizeof(datadir), "--datadir"))
      return 1;
  }
  else
    strxnmov(datadir, MAX_PATH_LEN - 1, strchr(mysqld_datadir, '=') + 1,
             "/", NullS);

  DBUG_ASSERT(mysqld_pid_file);
  pid_file= strchr(pid_file, '=') + 1;

  /* get the full path to the pidfile */
  my_load_path(result, pid_file, datadir);
  return 0;
}


int Instance_options::unlink_pidfile()
{
  return unlink(pid_file_with_path);
}


pid_t Instance_options::get_pid()
{
  FILE *pid_file_stream;

  /* get the pid */
  if ((pid_file_stream= my_fopen(pid_file_with_path,
                                O_RDONLY | O_BINARY, MYF(0))) != NULL)
  {
    pid_t pid;

    fscanf(pid_file_stream, "%i", &pid);
    my_fclose(pid_file_stream, MYF(0));
    return pid;
  }
  return 0;
}


int Instance_options::complete_initialization(const char *default_path,
                                              uint instance_type)
{
  const char *tmp;

  if (!mysqld_path && !(mysqld_path= strdup_root(&alloc, default_path)))
    goto err;

  mysqld_path_len= strlen(mysqld_path);

  if (mysqld_port)
    mysqld_port_val= atoi(strchr(mysqld_port, '=') + 1);

  if (shutdown_delay)
    shutdown_delay_val= atoi(shutdown_delay);

  if (!(tmp= strdup_root(&alloc, "--no-defaults")))
    goto err;

  if (!(mysqld_pid_file))
  {
    char pidfilename[MAX_PATH_LEN];
    char hostname[MAX_PATH_LEN];

    /*
      If we created only one istance [mysqld], because no config. files were
      found, we would like to model mysqld pid file values.
    */
    if (!gethostname(hostname, sizeof(hostname) - 1))
    {
      if (instance_type & DEFAULT_SINGLE_INSTANCE)
        strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", hostname,
                 ".pid", NullS);
      else
        strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", instance_name,
                 "-", hostname, ".pid", NullS);
    }
    else
    {
      if (instance_type & DEFAULT_SINGLE_INSTANCE)
        strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", "mysql",
                 ".pid", NullS);
      else
        strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", instance_name,
                 ".pid", NullS);
    }

    add_option(pidfilename);
  }

  if (get_pid_filename(pid_file_with_path))
    goto err;

  /* we need to reserve space for the final zero + possible default options */
  if (!(argv= (char**)
        alloc_root(&alloc, (options_array.elements + 1
                            + MAX_NUMBER_OF_DEFAULT_OPTIONS) * sizeof(char*))))
    goto err;

  /* the path must be first in the argv */
  if (add_to_argv(mysqld_path))
    goto err;

  if (add_to_argv(tmp))
    goto err;

  memcpy((gptr) (argv + filled_default_options), options_array.buffer,
         options_array.elements*sizeof(char*));
  argv[filled_default_options + options_array.elements]= 0;

  if (fill_log_options() || fill_instance_version())
    goto err;

  return 0;

err:
  return 1;
}


/*
  Assigns given value to the appropriate option from the class.

  SYNOPSYS
    add_option()
    option            string with the option prefixed by --

  DESCRIPTION

    The method is called from the option handling routine.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::add_option(const char* option)
{
  char *tmp;
  enum { SAVE_VALUE= 1, SAVE_WHOLE, SAVE_WHOLE_AND_ADD };
  struct selected_options_st
  {
    const char *name;
    uint length;
    const char **value;
    uint type;
  } options[]=
  {
    {"--socket=", 9, &mysqld_socket, SAVE_WHOLE_AND_ADD},
    {"--port=", 7, &mysqld_port, SAVE_WHOLE_AND_ADD},
    {"--datadir=", 10, &mysqld_datadir, SAVE_WHOLE_AND_ADD},
    {"--bind-address=", 15, &mysqld_bind_address, SAVE_WHOLE_AND_ADD},
    {"--pid-file=", 11, &mysqld_pid_file, SAVE_WHOLE_AND_ADD},
    {"--mysqld-path=", 14, &mysqld_path, SAVE_VALUE},
    {"--nonguarded", 9, &nonguarded, SAVE_WHOLE},
    {"--shutdown_delay", 9, &shutdown_delay, SAVE_VALUE},
    {NULL, 0, NULL, 0}
  };
  struct selected_options_st *selected_options;

  if (!(tmp= strdup_root(&alloc, option)))
    goto err;

   for (selected_options= options; selected_options->name; selected_options++)
   {
     if (strncmp(tmp, selected_options->name, selected_options->length) == 0)
       switch (selected_options->type) {
       case SAVE_WHOLE_AND_ADD:
         *(selected_options->value)= tmp;
         insert_dynamic(&options_array,(gptr) &tmp);
         return 0;
       case SAVE_VALUE:
         *(selected_options->value)= strchr(tmp, '=') + 1;
         return 0;
       case SAVE_WHOLE:
         *(selected_options->value)= tmp;
         return 0;
       default:
         break;
       }
   }

  /* if we haven't returned earlier we should just save the option */
  insert_dynamic(&options_array,(gptr) &tmp);

  return 0;

err:
  return 1;
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
  printf("printing out an instance %s argv:\n", instance_name);
  for (i=0; argv[i] != NULL; i++)
    printf("argv: %s\n", argv[i]);
}


/*
  We execute this function to initialize some options.
  Return value: 0 - ok. 1 - unable to allocate memory.
*/

int Instance_options::init(const char *instance_name_arg)
{
  instance_name_len= strlen(instance_name_arg);

  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);

  if (my_init_dynamic_array(&options_array, sizeof(char*), 0, 32))
    goto err;

  if (!(instance_name= strmake_root(&alloc, (char*) instance_name_arg,
                                    instance_name_len)))
    goto err;

  return 0;

err:
  return 1;
}


Instance_options::~Instance_options()
{
  free_root(&alloc, MYF(0));
  delete_dynamic(&options_array);
}

